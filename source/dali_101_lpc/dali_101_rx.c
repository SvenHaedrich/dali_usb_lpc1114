#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "log/log.h"
#include "board/board.h"
#include "dali_101.h"

#define MAX_DATA_LENGTH (32U)

#define DALI_RX_TASK_STACKSIZE (2*configMINIMAL_STACK_SIZE)
#define DALI_RX_PRIORITY (tskIDLE_PRIORITY +3U)

#define NOTIFY_CAPTURE (0x01)
#define NOTIFY_MATCH   (0x02)

#define QUEUE_SIZE (3U)

enum rx_status {
    IDLE = 0,
    START_BIT_START,
    START_BIT_INSIDE,
    DATA_BIT_START,
    DATA_BIT_INSIDE,
    ERROR_IN_FRAME,
    LOW,
    FAILURE
};

// see IEC 62386-101-2018 Table 18, Table 19 - Transmitter bit timing
// see IEC 62386-101-2018 Table 4 - Power interruption of bus power
static const struct _rx_timing {
    uint32_t min_half_bit_begin_us;
    uint32_t max_half_bit_begin_us;
    uint32_t min_half_bit_inside_us;
    uint32_t max_half_bit_inside_us;
    uint32_t min_full_bit_inside_us;
    uint32_t max_full_bit_inside_us;
    uint32_t min_stop_condition_us;
    uint32_t min_failure_condition_us;
} rx_timing = {
    .min_half_bit_begin_us = 333,   // Table 18
    .max_half_bit_begin_us = 500,
    .min_half_bit_inside_us = 333,  // Table 19
    .max_half_bit_inside_us = 500,
    .min_full_bit_inside_us = 666,
    .max_full_bit_inside_us = 1000,
    .min_stop_condition_us = 2400,
    .min_failure_condition_us = 500000, // Table 4
};

// module variables
struct _rx {
    uint32_t last_edge_count;
    uint32_t edge_count;
    enum rx_status status;
    struct dali_rx_frame frame;
    bool last_data_bit;
    TaskHandle_t task_handle;
    QueueHandle_t queue_handle;
} rx = {0};

// external references from tx module
extern void dali_tx_init(void);
extern void dali_tx_start_send(void);
extern bool dali_tx_is_idle(void);
extern uint32_t tx_get_settling_time(void);
extern bool dali_tx_repeat(void);
extern void tx_reset(void);

static void irq_capture_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;

    rx.edge_count = board_dali_rx_get_capture();
    board_dali_rx_set_stopbit_match(rx.edge_count + rx_timing.min_stop_condition_us);
    board_dali_rx_stopbit_match_enable(true);
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_CAPTURE, eSetBits, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static void irq_match_stopbit_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_MATCH, eSetBits, &higher_priority_woken);
    board_dali_rx_stopbit_match_enable(false);
    if (board_dali_rx_pin() == DALI_RX_IDLE) {
        rx.status = LOW;
    }
    portYIELD_FROM_ISR(higher_priority_woken);
}

static void irq_match_period_callback(void)
{
    dali_tx_start_send();
}

static bool is_valid_begin_bit_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_half_bit_begin_us) ||
        (time_difference_us < rx_timing.min_half_bit_begin_us)) return false;
    return true;
}

void generate_error_frame(enum dali_error code, uint8_t bit, uint32_t time_us)
{
    if (rx.status == ERROR_IN_FRAME) {
        return;
    }
    if (rx.status == IDLE || rx.status == LOW) {
        rx.frame.timestamp = xTaskGetTickCount();
    }
    rx.frame.is_status = true;
    rx.frame.length = code;
    rx.frame.data = (time_us & 0xffffff)<<8 | bit;
    xQueueSendToBack(rx.queue_handle, &rx.frame, 0);
    rx.status = ERROR_IN_FRAME;
}

static bool is_valid_halfbit_inside_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_half_bit_inside_us) ||
        (time_difference_us < rx_timing.min_half_bit_inside_us)) return false;
    return true;
}

static bool is_valid_fullbit_inside_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_full_bit_inside_us) ||
        (time_difference_us < rx_timing.min_full_bit_inside_us)) return false;
    return true;
}

static void check_start_timing(void)
{
    const uint32_t time_difference_us = rx.edge_count - rx.last_edge_count;
    if (is_valid_begin_bit_timing(time_difference_us)) {
        if (rx.status == DATA_BIT_START) {
            rx.frame.data = (rx.frame.data << 1U) | rx.last_data_bit;
            rx.frame.length++;
        }
        return;
    }
    if (rx.status == START_BIT_START) {
        generate_error_frame(DALI_ERROR_RECEIVE_START_TIMING, rx.frame.length, time_difference_us);
    }
    else {
        generate_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, rx.frame.length, time_difference_us);
    }
}

static enum rx_status check_inside_timing(void) 
{
    const uint32_t time_difference_us = rx.edge_count - rx.last_edge_count;
    if (is_valid_halfbit_inside_timing(time_difference_us)) {
        return DATA_BIT_START;
    } 
    if (is_valid_fullbit_inside_timing(time_difference_us)) {
        rx.last_data_bit = !rx.last_data_bit;
        rx.frame.data = (rx.frame.data << 1U) | rx.last_data_bit;
        rx.frame.length++;
        return DATA_BIT_INSIDE;
    }
    generate_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, rx.frame.length, time_difference_us);
    return ERROR_IN_FRAME;
}

