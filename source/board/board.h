#pragma once

#define DALI_RX_LOW true
#define DALI_RX_HIGH false
#define DALI_TX_IDLE false
#define DALI_TX_ACTIVE true

#define BOARD_MAIN_CLOCK (48000000U)
#define BOARD_AHB_CLOCK (48000000U)
#define BOARD_UART_CLOCK (12000000U)

enum board_led { LED_DALI, SERIAL_RX, SERIAL_TX };
enum board_toggle {NOTHING, DISABLE_TOGGLE};

bool board_get_dali_rx(void);
void board_set_dali_tx(bool state);

void board_init(void);
void board_set_led(enum board_led id);
void board_reset_led(enum board_led id);
void board_flash_rx(void);
void board_flash_tx(void);
void board_error(void);
bool core_isr_active(void);

void board_dali_tx_set(bool state);
void board_dali_tx_timer_stop(void);
void board_dali_tx_timer_next(uint32_t count, enum board_toggle toggle);
void board_dali_tx_timer_setup(uint32_t count);
void board_dali_tx_set_callback(void (*isr)(void));