#pragma once

#define BOARD_MAIN_CLOCK (48000000U)
#define BOARD_AHB_CLOCK (48000000U)
#define BOARD_UART_CLOCK (12000000U)

enum board_led { LED_DALI, SERIAL_RX, SERIAL_TX };

void board_init(void);
void board_set_led(enum board_led id);
void board_reset_led(enum board_led id);
void board_flash_rx(void);
void board_flash_tx(void);
void board_error(void);
bool core_isr_active(void);
