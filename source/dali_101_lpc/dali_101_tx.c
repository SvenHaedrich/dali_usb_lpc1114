#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h" // TickType_t in dali_101.h

#include "board/dali.h" // interface to hardware abstraction
#include "dali_101.h"   // self-include for consistency

#define COUNT_ARRAY_SIZE (2U + DALI_MAX_DATA_LENGTH * 2U + 1U) // start bit, 32 data bits, 1 stop bit
#define EXTEND_CORRUPT_PHASE 2

// see IEC 62386-101-2018 Table 16 - Transmitter bit timing
// see IEC 62386-101-2022 9.6.2 - Backward frame
static const struct _dali_timing {
    uint32_t half_bit_us;
    uint32_t full_bit_us;
    uint32_t corrupt_bit_us;
    uint32_t stop_condition_us;
} dali_timing = {
    .half_bit_us = 417,
    .full_bit_us = 833,
    .corrupt_bit_us = 1500,
    .stop_condition_us = 2450,
};

struct _tx {
    uint32_t count[COUNT_ARRAY_SIZE];
    uint_fast8_t index_next;
    uint_fast8_t index_max;
    bool state_now;
    uint8_t repeat;
    bool is_query;
} tx;

extern void queue_error_frame(enum dali_status code, uint8_t bit, uint32_t time_us);
extern void rx_schedule_transmission(enum dali_tx_priority);
extern void rx_schedule_query(void);

void tx_reset(void)
{
    if (!dali_101_tx_is_idle()) {
        board_dali_tx_set(DALI_TX_IDLE);
        board_dali_tx_timer_stop();
    }
    tx.index_next = 0;
    tx.index_max = 0;
    tx.state_now = true;
    tx.count[0] = 0;
}

static bool add_signal_phase(uint32_t duration_us, bool change_last_phase)
{
    if (tx.index_max >= COUNT_ARRAY_SIZE) {
        queue_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
        return true;
    }
    uint32_t count_now;
    if (tx.index_max & 1) {
        count_now = duration_us + (DALI_TX_RISE_US + DALI_TX_FALL_US);
    } else {
        count_now = duration_us - (DALI_TX_RISE_US + DALI_TX_FALL_US);
    }
    if (change_last_phase) {
        tx.index_max--;
    }
    if (tx.index_max) {
        count_now += tx.count[tx.index_max - 1];
    }
    tx.count[tx.index_max++] = count_now;
    return false;
}

static bool add_bit(bool value)
{
    if (tx.state_now == value) {
        if (add_signal_phase(dali_timing.half_bit_us, false) || add_signal_phase(dali_timing.half_bit_us, false)) {
            return true;
        }

    } else {
        if (add_signal_phase(dali_timing.full_bit_us, true) || add_signal_phase(dali_timing.half_bit_us, false)) {
            return true;
        }
    }
    tx.state_now = value;
    return false;
}

static bool add_stop_condition(void)
{
    if (tx.state_now) {
        if (add_signal_phase(dali_timing.stop_condition_us, true)) {
            return true;
        }
        tx.index_max--;
    } else {
        if (add_signal_phase(dali_timing.stop_condition_us, false)) {
            return true;
        }
        tx.index_max--;
    }
    return false;
}

static bool calculate_counts(const struct dali_tx_frame frame)
{
    if (frame.length > DALI_MAX_DATA_LENGTH) {
        queue_error_frame(DALI_ERROR_BAD_ARGUMENT, 0, 0);
        return true;
    }

    if (add_bit(true)) {
        return true;
    }

    if (frame.is_corrupt) {
        for (int_fast8_t i = 0; i < 16; i++) {
            if (i == EXTEND_CORRUPT_PHASE) {
                if (add_signal_phase(dali_timing.corrupt_bit_us, false)) {
                    return true;
                }
            }
            else {
                if (add_signal_phase(dali_timing.half_bit_us, false)) {
                    return true;
                }
            }
        }
    }
    else {
        for (int_fast8_t i = (frame.length - 1); i >= 0; i--) {
            if (add_bit(frame.data & (1 << i))) {
                return true;
            }
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
    if (!tx.repeat)
        tx.index_next = 0;
}

void dali_tx_start_send(void)
{
    tx.index_next = 1;
    board_dali_tx_timer_setup(tx.count[0]);
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

void dali_101_send(const struct dali_tx_frame frame)
{
    tx_reset();
    if (calculate_counts(frame)) {
        return;
    }
    tx.repeat = frame.repeat;
    tx.is_query = frame.is_query;
    rx_schedule_transmission(frame.priority);
    return;
}

void dali_101_sequence_start(void)
{
    tx_reset();
    tx.repeat = 0;
}

void dali_101_sequence_next(uint32_t period_us)
{
    add_signal_phase(period_us, false);
}

void dali_101_sequence_execute(void)
{
    if (tx.index_next >= tx.index_max || tx.index_max == 0) {
        queue_error_frame(DALI_ERROR_CAN_NOT_PROCESS, 0, 0);
        return;
    }
    tx.index_max--;
    dali_tx_start_send();
}

void dali_tx_init(void)
{
    board_dali_tx_set(DALI_TX_IDLE);
    tx_reset();
}