static void finish_frame(void)
{
    LOG_PRINTF(LOG_LOW, "length: %d data: 0x%08x", rx.frame.length, rx.frame.data);
    const BaseType_t result = xQueueSendToBack(rx.queue_handle, &rx.frame, 0);
    if (result == errQUEUE_FULL) {
        LOG_ASSERT(false);
    }
}

void rx_schedule_frame(void)
{
    uint32_t min_settling_time = tx_get_settling_time();
    uint32_t min_start_count = min_settling_time + rx.last_edge_count;
    const uint32_t timer_now = board_dali_rx_get_count();
    if (timer_now >  min_start_count) {
        dali_tx_start_send();
        return;
    }
    if ((min_start_count - timer_now) < 100)
        min_start_count += 100;
    board_dali_rx_set_period_match(min_start_count);
    board_dali_rx_period_match_enable(true);
}

static void manage_tx(void)
{
    if (dali_tx_is_idle()) {
        return;
    }
    if (dali_tx_repeat()) {
        rx_schedule_frame();
        return;
    }
    tx_reset();
}

static void rx_reset(void)
{
    rx.status = IDLE;
    rx.frame = (struct dali_rx_frame) {0};
}


static void set_new_status (enum rx_status new_state)
{
    if (rx.status != ERROR_IN_FRAME) {
        rx.status = new_state;
    }
}

static void process_capture_notification(void)
{
    switch (rx.status) {
        case IDLE:
            if (board_dali_rx_pin()==DALI_RX_ACTIVE) {
                set_new_status(START_BIT_START);
                rx.last_data_bit = true;
                rx.frame.timestamp = xTaskGetTickCount();
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
                const enum dali_error code = ((rx.frame.length <=1) ? DALI_ERROR_RECEIVE_START_TIMING : DALI_ERROR_RECEIVE_DATA_TIMING); 
                const uint32_t time_difference_us = rx.edge_count - rx.last_edge_count;
                generate_error_frame(code, (rx.frame.length-1), time_difference_us);
            }
            break;
        case FAILURE:
            if (board_dali_rx_pin() == DALI_RX_IDLE) {
                const uint32_t time_difference_us = rx.edge_count - rx.last_edge_count;
                generate_error_frame(DALI_SYSTEM_RECOVER, (rx.frame.length-1), time_difference_us);
            }
            break;
            break;
        default:
            LOG_ASSERT(false);
            break;
    }
    rx.last_edge_count = rx.edge_count;
}

static void process_match_notification(void)
{
    switch (rx.status) {
        case IDLE:
            break;
        case START_BIT_START:
        case START_BIT_INSIDE:
        case DATA_BIT_START:
        case DATA_BIT_INSIDE:
            manage_tx();
            rx_reset();
            finish_frame();
            break;
        case ERROR_IN_FRAME:
            manage_tx();
            rx_reset();
            break;
        case LOW:
        case FAILURE:
            const uint32_t time_difference_us = board_dali_rx_get_count() - rx.last_edge_count;
            if (time_difference_us >= rx_timing.min_failure_condition_us) {
                generate_error_frame(DALI_SYSTEM_FAILURE, 0, time_difference_us);
                rx.status = FAILURE;
            }
            board_dali_rx_set_stopbit_match(rx.last_edge_count + rx_timing.min_failure_condition_us);
            board_dali_rx_stopbit_match_enable(true);
            break;
        default:
            LOG_ASSERT(false);
            break;
    }
}

__attribute__((noreturn)) static void rx_task(__attribute__((unused)) void *dummy)
{
    LOG_THIS_INVOCATION(LOG_TASK);
    rx_reset();

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
        }
    }
}

bool dali_101_get(struct dali_rx_frame* frame, TickType_t wait)
{
    const BaseType_t rc = xQueueReceive(rx.queue_handle, frame, wait);
    return (rc == pdPASS);
}

static void dali_rx_init(void) 
{
   static StaticTask_t task_buffer;
   static StackType_t task_stack[DALI_RX_TASK_STACKSIZE];
   LOG_TEST(rx.task_handle = xTaskCreateStatic(rx_task, "DALI RX", DALI_RX_TASK_STACKSIZE, 
        NULL, DALI_RX_PRIORITY, task_stack, &task_buffer));
    
    static uint8_t queue_storage [QUEUE_SIZE * sizeof(struct dali_rx_frame)];
    static StaticQueue_t queue_buffer;
    rx.queue_handle = xQueueCreateStatic(
        QUEUE_SIZE, sizeof(struct dali_rx_frame), queue_storage, &queue_buffer);
    LOG_ASSERT(rx.queue_handle);

    board_dali_rx_set_capture_callback(irq_capture_callback);
    board_dali_rx_set_stopbit_match_callback(irq_match_stopbit_callback);
    board_dali_rx_set_period_match_callback(irq_match_period_callback);
    board_dali_rx_timer_setup();

    if (board_dali_rx_pin()==DALI_RX_IDLE) {
        rx.status = IDLE;
    }
    else {
        rx.status = FAILURE;
    }
}

void dali_101_request_status_frame(void)
{
    LOG_THIS_INVOCATION(LOG_LOW);
    
    const struct dali_rx_frame status_frame = {
        .is_status = true,
        .timestamp = xTaskGetTickCount(),
        .length = (rx.status == ERROR_IN_FRAME) ? rx.frame.length : DALI_SYSTEM_IDLE,
        .data = 0
    };
    xQueueSendToBack(rx.queue_handle, &status_frame, 0);
}


void dali_101_init(void)
{
    LOG_THIS_INVOCATION(LOG_INIT);

    dali_tx_init();
    dali_rx_init();
}