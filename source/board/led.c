#include <stdbool.h> // board/dali.h

#include "FreeRTOS.h"
#include "timers.h" // timer stuff

#include "lpc11xx.h"   // registers
#include "bitfields.h" // registers
#include "dali.h"      // access to DALI line status
#include "led.h"

#define HEARTBEAT_PERIOD_MS (100U)
#define SLOW_HEARTBEAT_COUNTER (10U)
#define FLASH_PERIOD_MS (50U)

#define LED_DALI_PIN 5
#define LED_SERIAL_PIN 6
#define LED_HEARTBEAT_PIN 4

static TimerHandle_t led_handle[LED_MAX];
static uint8_t led_gpio_pin[] = { LED_HEARTBEAT_PIN, LED_DALI_PIN, LED_SERIAL_PIN };

static void reset(enum board_led id)
{
    if (id < LED_MAX) {
        LPC_GPIO2->DATA &= ~(1U << led_gpio_pin[id]);
    }
}

static void set(enum board_led id)
{
    if (id < LED_MAX) {
        LPC_GPIO2->DATA |= (1U << led_gpio_pin[id]);
    }
}

static void setup_pins(void)
{
    LPC_GPIO2->DIR |= (1U << LED_DALI_PIN) | (1U << LED_SERIAL_PIN) | (1U << LED_HEARTBEAT_PIN);
}

static void heartbeat(TimerHandle_t __attribute__((unused)) dummy)
{
    static uint32_t counter = 0;

    counter++;
    if (board_dali_rx_pin() == DALI_RX_IDLE) {
        if (counter & 1) {
            set(LED_HEARTBEAT);
        } else {
            reset(LED_HEARTBEAT);
        }
    } else {
        if (counter > SLOW_HEARTBEAT_COUNTER) {
            set(LED_HEARTBEAT);
            counter = 0;
        } else
            reset(LED_HEARTBEAT);
    }
}

void board_indicate_error(void)
{
    for (uint_fast8_t i = 0; i < LED_MAX; i++) {
        xTimerStop(led_handle[i], 0);
        set(i);
    }
}

static void led_1_timeout(__attribute__((unused)) TimerHandle_t dummy)
{
    reset(LED_SERIAL);
}

static void led_2_timeout(__attribute__((unused)) TimerHandle_t dummy)
{
    reset(LED_DALI);
}

void board_flash(enum board_led id)
{
    if (id == LED_SERIAL || id == LED_DALI) {
        set(id);
        xTimerStart(led_handle[id], 0);
    }
}

void board_led_init(void)
{
    setup_pins();

    static StaticTimer_t timer_buffer[LED_MAX];
    led_handle[LED_HEARTBEAT] =
        xTimerCreateStatic("heartbeat", HEARTBEAT_PERIOD_MS, pdTRUE, NULL, heartbeat, &timer_buffer[LED_HEARTBEAT]);
    led_handle[LED_SERIAL] =
        xTimerCreateStatic("led_serial", FLASH_PERIOD_MS, pdFALSE, NULL, led_1_timeout, &timer_buffer[LED_SERIAL]);
    led_handle[LED_DALI] =
        xTimerCreateStatic("led_dali", FLASH_PERIOD_MS, pdFALSE, NULL, led_2_timeout, &timer_buffer[LED_DALI]);

    configASSERT(led_handle[LED_HEARTBEAT]);
    configASSERT(led_handle[LED_SERIAL]);
    configASSERT(led_handle[LED_DALI]);
    configASSERT(xTimerStart(led_handle[LED_HEARTBEAT], 0) == pdPASS);

    for (uint_fast8_t i = 0; i < LED_MAX; i++) {
        reset(i);
    }
}