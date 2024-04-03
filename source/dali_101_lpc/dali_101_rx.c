#include <limits.h> // ULONG_MAX

#include "FreeRTOS.h" // receive task, queue
#include "task.h"
#include "queue.h"

#include "board/dali.h" // interface to hardware abstraction
#include "dali_101.h"   // self include for consistency

#define DALI_RX_TASK_STACKSIZE (2U * configMINIMAL_STACK_SIZE)
#define DALI_RX_PRIORITY (tskIDLE_PRIORITY + 3U)

#define NOTIFY_CAPTURE (0x01)
#define NOTIFY_MATCH (0x02)
#define NOTIFY_PRIORITY (0x04)
#define NOTIFY_QUERY (0x08)

#define QUEUE_SIZE (5U)

enum rx_status {
    IDLE = 0,
    START_BIT_START,
    START_BIT_INSIDE,
    DATA_BIT_START,
    DATA_BIT_INSIDE,
    ERROR_IN_FRAME,
    INTER_FRAME_IDLE,
    LOW,
    FAILURE
};

// see IEC 62386-101-2018 Table 18, Table 19 - Transmitter bit timing
// see IEC 62386-101-2018 Table 4 - Power interruption of bus power
// see IEC 62386-101-2022 Table 20 - Receiver settling time values
static const struct _rx_timing {
    uint32_t min_half_bit_begin_us;
    uint32_t max_half_bit_begin_us;
    uint32_t min_half_bit_inside_us;
    uint32_t max_half_bit_inside_us;
    uint32_t min_full_bit_inside_us;
    uint32_t max_full_bit_inside_us;
    uint32_t min_stop_condition_us;
    uint32_t min_failure_condition_us;
    uint32_t max_backward_settling_us;
    uint32_t max_receive_twice_delay_ms;
} rx_timing = {
    .min_half_bit_begin_us = (333 - DALI_SAFTEY_MARGIN_US), // Table 18
    .max_half_bit_begin_us = (500 + DALI_SAFTEY_MARGIN_US),
    .min_half_bit_inside_us = (333 - DALI_SAFTEY_MARGIN_US), // Table 19
    .max_half_bit_inside_us = (500 + DALI_SAFTEY_MARGIN_US),
    .min_full_bit_inside_us = (666 - DALI_SAFTEY_MARGIN_US),
    .max_full_bit_inside_us = (1000 + DALI_SAFTEY_MARGIN_US),
    .min_stop_condition_us = 2400,
    .min_failure_condition_us = 500000, // Table 4
    .max_backward_settling_us = 13400,  // Table 20
    .max_receive_twice_delay_ms = 95,   // Table 20
};

// module variables
struct _rx {
    uint32_t last_edge_count;
    uint32_t last_full_frame_count;
    uint32_t edge_count;
    uint32_t end_inter_frame_idle;
    enum rx_status status;
    struct dali_rx_frame frame;
    bool last_data_bit;
    bool transmission_is_waiting;
    enum dali_frame_type transmission_frame_type;
    TaskHandle_t task_handle;
    QueueHandle_t queue_handle;
} rx = { 0 };

// external references from tx module
extern void dali_tx_init(void);
extern void dali_tx_start_send(void);
extern uint32_t tx_get_settling_time(void);
extern bool dali_tx_repeat(void);
extern void tx_reset(void);

void dali_rx_irq_capture_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;

    rx.edge_count = board_dali_rx_get_capture();
    board_dali_rx_set_stopbit_match(rx.edge_count + rx_timing.min_stop_condition_us);
    board_dali_rx_stopbit_match_enable(true);
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_CAPTURE, eSetBits, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void dali_rx_irq_stopbit_match_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_MATCH, eSetBits, &higher_priority_woken);
    board_dali_rx_stopbit_match_enable(false);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void dali_rx_irq_period_match_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_PRIORITY, eSetBits, &higher_priority_woken);
    board_dali_rx_stopbit_match_enable(false);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void dali_rx_irq_query_match_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_QUERY, eSetBits, &higher_priority_woken);
    board_dali_rx_query_match_enable(false);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static uint32_t get_settling_time_us(enum dali_frame_type type)
{
    static const uint32_t settling_time_us[] = { 5500, 13500, 14900, 16300, 17900, 19500, 2450 };
    switch (type) {
    case DALI_FRAME_BACKWARD:
        return settling_time_us[0];
    case DALI_FRAME_FORWARD_1:
    case DALI_FRAME_QUERY_1:
        return settling_time_us[1];
    case DALI_FRAME_FORWARD_2:
    case DALI_FRAME_QUERY_2:
        return settling_time_us[2];
    case DALI_FRAME_FORWARD_3:
    case DALI_FRAME_QUERY_3:
        return settling_time_us[3];
    case DALI_FRAME_FORWARD_4:
    case DALI_FRAME_QUERY_4:
        return settling_time_us[4];
    case DALI_FRAME_FORWARD_5:
    case DALI_FRAME_QUERY_5:
        return settling_time_us[5];
    case DALI_FRAME_BACK_TO_BACK:
        return settling_time_us[6];
    default:
        return 0;
    }
}

