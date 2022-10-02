#include <stdint.h>
#include <stdbool.h>

#include "log/log.h"
#include "board/board.h"
#include "dali_101_tx.h"

#define MAX_COUNTS 66

const struct _dali_timing {
    uint32_t half_bit_us;
    uint32_t full_bit_us;
    uint32_t stop_condition_us;
} dali_timing = {
    .half_bit_us = 417,
    .full_bit_us = 833,
    .stop_condition_us = 2400
};

struct _dali_tx {
    uint32_t count_now;
    uint32_t count[MAX_COUNTS];
    int8_t index_next;
    int8_t index_max;
    bool state_now;
} dali_tx = { .state_now = DALI_TX_IDLE};

static void dali_reset_counts(void)
{
    dali_tx.count_now = 0;
    dali_tx.index_next = 0;
    dali_tx.index_max = 0;
    dali_tx.state_now = true;
}

static void dali_add_bit(bool value)
{
    if (dali_tx.state_now == value) {
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
    }
    else {
        dali_tx.index_max--;
        dali_tx.count_now += dali_timing.full_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
        dali_tx.count_now += dali_timing.half_bit_us;
        dali_tx.count[dali_tx.index_max++] = dali_tx.count_now;
    }
    dali_tx.state_now = value;
}

static void dali_add_stop_condition(void)
{
    dali_tx.count_now += dali_timing.stop_condition_us;
    dali_tx.count[dali_tx.index_max] = dali_tx.count_now;
}

static void dali_calculate_counts(const struct dali_tx_frame frame)
{
    LOG_THIS_INVOCATION(LOG_LOW);
    dali_reset_counts();
    dali_add_bit(true);
    for (uint_fast8_t i=frame.length; i>0; i--) {
        dali_add_bit(frame.data & (1 << i));
    }
    dali_add_stop_condition();
}

static void dali_set_next_count(void)
{
    if (dali_tx.index_next < dali_tx.index_max) {
        board_dali_tx_timer_next(dali_tx.count[dali_tx.index_next++]);
    } 
    else  {
        board_dali_tx_timer_stop();
    }
}

bool dali_is_tx_active(void)
{
    // TODO
    return false;
}

void dali_destroy (void)
{
    // TODO
}

void dali_transmit (const struct dali_tx_frame frame)
{
    LOG_PRINTF(LOG_LOW,"dali_transmit (0x%08x)");
    dali_calculate_counts(frame);
    board_dali_tx_timer_setup(dali_tx.count[dali_tx.index_next++]);
}

void dali_tx_init(void)
{
    LOG_THIS_INVOCATION(LOG_LOW);
    board_dali_tx_set(DALI_TX_IDLE);
    board_dali_tx_set_callback(dali_set_next_count);
}