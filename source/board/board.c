#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "timers.h"

#include "lpc11xx.h"
#include "bitfields.h"
#include "../main.h"
#include "../log/log.h"
#include "board.h"

#define HEARTBEAT_PERIOD_MS (100U)
#define SLOW_HEARTBEAT_COUNTER (10U)
#define FLASH_PERIOD_MS (200U)

#define DALI_TIMER_RATE_HZ (1000000U)

static TimerHandle_t board_heartbeat_timer_id;
static TimerHandle_t board_rx_timer_id;
static TimerHandle_t board_tx_timer_id;

static StaticTimer_t board_heartbeat_buffer;
static StaticTimer_t board_rx_buffer;
static StaticTimer_t board_tx_buffer;

uint32_t SystemCoreClock = 12000000;
void (*irq_timer_32_0)(void) = {0};
void (*irq_timer_32_1_capture)(void) = {0};
void (*irq_timer_32_1_stopbit)(void) = {0};
void (*irq_timer_32_1_period)(void) = {0};

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

static void board_setup_dali_clock(void)
{
    // enable GPIO, CT32B0 and CT32B1 blocks
    LPC_SYSCON->SYSAHBCLKCTRL |= (SYSAHBCLKCTRL_CT32B0|SYSAHBCLKCTRL_CT32B1|SYSAHBCLKCTRL_GPIO);
}

static void board_setup_clocking(void)
{
    board_setup_main_clock();
    board_setup_uart_clock();
    board_setup_dali_clock();
}

static void board_setup_pins(void)
{
    LPC_GPIO2->DIR |= (1U << 4U) |(1U << 5U) | (1U << 6U);
    LPC_GPIO0->DIR |= (1U << 1U);
}

void board_reset_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO2->DATA &= ~(1U << 4U);
    }
    if (id == SERIAL_RX) {
        LPC_GPIO2->DATA &= ~(1U << 5U);
    }
    if (id == SERIAL_RX) {
        LPC_GPIO2->DATA &= ~(1U << 6U);
    }
}

void board_set_led(enum board_led id)
{
    if (id == LED_DALI) {
        LPC_GPIO2->DATA |= (1U << 4U);
    }
    if (id == SERIAL_RX) {
        LPC_GPIO2->DATA |= (1U << 5U);
    }
    if (id == SERIAL_RX) {
        LPC_GPIO2->DATA |= (1U << 6U);
    }
}

void board_dali_tx_set(bool state)
{
    LPC_GPIO0->DIR |= (1U << 11U);
    LPC_IOCON->R_PIO0_11 = (IOCON_R_PIO0_11_FUNC_MASK & (1 << IOCON_R_PIO0_11_FUNC_SHIFT))
                         | (IOCON_R_PIO0_11_MODE_MASK & (1 << IOCON_R_PIO0_11_MODE_SHIFT))
                         | (IOCON_R_PIO0_11_ADMODE);
    if (state)
        LPC_GPIO0->DATA |= (1U << 11U);
    else
        LPC_GPIO0->DATA &= ~(1U << 11U);
}

void board_dali_tx_timer_stop(void)
{
    board_dali_tx_set(DALI_TX_IDLE);
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR2I|TMR32B0MCR_MR2R|TMR32B0MCR_MR2S));
    LPC_TMR32B0->EMR = (TMR32B0EMR_EM2|(TMR32B0EMR_EMC2_MASK & (0 << TMR32B0EMR_EMC2_SHIFT)));
}

void board_dali_tx_timer_next(uint32_t count, enum board_toggle toggle)
{
    LPC_TMR32B0->MR3 = count;
    if (toggle == DISABLE_TOGGLE) {
        LPC_TMR32B0->EMR &= ~TMR32B0EMR_EMC3_MASK;
    }
}

