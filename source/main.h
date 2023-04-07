#pragma once

/* Private defines -----------------------------------------------------------*/
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