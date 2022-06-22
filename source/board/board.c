#include <stdbool.h>

#include "FreeRTOS.h"
#include "timers.h"

#include "lpc11xx.h"
#include "bitfields.h"
#include "../main.h"
#include "../log/log.h"
#include "board.h"

#define HEARTBEAT_PERIOD_MS 100
#define SLOW_HEARTBEAT_COUNTER 10
#define FLASH_PERIOD_MS 200

static TimerHandle_t board_heartbeat_timer_id;
static TimerHandle_t board_rx_timer_id;
static TimerHandle_t board_tx_timer_id;

static StaticTimer_t board_heartbeat_buffer;
static StaticTimer_t board_rx_buffer;
static StaticTimer_t board_tx_buffer;

#include "board.h"
#include "string.h"

u_int32_t SystemCoreClock = 12000000;

bool core_isr_active(void)
{
    return SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;
}

static void board_setup_main_clock(void)
{
    // power up main oscillator
    LPC_SYSCON->PDRUNCFG &= ~PDRUNCFG_SYSOSC_PD;

    // wait 200 µs for main oscillator to stabilize
    for (uint32_t i = 0; i < 0x100U; i++)
        ;

    // set PLL input to main oscillator
    LPC_SYSCON->SYSPLLCLKSEL = 0x01U;

    // power down pll to change divider settings
    LPC_SYSCON->PDRUNCFG |= PDRUNCFG_SYSPLL_PD;

    // setup pll for main oscillator frequency (FCLKIN = 12MHz) * 4 = 48MHz
    // MSEL = 3 (this is pre-decremented), PSEL = 1 (for P = 2)
    // FCLKOUT = FCLKIN * (MSEL + 1) = 12MHz * 4 = 48MHz
    // FCCO = FCLKOUT * 2 * P = 48MHz * 2 * 2 = 192MHz (within FCCO range)
    LPC_SYSCON->SYSPLLCTRL = (0x03U) + (0x01U << 5U);

    // power up pll
    LPC_SYSCON->PDRUNCFG &= ~PDRUNCFG_SYSPLL_PD;

    // wair for pll lock
    while (!(LPC_SYSCON->SYSPLLSTAT & SYSPLLSTAT_LOCK))
        ;

    // set system clock divider to 1
    LPC_SYSCON->SYSAHBCLKDIV = 0x01U;

    // set flash access to 3 cloks
    LPC_FLASHCTRL->FLASHCFG = 0x03U;

    // set main clock source to pll
    LPC_SYSCON->MAINCLKSEL = 0x03U;
    LPC_SYSCON->MAINCLKUEN = 0x00U;
    LPC_SYSCON->MAINCLKUEN = 0x01U;
    SystemCoreClock = 48000000;
}

static void board_setup_uart_clock(void)
{
    // enable IOCON block
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_IOCON;

    // enable uart block
    // enable pins first!
    LPC_IOCON->PIO1_6 |= 0x01;
    LPC_IOCON->PIO1_7 |= 0x01;
    // set uart clock to 12 MHz
    LPC_SYSCON->UARTCLKDIV = 0x04U;
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_UART;
}

static void board_setup_clocking(void)
{
    board_setup_main_clock();
    board_setup_uart_clock();
}

static void board_setup_pin_mux(void)
{}

void board_set_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO3->DATA &= ~(1 << 5);
    }
}

void board_reset_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO3->DATA |= (1 << 5);
    }
}

bool board_get_dali_rx(void)
{
    return false;
}

void board_set_dali_tx(bool state)
{}

static void board_heartbeat(TimerHandle_t NOUSE(dummy))
{
    static uint32_t counter = 0;

    counter++;
    if (board_get_dali_rx()) {
        if (counter & 1) {
            board_set_led(LED_DALI);
        } else {
            board_reset_led(LED_DALI);
        }
    } else {
        if (counter > SLOW_HEARTBEAT_COUNTER) {
            board_set_led(LED_DALI);
            counter = 0;
        } else
            board_reset_led(LED_DALI);
    }
}

// static void board_tx_timeout(TimerHandle_t NOUSE(dummy))
// {
//     board_reset_led(SERIAL_TX);
// }

// static void board_rx_timeout(TimerHandle_t NOUSE(dummy))
// {
//     board_reset_led(SERIAL_RX);
// }

static void board_stop_all_timer(void)
{
    // xTimerStop(board_heartbeat_timer_id, 0x0);
    // xTimerStop(board_tx_timer_id, 0x0);
    // xTimerStop(board_rx_timer_id, 0x0);
}

void board_error(void)
{
    board_set_led(SERIAL_RX);
    board_set_led(SERIAL_TX);
    board_set_led(LED_DALI);
    board_stop_all_timer();
}

void board_flash_rx(void)
{
    board_set_led(SERIAL_RX);
    // xTimerStart(board_rx_timer_id, 0x0);
}

void board_flash_tx(void)
{
    board_set_led(SERIAL_TX);
    // xTimerStart(board_tx_timer_id, 0x0);
}

void board_init(void)
{
    LOG_THIS_INVOCATION(LOG_FORCE);

    LOG_PRINTF(LOG_FORCE, "GPIO3 direction register at: %08lx", &(LPC_GPIO3->DIR));
    LPC_GPIO3->DIR |= (1 << 5);

    LOG_TEST(board_heartbeat_timer_id = xTimerCreateStatic(
                 "heart_timer", HEARTBEAT_PERIOD_MS, pdTRUE, NULL, board_heartbeat, &board_heartbeat_buffer));
    // LOG_TEST(board_rx_timer_id =
    //              xTimerCreateStatic("rx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_rx_timeout, &board_rx_buffer));
    // LOG_TEST(board_tx_timer_id =
    //              xTimerCreateStatic("tx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_tx_timeout, &board_tx_buffer));
    LOG_TEST(xTimerStart(board_heartbeat_timer_id, 0x0));
}

void board_system_init(void)
{
    board_setup_clocking();
    board_setup_pin_mux();
}