void board_dali_tx_timer_setup(uint32_t count)
{
    LPC_TMR32B0->TCR = TMR32B0TCR_CRST;
    board_dali_tx_timer_stop();
    board_dali_tx_timer_next(count,NOTHING);
    // set prescaler to base rate
    LPC_TMR32B0->PR = (BOARD_AHB_CLOCK / DALI_TIMER_RATE_HZ) - 1;
    // set timer mode
    LPC_TMR32B0->CTCR = 0;
    // on MR3 match: IRQ
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR3I|TMR32B0MCR_MR3R|TMR32B0MCR_MR3S)) | (TMR32B0MCR_MR3I);
    // on MR3 match: toggle output - start with DALI active
    LPC_TMR32B0->EMR = TMR32B0EMR_EM3 | (TMR32B0EMR_EMC3_MASK & (3 << TMR32B0EMR_EMC3_SHIFT));
    // outputs are controlled by EMx
    LPC_TMR32B0->PWMC = 0;
    // pin function: CT32B0_MAT3
    // function mode: no pull up/down resistors
    // hysteresis disabled
    // standard gpio
    LPC_IOCON->R_PIO0_11 = (IOCON_R_PIO0_11_FUNC_MASK & (3 << IOCON_R_PIO0_11_FUNC_SHIFT))
                         | (IOCON_R_PIO0_11_MODE_MASK & (2 << IOCON_R_PIO0_11_MODE_SHIFT))
                         | (IOCON_R_PIO0_11_ADMODE);
    // start timer
    LPC_TMR32B0->TCR = TMR32B0TCR_CEN;
}

void TIMER32_0_IRQHandler(void)
{
    if (LPC_TMR32B0->IR & TMR32B0IR_MR3_INTERRUPT) {
        if (irq_timer_32_0) {
            irq_timer_32_0();
        }
        LPC_TMR32B0->IR = TMR32B0IR_MR3_INTERRUPT;
    }
}

void board_dali_tx_set_callback(void (*isr)(void))
{
    irq_timer_32_0 = isr;
}

void TIMER32_1_IRQHandler(void)
{
    if (LPC_TMR32B1->IR & TMR32B0IR_CR0_INTERRUPT) {
        if (irq_timer_32_1_capture) {
            irq_timer_32_1_capture();
        }
        LPC_TMR32B1->IR = TMR32B0IR_CR0_INTERRUPT;
    }
    if (LPC_TMR32B1->IR & TMR32B0IR_MR0_INTERRUPT) {
        if (irq_timer_32_1_stopbit) {
            irq_timer_32_1_stopbit();
        }
        LPC_TMR32B1->IR = TMR32B0IR_MR0_INTERRUPT;
    }
    if (LPC_TMR32B1->IR & TMR32B0IR_MR1_INTERRUPT) {
        if (irq_timer_32_1_period) {
            irq_timer_32_1_period();
        }
        LPC_TMR32B1->IR = TMR32B0IR_MR1_INTERRUPT;
    }
}

bool board_dali_rx_pin(void)
{
    return (LPC_GPIO1->DATA & (1 << 0));
}

uint32_t board_dali_rx_get_capture(void)
{
    return LPC_TMR32B1->CR0;
}

uint32_t board_dali_rx_get_count(void)
{
    return LPC_TMR32B1->TC;
}

void board_dali_rx_set_capture_callback(void (*isr)(void))
{
    irq_timer_32_1_capture = isr;
}

void board_dali_rx_set_stopbit_match_callback(void (*isr)(void))
{
    irq_timer_32_1_stopbit = isr;
}

void board_dali_rx_set_period_match_callback(void (*isr)(void))
{
    irq_timer_32_1_period = isr;
}

void board_dali_rx_set_stopbit_match(uint32_t match_count)
{
    LPC_TMR32B1->MR0 = match_count;
}

void board_dali_rx_set_period_match(uint32_t match_count)
{
    LPC_TMR32B1->MR1 = match_count;
}

