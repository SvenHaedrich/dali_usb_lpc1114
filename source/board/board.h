#pragma once

#define LED_SERIAL_RX GPIO_PIN_2
#define LED_SERIAL_TX GPIO_PIN_1
#define LED_DALI_STATE GPIO_PIN_0
#define LED_GPIO_PORT GPIOA

#define DALI_TX_Pin GPIO_PIN_8
#define DALI_TX_GPIO_Port GPIOA
#define DALI_RX_Pin GPIO_PIN_14
#define DALI_RX_GPIO_Port GPIOB
#define DALI_RX_EXTI_IRQn EXTI4_15_IRQn

#define MCU_XX_P9_UART1_TX_Pin GPIO_PIN_9
#define MCU_XX_P9_UART1_TX_GPIO_Port GPIOA
#define MCU_XX_PA10_UART1_RX_Pin GPIO_PIN_10
#define MCU_XX_PA10_UART1_RX_GPIO_Port GPIOA
#define PSU_ERROR_EXTI_IRQn EXTI4_15_IRQn

#define IRQ_PRIO_HIGH 0
#define IRQ_PRIO_ELEV 1
#define IRQ_PRIO_NORM 3

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
