#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "timers.h"

#include "lpc11xx.h"
#include "bitfields.h"
#include "dali_101_lpc/dali_101.h" // irq callbacks
#include "dali.h"
#include "led.h"
#include "board.h"

#define DALI_TIMER_RATE_HZ (1000000U)

uint32_t SystemCoreClock = 12000000;

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
    LPC_IOCON->PIO1_6 |= 0x01U;
    LPC_IOCON->PIO1_7 |= 0x01U;
    // set uart clock to 48 MHz
    LPC_SYSCON->UARTCLKDIV = 0x01U;
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_UART;
}

static void board_setup_clocking(void)
{
    board_setup_main_clock();
    board_setup_uart_clock();
    board_setup_dali_clock();
}

static void board_setup_IRQs(void)
{
    NVIC_SetPriority(TIMER_32_1_IRQn, IRQ_PRIO_ELEV);
    NVIC_SetPriority(TIMER_32_0_IRQn, IRQ_PRIO_HIGH);
    NVIC_EnableIRQ(TIMER_32_0_IRQn);
    NVIC_EnableIRQ(TIMER_32_1_IRQn);
    __enable_irq();
}

void board_init(void)
{
    board_setup_IRQs();
    board_led_init();
}

void board_system_init(void)
{
    board_setup_clocking(); // TODO make at least the system clock init part of the pre-main init
}