// clang-format off
#include "watchdog.h"
#include <stdint.h>     // for uint32_t
#include "bitfields.h"  // for SYSAHBCLKCTRL_WDT, WDMOD_WDEN, WDMOD_WDRESET
#include "lpc11xx.h"    // for LPC_SYSCON, LPC_WDT, __disable_irq
// clang-format on

#define WATCHDOG_TIMEOUT_S (60U)
#define WATCHDOG_CLKSEL_IRC (0U)
// 12 MHz IRC / 250, and the watchdog counts every fourth clock: 12 kHz
#define WATCHDOG_CLKDIV (250U)
#define WATCHDOG_COUNT_HZ (12000000U / WATCHDOG_CLKDIV / 4U)

void board_watchdog_feed(void)
{
#ifdef BOARD_WATCHDOG
    // nothing may reach the watchdog between the two writes of a feed
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    LPC_WDT->FEED = 0xAAU;
    LPC_WDT->FEED = 0x55U;
    __set_PRIMASK(primask);
#endif
}

void board_watchdog_start(void)
{
#ifdef BOARD_WATCHDOG
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_WDT;
    LPC_SYSCON->WDTCLKSEL = WATCHDOG_CLKSEL_IRC;
    LPC_SYSCON->WDTCLKUEN = 0x00U;
    LPC_SYSCON->WDTCLKUEN = WDTCLKUEN_ENA;
    LPC_SYSCON->WDTCLKDIV = WATCHDOG_CLKDIV;

    LPC_WDT->TC = WATCHDOG_TIMEOUT_S * WATCHDOG_COUNT_HZ;
    LPC_WDT->MOD = WDMOD_WDEN | WDMOD_WDRESET;
    // the watchdog only starts counting with its first feed
    board_watchdog_feed();
#endif
}
