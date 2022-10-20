#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"   // TickType_t in dali_101.h

#include "log/log.h"
#include "board/board.h"
#include "dali_101.h"

#define MAX_DATA_LENGTH  (32U)
#define COUNT_ARRAY_SIZE (2U+MAX_DATA_LENGTH*2U+1U) // start bit, 32 data bits, 1 stop bit

// see IEC 62386-101-2018 Table 16 - Transmitter bit timing
static const struct _dali_timing {
    uint32_t half_bit_us;
    uint32_t full_bit_us;
    uint32_t stop_condition_us;
} dali_timing = {
    .half_bit_us = 417,
    .full_bit_us = 833,
    .stop_condition_us = 2450
};

struct _dali_tx {
    uint32_t count_now;
    uint32_t count[COUNT_ARRAY_SIZE];
    int_fast8_t index_next;
    int_fast8_t index_max;
    bool state_now;
} dali_tx; // TODO rename to tx

static void dali_reset_counts(void)
{
    dali_tx.count_now = 0;
    dali_tx.index_next = 0;
    dali_tx.index_max = 0;
    dali_tx.state_now = true;
}

static enum dali_tx_return dali_add_bit(bool value)
{
    if (dali_tx.state_now == value) {
        if (dali_tx.index_max > (COUNT_ARRAY_SIZE-2)) {
            return -RETURN_CAN_NOT_CONVERT;
        }
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
    }
    else {
        if (dali_tx.index_max > (COUNT_ARRAY_SIZE-1)) {
            return -RETURN_CAN_NOT_CONVERT;
        }
        dali_tx.index_max--;
        dali_tx.count_now = dali_tx.count[dali_tx.index_max -1] + dali_timing.full_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
    }
    dali_tx.state_now = value;
    return RETURN_OK;
}

static enum dali_tx_return dali_add_stop_condition(void)
{
    if (dali_tx.state_now) {
        if (!dali_tx.index_max) {
            return -RETURN_CAN_NOT_CONVERT;
        }
        dali_tx.index_max--;
        dali_tx.count_now = dali_tx.count[dali_tx.index_max-1] + dali_timing.stop_condition_us;
    } 
    else {
        if (dali_tx.index_max > COUNT_ARRAY_SIZE) {
            return -RETURN_CAN_NOT_CONVERT;
        }
        dali_tx.count_now += dali_timing.stop_condition_us;
        dali_tx.count[dali_tx.index_max] = dali_tx.count_now;
    }
    return RETURN_OK;
}

static enum dali_tx_return dali_calculate_counts(const struct dali_tx_frame frame)
{
    LOG_THIS_INVOCATION(LOG_LOW);

    if (frame.length > MAX_DATA_LENGTH) {
        return -RETURN_BAD_ARGUMENT;
    }

    dali_reset_counts();
    enum dali_tx_return rc = dali_add_bit(true);
    if (rc != RETURN_OK) {
        return rc;
    }
    for (int_fast8_t i=(frame.length-1); i>=0; i--) {
        rc = dali_add_bit(frame.data & (1 << i));
        if (rc != RETURN_OK) {
            return rc;
        }
    }
    return dali_add_stop_condition();
}

void dali_tx_irq_callback(void)
{
    if (dali_tx.index_next < dali_tx.index_max) {
        board_dali_tx_timer_next(dali_tx.count[dali_tx.index_next++], NOTHING);
        return;
    } 
    if (dali_tx.index_next == dali_tx.index_max) {
        board_dali_tx_timer_next(dali_tx.count[dali_tx.index_next++], DISABLE_TOGGLE);
        return;
    }
    board_dali_tx_timer_stop();
}

enum dali_tx_return dali_101_send (const struct dali_tx_frame frame)
{
    LOG_PRINTF(LOG_LOW, "dali_transmit (0x%08x)", frame.data);
    enum dali_tx_return rc = dali_calculate_counts(frame);
    LOG_PRINTF(LOG_LOW, "  count entries used: %d dec", (dali_tx.index_max+1));
    if (rc != RETURN_OK) {
        LOG_PRINTF(LOG_LOW, "  conversion failed: %d", rc);
        return rc;
    }
    board_dali_tx_timer_setup(dali_tx.count[dali_tx.index_next++]);
    return rc;
}