static bool is_valid_begin_bit_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_half_bit_begin_us) ||
        (time_difference_us < rx_timing.min_half_bit_begin_us))
        return false;
    return true;
}

void queue_error_frame(enum dali_status code, uint8_t bit, uint32_t time_us)
{
    if (rx.status == ERROR_IN_FRAME) {
        return;
    }
    if (rx.status == IDLE) {
        rx.frame.timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
    }
    if (rx.status == LOW) {
        rx.frame.timestamp = pdTICKS_TO_MS(xTaskGetTickCount()) - (rx_timing.min_failure_condition_us / 1000);
    }
    rx.frame.status = code;
    rx.frame.length = 0;
    rx.frame.data = (time_us & 0xffffff) << 8 | bit;
    xQueueSendToBack(rx.queue_handle, &rx.frame, 0);
    rx.status = ERROR_IN_FRAME;
}

static uint32_t frame_start_count(enum dali_frame_type type)
{
    if (type == DALI_FRAME_BACKWARD) {
        return rx.last_full_frame_count + get_settling_time_us(type);
    } else {
        return rx.last_edge_count + get_settling_time_us(type);
    }
}

static void schedule_settling_timeout(enum dali_frame_type type)
{
    rx.end_inter_frame_idle = frame_start_count(type);
    const uint32_t timer_now = board_dali_rx_get_count();
    if ((rx.end_inter_frame_idle - timer_now) < 100)
        rx.end_inter_frame_idle = timer_now + 100;
    board_dali_rx_set_period_match(rx.end_inter_frame_idle);
    board_dali_rx_period_match_enable(true);
}

static void rx_reset(void)
{
    rx.status = IDLE;
    rx.frame = (struct dali_rx_frame){ 0 };
}

static void generate_timeout_frame(void)
{
    if (rx.status == IDLE || rx.status == INTER_FRAME_IDLE) {
        rx.frame.timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        rx.frame.status = DALI_TIMEOUT;
        rx.frame.length = 0;
        rx.frame.loopback = false;
        rx.frame.data = 0;
        xQueueSendToBack(rx.queue_handle, &rx.frame, 0);
        rx_reset();
    }
}

static bool is_valid_halfbit_inside_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_half_bit_inside_us) ||
        (time_difference_us < rx_timing.min_half_bit_inside_us))
        return false;
    return true;
}

static bool is_valid_fullbit_inside_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_full_bit_inside_us) ||
        (time_difference_us < rx_timing.min_full_bit_inside_us))
        return false;
    return true;
}

static uint32_t get_corrected_time_difference_us(bool invert)
{
    const uint32_t time_difference_us = rx.edge_count - rx.last_edge_count;
    if (rx.last_data_bit ^ invert) {
        return time_difference_us + (DALI_RX_FALL_US - DALI_RX_RISE_US);
    }
    return time_difference_us - (DALI_RX_FALL_US - DALI_RX_RISE_US);
}

static void check_start_timing(void)
{
    const uint32_t time_difference_us = get_corrected_time_difference_us(false);
    if (is_valid_begin_bit_timing(time_difference_us)) {
        if (rx.status == DATA_BIT_START) {
            rx.frame.data = (rx.frame.data << 1U) | rx.last_data_bit;
            rx.frame.length++;
        }
        return;
    }
    if (rx.status == START_BIT_START) {
        queue_error_frame(DALI_ERROR_RECEIVE_START_TIMING, rx.frame.length, time_difference_us);
    } else {
        queue_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, rx.frame.length, time_difference_us);
    }
}

