#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h" // TickType_t in dali_101.h

#include "board/dali.h" // interface to hardware abstraction
#include "dali_101.h"   // self-include for consistency

#define COUNT_ARRAY_SIZE (2U + DALI_MAX_DATA_LENGTH * 2U + 1U) // start bit, 32 data bits, 1 stop bit

// see IEC 62386-101-2018 Table 16 - Transmitter bit timing
static const struct _dali_timing {
    uint32_t half_bit_us;
    uint32_t full_bit_us;
    uint32_t stop_condition_us;
    uint32_t settling_time_us[DALI_PRIORITY_END];
} dali_timing = {
    .half_bit_us = 417,
    .full_bit_us = 833,
    .stop_condition_us = 2450,
    .settling_time_us = { 5500, 13500, 14900, 16300, 17900, 19500, 13500 },
};

struct _tx {
    uint32_t count_now;
    uint32_t count[COUNT_ARRAY_SIZE];
    uint_fast8_t index_next;
    uint_fast8_t index_max;
    bool state_now;
    uint8_t repeat;
    uint32_t min_settling_time;
    bool is_query;
} tx;

extern void generate_error_frame(enum dali_status code, uint8_t bit, uint32_t time_us);
extern void rx_schedule_frame(bool is_backframe);
extern void rx_schedule_query(void);

void tx_reset(void)
{
    tx.count_now = 0;
    tx.index_next = 0;
    tx.index_max = 0;
    tx.state_now = true;
}

static bool add_bit(bool value)
{
    if (tx.state_now == value) {
        if (tx.index_max > (COUNT_ARRAY_SIZE - 2)) {
            generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
            return true;
        }
        tx.count_now += dali_timing.half_bit_us;
        tx.count[tx.index_max++] = tx.count_now;
        tx.count_now += dali_timing.half_bit_us;
        tx.count[tx.index_max++] = tx.count_now;
    } else {
        if (tx.index_max > (COUNT_ARRAY_SIZE - 2)) {
            generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
            return true;
        }
        tx.index_max--;
        tx.count_now = tx.count[tx.index_max - 1] + dali_timing.full_bit_us;
        tx.count[tx.index_max++] = tx.count_now;
        tx.count_now += dali_timing.half_bit_us;
        tx.count[tx.index_max++] = tx.count_now;
    }
    tx.state_now = value;
    return false;
}

static bool add_stop_condition(void)
{
    if (tx.state_now) {
        if (!tx.index_max) {
            generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
            return true;
        }
        tx.index_max--;
        tx.count_now = tx.count[tx.index_max - 1] + dali_timing.stop_condition_us;
    } else {
        if (tx.index_max > COUNT_ARRAY_SIZE) {
            generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
            return true;
        }
        tx.count_now += dali_timing.stop_condition_us;
        tx.count[tx.index_max] = tx.count_now;
    }
    return false;
}

static bool calculate_counts(const struct dali_tx_frame frame)
{
    if (frame.length > DALI_MAX_DATA_LENGTH) {
        generate_error_frame(DALI_ERROR_BAD_ARGUMENT, 0, 0);
        return true;
    }

    if (add_bit(true)) {
        return true;
    }
    for (int_fast8_t i = (frame.length - 1); i >= 0; i--) {
        if (add_bit(frame.data & (1 << i))) {
            return true;
        }
    }
    return add_stop_condition();
}

void dali_tx_irq_callback(void)
{
    if (tx.index_next < tx.index_max) {
        board_dali_tx_timer_next(tx.count[tx.index_next++], NOTHING);
        return;
    }
    if (tx.index_next == tx.index_max) {
        board_dali_tx_timer_next(tx.count[tx.index_next++], DISABLE_TOGGLE);
        return;
    }
    board_dali_tx_set(DALI_TX_IDLE);
    board_dali_tx_timer_stop();
    if (tx.is_query) {
        rx_schedule_query();
        tx.is_query = false;
    }
}

void dali_tx_start_send(void)
{
    if (tx.index_max) {
        tx.index_next = 1;
        board_dali_tx_timer_setup(tx.count[0]);
    }
}

bool dali_101_tx_is_idle(void)
{
    return (tx.index_next == 0);
}

bool dali_tx_repeat(void)
{
    if (tx.repeat) {
        tx.repeat--;
        return true;
    }
    return false;
}

uint32_t tx_get_settling_time(void)
{
    return tx.min_settling_time;
}

void dali_101_send(const struct dali_tx_frame frame)
{
    tx_reset();
    if (calculate_counts(frame)) {
        return;
    }
    tx.min_settling_time = dali_timing.settling_time_us[frame.priority];
    tx.repeat = frame.repeat;
    tx.is_query = frame.is_query;
    rx_schedule_frame(frame.priority == DALI_BACKWARD_FRAME);
    return;
}

void dali_101_sequence_start(void)
{
    tx_reset();
    tx.min_settling_time = 0;
    tx.repeat = 0;
}

void dali_101_sequence_next(uint32_t period_us)
{
    tx.count_now += period_us;
    tx.count[tx.index_max++] = tx.count_now;
    if (tx.index_max > COUNT_ARRAY_SIZE) {
        generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
    }
}

void dali_101_sequence_execute(void)
{
    if (tx.index_next >= tx.index_max || tx.index_max == 0) {
        generate_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
        return;
    }
    dali_tx_start_send();
}

void dali_tx_init(void)
{
    board_dali_tx_set(DALI_TX_IDLE);
    tx_reset();
}
