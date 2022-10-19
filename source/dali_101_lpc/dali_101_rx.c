#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"

#include "log/log.h"
#include "board/board.h"
#include "dali_101.h"

#define MAX_DATA_LENGTH (32U)
#define COUNT_ARRAY_SIZE (2U+MAX_DATA_LENGTH*2U+1U) // start bit, 32 data bits, 1 stop bit
#define DALI_RX_TASK_STACKSIZE (configMINIMAL_STACK_SIZE)
#define DALI_RX_PRIORITY (4U)

#define NOTIFY_CAPTURE (0x01)
#define NOTIFY_MATCH   (0x02)

enum rx_status {
    IDLE = 0,
    START_BIT_START,
    START_BIT_INSIDE,
    DATA_BIT_START,
    DATA_BIT_INSIDE,
    WAIT,
    LOW
};

// see IEC 62386-101-2018 Table 18, Table 19 - Transmitter bit timing
static const struct _rx_timing {
    uint32_t min_half_bit_begin_us;
    uint32_t max_half_bit_begin_us;
    uint32_t min_half_bit_inside_us;
    uint32_t max_half_bit_inside_us;
    uint32_t min_full_bit_inside_us;
    uint32_t max_full_bit_inside_us;
    uint32_t min_stop_condition_us;
} rx_timing = {
    .min_half_bit_begin_us = 333,   // Table 18
    .max_half_bit_begin_us = 500,
    .min_half_bit_inside_us = 333,  // Table 19
    .max_half_bit_inside_us = 500,
    .min_full_bit_inside_us = 666,
    .max_full_bit_inside_us = 1000,
    .min_stop_condition_us = 2400,
};

// module variables
struct _rx {
    uint32_t last_edge_count;
    uint32_t edge_count;
    enum rx_status status;
    struct dali_rx_frame frame;
    bool last_data_bit;
    void (*process_finished_frame)(struct dali_rx_frame);
    TaskHandle_t task_handle;
    StaticTask_t task_buffer;
    StackType_t task_stack[DALI_RX_TASK_STACKSIZE];
} rx = {0};

static void irq_capture_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;

    rx.edge_count = board_dali_rx_get_count();
    board_dali_rx_set_match(rx.edge_count + rx_timing.min_stop_condition_us);
    board_dali_rx_match_irq(true);
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_CAPTURE, eSetBits, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static void irq_match_callback(void)
{
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskNotifyFromISR(rx.task_handle, NOTIFY_MATCH, eSetBits, &higher_priority_woken);
    board_dali_rx_match_irq(false);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static bool is_valid_begin_bit_timing(const uint32_t time_difference_us)
{
    if ((time_difference_us > rx_timing.max_half_bit_begin_us) ||
        (time_difference_us < rx_timing.min_half_bit_begin_us)) return false;
    return true;

}

static void generate_error_frame(enum dali_errors code, uint32_t timer)
{
    if (rx.frame.error_code == DALI_OK) {
        rx.frame.error_code = code;
        rx.frame.error_timer_usec = timer;
    }
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
        generate_error_frame(DALI_ERROR_RECEIVE_START_TIMING, time_difference_us);
    }
    else {
        generate_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, time_difference_us);
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
    generate_error_frame(DALI_ERROR_RECEIVE_DATA_TIMING, time_difference_us);
    return DATA_BIT_START;
}

static void finish_frame(void)
{
    board_dali_rx_match_irq(false);
    LOG_PRINTF(LOG_LOW, "length: %d data: 0x%08x", rx.frame.length, rx.frame.data);
#ifndef NDEBUG
    switch (rx.frame.error_code) {
        case DALI_OK:
            break;
        case DALI_ERROR_RECEIVE_START_TIMING:
            LOG_PRINTF(LOG_LOW, "frame with RECEIVE_START_TIMING error - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_RECEIVE_DATA_TIMING:
            LOG_PRINTF(LOG_LOW, "frame with RECEIVE_DATA_TIMING error - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_COLLISION_LOOPBACK_TIME:
            LOG_PRINTF(LOG_LOW, "frame with COLLISION_LOOPBACK time - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_COLLISION_NO_CHANGE:
            LOG_PRINTF(LOG_LOW, "frame with COLLISION_NO_CHANGE time - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_COLLISION_WRONG_STATE:
            LOG_PRINTF(LOG_LOW, "frame with COLLISION_WRONG_STATE at - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_SETTLING_TIME_VIOLATION:
            LOG_PRINTF(LOG_LOW, "frame with SETTLING_TIME_VIOLATION at - %d us", rx.frame.error_timer_usec);
            break;
        case DALI_ERROR_SYSTEM_FAILURE:
            LOG_PRINTF(LOG_LOW, "system failure detected");
            break;
        case DALI_ERROR_SYSTEM_RECOVER:
            LOG_PRINTF(LOG_LOW, "system recovered from failure");
            break;
        default:
            LOG_PRINTF(LOG_LOW, "unexpected error code: %d", rx.frame.error_code);
            break;
    }
#endif
    if (rx.process_finished_frame != 0) {
        rx.process_finished_frame(rx.frame);
    }
}

static void rx_reset(void)
{
    rx.status = IDLE;
    rx.frame = (struct dali_rx_frame) {0};
}

static void process_capture_notification(void)
{
    switch (rx.status) {
        case IDLE:
            if (board_dali_rx_pin()) {
                rx.status = START_BIT_START;
                rx.last_data_bit = true;
                rx.frame.timestamp = xTaskGetTickCount();
            }
            break;
        case START_BIT_START:
            check_start_timing();
            rx.status = START_BIT_INSIDE;
            break;
        case START_BIT_INSIDE:
            rx.status = check_inside_timing();
            break;
        case DATA_BIT_START:
            check_start_timing();
            rx.status = DATA_BIT_INSIDE;
            break;
        case DATA_BIT_INSIDE:
            rx.status = check_inside_timing();
            break;
        default:
            LOG_ASSERT(0);
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
            finish_frame();
            rx_reset();
            break;
        default:
            LOG_ASSERT(false);
            break;
    }
}

__attribute__((noreturn)) static void rx_task(__attribute__((unused)) void *dummy)
{
    LOG_THIS_INVOCATION(LOG_FORCE);
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

void dali_rx_set_callback(void (*callback)(struct dali_rx_frame))
{
    rx.process_finished_frame = callback;
}

void dali_rx_init(void) {
    LOG_THIS_INVOCATION(LOG_FORCE);
    LOG_TEST(rx.task_handle = xTaskCreateStatic(rx_task, "DALI RX", DALI_RX_TASK_STACKSIZE, 
        NULL, DALI_RX_PRIORITY, rx.task_stack, &rx.task_buffer));
    
    board_dali_rx_set_capture_callback(irq_capture_callback);
    board_dali_rx_set_match_callback(irq_match_callback);
    board_dali_rx_timer_setup();
}