static enum rx_status check_inside_timing(void)
{
    uint32_t time_difference_us = get_corrected_time_difference_us(true);
    if (is_valid_halfbit_inside_timing(time_difference_us)) {
        return DATA_BIT_START;
    }
    if (is_valid_fullbit_inside_timing(time_difference_us)) {
        rx.last_data_bit = !rx.last_data_bit;
        rx.frame.data = (rx.frame.data << 1U) | rx.last_data_bit;
        rx.frame.length++;
        return DATA_BIT_INSIDE;
    }
    queue_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, rx.frame.length, time_difference_us);
    return ERROR_IN_FRAME;
}

static bool is_frame_received_twice(void)
{
    static struct dali_rx_frame last_frame = { 0 };
    if (last_frame.length == rx.frame.length) {
        if (last_frame.data == rx.frame.data) {
            const uint32_t delay = rx.frame.timestamp - last_frame.timestamp;
            if (delay < rx_timing.max_receive_twice_delay_ms) {
                return true;
            }
        }
    }
    return false;
}

static void queue_frame(void)
{
    rx.last_full_frame_count = rx.last_edge_count;
    rx.frame.twice = is_frame_received_twice();
    const BaseType_t result = xQueueSendToBack(rx.queue_handle, &rx.frame, 0);
    if (result == errQUEUE_FULL) {
        configASSERT(false);
    }
    rx.frame = (struct dali_rx_frame){ 0 };
}

static void process_pending_frame(void)
{
    if (rx.transmission_is_waiting) {
        schedule_settling_timeout(rx.transmission_frame_type);
    } else {
        schedule_settling_timeout(DALI_FRAME_FORWARD_5);
    }
    rx.status = INTER_FRAME_IDLE;
}

void rx_schedule_transmission(enum dali_frame_type type)
{
    rx.transmission_frame_type = type;
    if (rx.status == IDLE) {
        dali_tx_start_send();
        rx.transmission_is_waiting = false;
        return;
    }
    if (rx.status == INTER_FRAME_IDLE) {
        const uint32_t timer_now = board_dali_rx_get_count();
        uint32_t scheduled_start = frame_start_count(type);
        if (timer_now >= scheduled_start) {
            dali_tx_start_send();
            return;
        }
        if (scheduled_start < rx.end_inter_frame_idle) {
            if ((scheduled_start - timer_now) < 100)
                scheduled_start = timer_now + 100;
            board_dali_rx_set_period_match(scheduled_start);
            board_dali_rx_period_match_enable(true);
        }
    }
    rx.transmission_is_waiting = true;
}

void rx_schedule_query(void)
{
    const uint32_t timer_now = board_dali_rx_get_count();
    const uint32_t query_count = timer_now + rx_timing.max_backward_settling_us;
    board_dali_rx_set_query_match(query_count);
    board_dali_rx_query_match_enable(true);
}

static void manage_tx(void)
{
    if (dali_101_tx_is_idle()) {
        return;
    }
    if (dali_tx_repeat()) {
        rx_schedule_transmission(rx.transmission_frame_type);
        return;
    }
}

static void set_new_status(enum rx_status new_state)
{
    if (rx.status != ERROR_IN_FRAME) {
        rx.status = new_state;
    }
}

