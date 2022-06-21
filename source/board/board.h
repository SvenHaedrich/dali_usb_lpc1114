#pragma once

#define DALI_RX_LOW true
#define DALI_RX_HIGH false
#define DALI_TX_LOW true
#define DALI_TX_HIGH false

#define BOARD_MAIN_CLOCK 48000000
#define BOARD_UART_CLOCK 12000000

enum board_led { LED_DALI, SERIAL_RX, SERIAL_TX };

bool board_get_dali_rx(void);
void board_set_dali_tx(bool state);

void board_init(void);
void board_set_led(enum board_led id);
void board_reset_led(enum board_led id);
void board_flash_rx(void);
void board_flash_tx(void);
void board_error(void);
bool core_isr_active(void);
