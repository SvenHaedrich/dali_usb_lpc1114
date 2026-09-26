// clang-format off
#include <errno.h>             // for EINVAL, ENOSPC
#include <stdbool.h>           // for true, false, bool
#include <stdint.h>            // for uint32_t, int_fast8_t, uint8_t, uint_fast8_t
#include "board/dali.h"        // for board_dali_tx_set, board_dali_tx_timer_next
#include "dali_101.h"          // for dali_tx_frame, DALI_MAX_DATA_LENGTH, DALI_ER...
#include "dali_101_private.h"  // for rx_schedule_transmission, rx_schedule_query
// clang-format on

#define COUNT_ARRAY_SIZE (2U + DALI_MAX_DATA_LENGTH * 2U + 1U) // start bit, 32 data bits, 1 stop bit
#define EXTEND_CORRUPT_PHASE 2

// every second phase has the rise and fall time subtracted from it
#define DALI_TX_COMPENSATION_US (DALI_TX_RISE_US + DALI_TX_FALL_US)
// a phase has to outlast the compensation, otherwise the subtraction wraps, and a phase
// of exactly the compensation leaves a match count that does not advance
#define DALI_TX_PERIOD_MIN_US (DALI_TX_COMPENSATION_US + 1U)

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
    bool sequence; // a sequence is defined and not executed yet
} tx;

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
    tx.sequence = false;
}

static int add_signal_phase(uint32_t duration_us, bool change_last_phase)
{
    if (tx.index_max >= COUNT_ARRAY_SIZE) {
        return -ENOSPC;
    }
    if (duration_us < DALI_TX_PERIOD_MIN_US) {
        return -EINVAL;
    }
    if (change_last_phase && tx.index_max == 0) {
        return -EINVAL;
    }
    // the parity of the phase decides the sign of the compensation, and it is taken
    // before change_last_phase rewinds the index
    uint32_t count_now;
    if (tx.index_max & 1) {
        if (duration_us > (UINT32_MAX - DALI_TX_COMPENSATION_US)) {
            return -EINVAL;
        }
        count_now = duration_us + DALI_TX_COMPENSATION_US;
    } else {
        count_now = duration_us - DALI_TX_COMPENSATION_US;
    }
    const uint_fast8_t index = change_last_phase ? (tx.index_max - 1U) : tx.index_max;
    const uint32_t previous = index ? tx.count[index - 1U] : 0U;
    // the counts are absolute and the timer is not allowed to roll over, so the whole
    // sequence has to fit into the counter
    if (count_now > (UINT32_MAX - previous)) {
        return -EINVAL;
    }
    tx.count[index] = count_now + previous;
    tx.index_max = index + 1U;
    return 0;
}

static int add_bit(bool value)
{
    int rc;
    if (tx.state_now == value) {
        rc = add_signal_phase(dali_timing.half_bit_us, false);
        if (!rc) {
            rc = add_signal_phase(dali_timing.half_bit_us, false);
        }
    } else {
        rc = add_signal_phase(dali_timing.full_bit_us, true);
        if (!rc) {
            rc = add_signal_phase(dali_timing.half_bit_us, false);
        }
    }
    if (rc) {
        return rc;
    }
    tx.state_now = value;
    return 0;
}

static int add_stop_condition(void)
{
    int rc;
    if (tx.state_now) {
        rc = add_signal_phase(dali_timing.stop_condition_us, true);
    } else {
        rc = add_signal_phase(dali_timing.stop_condition_us, false);
    }
    if (rc) {
        return rc;
    }
    tx.index_max--;
    return 0;
}

static int calculate_counts(const struct dali_tx_frame frame)
{
    if (frame.length > DALI_MAX_DATA_LENGTH) {
        return -EINVAL;
    }

    int rc = add_bit(true);
    if (rc) {
        return rc;
    }

    if (frame.type == DALI_FRAME_CORRUPT) {
        for (int_fast8_t i = 0; i < 16; i++) {
            if (i == EXTEND_CORRUPT_PHASE) {
                rc = add_signal_phase(dali_timing.corrupt_bit_us, false);
            } else {
                rc = add_signal_phase(dali_timing.half_bit_us, false);
            }
            if (rc) {
                return rc;
            }
        }
    } else {
        for (int_fast8_t i = (frame.length - 1); i >= 0; i--) {
            rc = add_bit(frame.data & (1 << i));
            if (rc) {
                return rc;
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

int dali_101_send(const struct dali_tx_frame frame)
{
    if (frame.type == DALI_FRAME_NONE) {
        return -EINVAL;
    }
    tx_reset();
    const int rc = calculate_counts(frame);
    if (rc) {
        return rc;
    }
    if (frame.type == DALI_FRAME_QUERY_1 || frame.type == DALI_FRAME_QUERY_2 || frame.type == DALI_FRAME_QUERY_3 ||
        frame.type == DALI_FRAME_QUERY_4 || frame.type == DALI_FRAME_QUERY_5) {
        tx.is_query = true;
    }
    tx.repeat = frame.repeat;
    rx_schedule_transmission(frame.type);
    return 0;
}

void dali_101_sequence_start(void)
{
    tx_reset();
    tx.repeat = 0;
    tx.sequence = true;
}

int dali_101_sequence_next(uint32_t period_us)
{
    if (!tx.sequence) {
        return -EINVAL;
    }
    const int rc = add_signal_phase(period_us, false);
    if (rc) {
        tx.sequence = false;
        return rc;
    }
    return 0;
}

int dali_101_sequence_execute(void)
{
    if (!tx.sequence || tx.index_next >= tx.index_max || tx.index_max == 0) {
        return -EINVAL;
    }
    tx.sequence = false;
    tx.index_max--;
    dali_tx_start_send();
    return 0;
}

void dali_tx_init(void)
{
    board_dali_tx_set(DALI_TX_IDLE);
    tx_reset();
}
