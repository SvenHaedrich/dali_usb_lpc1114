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

#define DALI_TIMER_RATE_HZ 1000000

static TimerHandle_t board_heartbeat_timer_id;
static TimerHandle_t board_rx_timer_id;
static TimerHandle_t board_tx_timer_id;

static StaticTimer_t board_heartbeat_buffer;
static StaticTimer_t board_rx_buffer;
static StaticTimer_t board_tx_buffer;

#include "board.h"
#include "string.h"

uint32_t SystemCoreClock = 12000000;
void (*irq_timer_32_0)(void) = {0};


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
    // set uart clock to 12 MHz
    LPC_SYSCON->UARTCLKDIV = 0x04U;
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_UART;
}

static void board_setup_dali_tx_clock(void)
{
    // enable GPIO and CT32B0 blocks
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_CT32B0;
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSAHBCLKCTRL_GPIO;
}

static void board_setup_clocking(void)
{
    board_setup_main_clock();
    board_setup_uart_clock();
    board_setup_dali_tx_clock();
}

static void board_setup_pins(void)
{
    LPC_GPIO3->DIR |= (1U << 5U);
}

void board_set_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO3->DATA &= ~(1U << 5U);
    }
}

void board_reset_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO3->DATA |= (1U << 5U);
    }
}

bool board_get_dali_rx(void)
{
    return false;
}

void board_dali_tx_set(bool state)
{
    // pin function: PIO2_5
    // function mode: pull-up resistor enabled
    // hysteresis disabled
    // standard gpio
    LPC_GPIO0->DIR |= (1U << 1U);
    LPC_IOCON->PIO0_1 = (IOCON_PIO0_1_FUNC_MASK & (0 << IOCON_PIO0_1_FUNC_SHIFT))
                      | (IOCON_PIO0_1_MODE_MASK & (2 << IOCON_PIO0_1_MODE_SHIFT));
    if (state)
        LPC_GPIO0->DATA |= (1U << 1U);
    else
        LPC_GPIO0->DATA &= ~(1U << 1U);
}

void board_dali_tx_timer_stop(void)
{
    board_dali_tx_set(DALI_TX_IDLE);
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR2I|TMR32B0MCR_MR2R|TMR32B0MCR_MR2S));
    LPC_TMR32B0->EMR = (TMR32B0EMR_EM2|(TMR32B0EMR_EMC2_MASK & (0 << TMR32B0EMR_EMC2_SHIFT)));
}

void board_dali_tx_timer_next(uint32_t count)
{
    LPC_TMR32B0->MR2 = count;
}

void board_dali_tx_timer_setup(uint32_t count)
{
    LPC_TMR32B0->TCR = 2;
    board_dali_tx_timer_stop();
    board_dali_tx_timer_next(count);
    // set base rate
    LPC_TMR32B0->PR = (BOARD_AHB_CLOCK / DALI_TIMER_RATE_HZ) - 1;
    // set timer mode
    LPC_TMR32B0->CTCR = 0;
    // on MR2 match: IRQ, reset TC
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR2I|TMR32B0MCR_MR2R|TMR32B0MCR_MR2S)) | (TMR32B0MCR_MR2I);
    // on MR2 match: toggle output - start from 1
    LPC_TMR32B0->EMR = (TMR32B0EMR_EM2|(TMR32B0EMR_EMC2_MASK & (3 << TMR32B0EMR_EMC2_SHIFT)));
    // outputs are controlled by EMx
    LPC_TMR32B0->PWMC = 0;
    // pin function: CT32B0_MAT2
    // function mode: no pull resistors
    // hysteresis disabled
    // standard gpio
    LPC_IOCON->PIO0_1 = (IOCON_PIO0_1_FUNC_MASK & (2 << IOCON_PIO0_1_FUNC_SHIFT))
                      | (IOCON_PIO0_1_MODE_MASK & (2 << IOCON_PIO0_1_MODE_SHIFT));
    // start timer
    LPC_TMR32B0->TCR = 1;
}

void TIMER32_0_IRQHandler(void)
{
    if (LPC_TMR32B0->IR & TMR32B0IR_MR2_INTERRUPT) {
        if (irq_timer_32_0) {
            irq_timer_32_0();
        }
        LPC_TMR32B0->IR = TMR32B0IR_MR2_INTERRUPT;
    }
}

void board_dali_tx_set_callback(void (*isr)(void))
{
    irq_timer_32_0 = isr;
}


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

static void board_setup_IRQs(void)
{
    NVIC_EnableIRQ(TIMER_32_0_IRQn);
    __enable_irq();
}

void board_init(void)
{
    LOG_THIS_INVOCATION(LOG_FORCE);

    LOG_TEST(board_heartbeat_timer_id = xTimerCreateStatic(
                 "heart_timer", HEARTBEAT_PERIOD_MS, pdTRUE, NULL, board_heartbeat, &board_heartbeat_buffer));
    // LOG_TEST(board_rx_timer_id =
    //              xTimerCreateStatic("rx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_rx_timeout, &board_rx_buffer));
    // LOG_TEST(board_tx_timer_id =
    //              xTimerCreateStatic("tx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_tx_timeout, &board_tx_buffer));
    LOG_TEST(xTimerStart(board_heartbeat_timer_id, 0x0));
    board_setup_pins();
    board_setup_IRQs();
}

void board_system_init(void)
{
    board_setup_clocking();
}