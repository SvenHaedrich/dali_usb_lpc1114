#pragma once

#define DALI_TX_Pin GPIO_PIN_8
#define DALI_TX_GPIO_Port GPIOA
#define DALI_RX_Pin GPIO_PIN_14
#define DALI_RX_GPIO_Port GPIOB
#define DALI_RX_EXTI_IRQn EXTI4_15_IRQn

#define IRQ_PRIO_HIGH 1
#define IRQ_PRIO_ELEV 2
#define IRQ_PRIO_NORM 3

#define BOARD_MAIN_CLOCK (48000000U)
#define BOARD_AHB_CLOCK (48000000U)
#define BOARD_UART_CLOCK (12000000U)

void board_init(void);
bool core_isr_active(void);