static void process_capture_notification(void)
{
    switch (rx.status) {
    case INTER_FRAME_IDLE:
    case IDLE:
        if (board_dali_rx_pin() == DALI_RX_ACTIVE) {
            set_new_status(START_BIT_START);
            rx.last_data_bit = true;
            rx.frame.timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
            rx.frame.loopback = !dali_101_tx_is_idle();
            board_dali_rx_query_match_enable(false);
            board_dali_rx_period_match_enable(false);
        }
        break;
    case START_BIT_START:
        check_start_timing();
        set_new_status(START_BIT_INSIDE);
        break;
    case START_BIT_INSIDE:
        set_new_status(check_inside_timing());
        break;
    case DATA_BIT_START:
        check_start_timing();
        set_new_status(DATA_BIT_INSIDE);
        break;
    case DATA_BIT_INSIDE:
        set_new_status(check_inside_timing());
        break;
    case ERROR_IN_FRAME:
        break;
    case LOW:
        if (board_dali_rx_pin() == DALI_RX_IDLE) {
            enum dali_status code = DALI_ERROR_RECEIVE_START_TIMING;
            if (rx.frame.length > 1) {
                code = DALI_ERROR_RECEIVE_DATA_TIMING;
            }
            const uint32_t time_difference_us = get_corrected_time_difference_us(false);
            queue_error_frame(code, rx.frame.length, time_difference_us);
            manage_tx();
            rx_reset();
        }
        break;
    case FAILURE:
        if (board_dali_rx_pin() == DALI_RX_IDLE) {
            rx.frame.timestamp = pdTICKS_TO_MS(xTaskGetTickCount());
            const uint32_t time_difference_us = get_corrected_time_difference_us(false);
            queue_error_frame(DALI_SYSTEM_RECOVER, 0, time_difference_us);
            manage_tx();
            rx_reset();
        }
        break;
    default:
        configASSERT(false);
        break;
    }
    rx.last_edge_count = rx.edge_count;
}

static void process_match_notification(void)
{
    if (board_dali_rx_pin() == DALI_RX_IDLE) {
        switch (rx.status) {
        case IDLE:
        case INTER_FRAME_IDLE:
            return;
        case START_BIT_START:
        case START_BIT_INSIDE:
        case DATA_BIT_START:
        case DATA_BIT_INSIDE:
            manage_tx();
            queue_frame();
            process_pending_frame();
            return;
        case LOW:
        case FAILURE:
        case ERROR_IN_FRAME:
            manage_tx();
            rx_reset();
            process_pending_frame();
            return;
        default:
            configASSERT(false);
            return;
        }
    }
    switch (rx.status) {
    case LOW:
        queue_error_frame(DALI_SYSTEM_FAILURE, 0, 0);
        rx.status = FAILURE;
        return;
    case FAILURE:
        return;
    default:
        board_dali_rx_set_stopbit_match(rx.last_edge_count + rx_timing.min_failure_condition_us);
        board_dali_rx_stopbit_match_enable(true);
        rx.status = LOW;
    }
}

static void process_priority_timeout(void)
{
    rx.status = IDLE;
    if (rx.transmission_is_waiting) {
        dali_tx_start_send();
        rx.transmission_is_waiting = false;
    }
}

__attribute__((noreturn)) static void rx_task(__attribute__((unused)) void* dummy)
{
    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifications, portMAX_DELAY);
        if (result == pdPASS) {
            if (notifications & NOTIFY_CAPTURE) {
                process_capture_notification();
            }
            if (notifications & NOTIFY_MATCH) {
                process_match_notification();
            }
            if (notifications & NOTIFY_PRIORITY) {
                process_priority_timeout();
            }
            if (notifications & NOTIFY_QUERY) {
                generate_timeout_frame();
            }
        }
    }
}

bool dali_101_get(struct dali_rx_frame* frame, uint32_t wait_ms, bool forever)
{
    TickType_t wait_ticks = forever ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms);
    const BaseType_t rc = xQueueReceive(rx.queue_handle, frame, wait_ticks);
    return (rc == pdPASS);
}

static void dali_rx_init(void)
{
    static StaticTask_t task_buffer;
    static StackType_t task_stack[DALI_RX_TASK_STACKSIZE];
    rx.task_handle =
        xTaskCreateStatic(rx_task, "DALI RX", DALI_RX_TASK_STACKSIZE, NULL, DALI_RX_PRIORITY, task_stack, &task_buffer);

    static uint8_t queue_storage[QUEUE_SIZE * sizeof(struct dali_rx_frame)];
    static StaticQueue_t queue_buffer;
    rx.queue_handle = xQueueCreateStatic(QUEUE_SIZE, sizeof(struct dali_rx_frame), queue_storage, &queue_buffer);
    configASSERT(rx.queue_handle);

    board_dali_rx_timer_setup();

    if (board_dali_rx_pin() == DALI_RX_IDLE) {
        rx.status = IDLE;
    } else {
        rx.status = FAILURE;
    }
}

void dali_101_init(void)
{
    dali_tx_init();
    dali_rx_init();
}