void board_dali_rx_stopbit_match_enable(bool enable)
{
    if (enable) {
        LPC_TMR32B1->MCR |= (TMR32B0MCR_MR0I);
    }
    else {
        LPC_TMR32B1->MCR &= ~(TMR32B0MCR_MR0I);
    }
}

void board_dali_rx_period_match_enable(bool enable)
{
    if (enable) {
        LPC_TMR32B1->MCR |= (TMR32B0MCR_MR1I);
    }
    else {
        LPC_TMR32B1->MCR &= ~(TMR32B0MCR_MR1I);
    }
}


void board_dali_rx_timer_setup(void)
{
    LPC_TMR32B1->TCR = TMR32B0TCR_CRST;
    // set prescaler to base rate
    LPC_TMR32B1->PR = (BOARD_AHB_CLOCK / DALI_TIMER_RATE_HZ) - 1;
    // set timer mode
    LPC_TMR32B1->CTCR = 0;
    // capture both edges, trigger IRQ
    LPC_TMR32B1->CCR = (TMR32B0CCR_CAP0FE|TMR32B0CCR_CAP0RE|TMR32B0CCR_CAP0I);
    board_dali_rx_stopbit_match_enable(false);
    // pin function: CT32B1_CAP0
    // function mode: enable pull up resistor
    // hysteresis disabled
    // digital function mode, standard gpio
    LPC_IOCON->R_PIO1_0 = (IOCON_R_PIO1_0_FUNC_MASK & (3 << IOCON_R_PIO1_0_FUNC_SHIFT)) | 
                          (IOCON_R_PIO1_0_MODE_MASK & (2 << IOCON_R_PIO1_0_MODE_SHIFT)) |
                          (IOCON_R_PIO1_0_ADMODE);
    // start timer
    LPC_TMR32B1->TCR = TMR32B0TCR_CEN;
}

static void board_heartbeat(TimerHandle_t NOUSE(dummy))
{
    static uint32_t counter = 0;

    counter++;
    if (board_dali_rx_pin()==DALI_RX_IDLE) {
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

static void board_tx_timeout(TimerHandle_t NOUSE(dummy))
{
    board_reset_led(SERIAL_TX);
}

static void board_rx_timeout(TimerHandle_t NOUSE(dummy))
{
    board_reset_led(SERIAL_RX);
}

static void board_stop_all_timer(void)
{
    xTimerStop(board_heartbeat_timer_id, 0x0);
    xTimerStop(board_tx_timer_id, 0x0);
    xTimerStop(board_rx_timer_id, 0x0);
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
    xTimerStart(board_rx_timer_id, 0x0);
}

void board_flash_tx(void)
{
    board_set_led(SERIAL_TX);
    xTimerStart(board_tx_timer_id, 0x0);
}

static void board_setup_IRQs(void)
{
    NVIC_EnableIRQ(TIMER_32_0_IRQn);
    NVIC_EnableIRQ(TIMER_32_1_IRQn);
    __enable_irq();
}

void board_init(void)
{
    LOG_THIS_INVOCATION(LOG_INIT);

    LOG_TEST(board_heartbeat_timer_id = xTimerCreateStatic(
                "heart_timer", HEARTBEAT_PERIOD_MS, pdTRUE, NULL, board_heartbeat, &board_heartbeat_buffer));
    LOG_TEST(board_rx_timer_id = xTimerCreateStatic(
                "rx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_rx_timeout, &board_rx_buffer));
    LOG_TEST(board_tx_timer_id = xTimerCreateStatic(
                "tx_timer", FLASH_PERIOD_MS, pdFALSE, NULL, board_tx_timeout, &board_tx_buffer));
    LOG_TEST(xTimerStart(board_heartbeat_timer_id, 0x0));
    board_setup_pins();
    board_reset_led(LED_DALI);
    board_reset_led(SERIAL_RX);
    board_reset_led(SERIAL_TX);
    board_setup_IRQs();
}

void board_system_init(void)
{
    board_setup_clocking();
}