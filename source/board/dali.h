#pragma once
#include <stdint.h>
#include <stdbool.h>

#define DALI_RX_IDLE false
#define DALI_RX_ACTIVE true
#define DALI_TX_IDLE false
#define DALI_TX_ACTIVE true

#define DALI_TX_FALL_US (4)
#define DALI_TX_RISE_US (20)
#define DALI_RX_FALL_US (20)
#define DALI_RX_RISE_US (4)
#define DALI_SAFTEY_MARGIN_US (8)

enum board_toggle { NOTHING, DISABLE_TOGGLE };

void board_setup_dali_clock(void);

void board_dali_tx_set(bool state);
void board_dali_tx_timer_stop(void);
void board_dali_tx_timer_next(uint32_t count, enum board_toggle toggle);
void board_dali_tx_timer_setup(uint32_t count);

bool board_dali_rx_pin(void);
void board_dali_rx_timer_setup(void);
uint32_t board_dali_rx_get_capture(void);
uint32_t board_dali_rx_get_count(void);
void board_dali_rx_set_stopbit_match(uint32_t match);
void board_dali_rx_stopbit_match_enable(bool enable);
void board_dali_rx_set_period_match(uint32_t match);
void board_dali_rx_period_match_enable(bool enable);
void board_dali_rx_set_query_match(uint32_t match_count);
void board_dali_rx_query_match_enable(bool enable);
