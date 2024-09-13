#include "dali.h"
#include <stdbool.h>               // for bool, false
#include "bitfields.h"             // for TMR32B0MCR_MR3I, IOCON_R_PIO0_11_...
#include "board.h"                 // for BOARD_AHB_CLOCK
#include "dali_101_lpc/dali_101.h" // for dali_rx_irq_capture_callback, dal...
#include "lpc11xx.h"               // for LPC_TMR_TypeDef, LPC_TMR32B1, LPC...

#define DALI_TIMER_RATE_HZ (1000000U)

void board_setup_dali_clock(void)
{
    // enable GPIO, CT32B0 and CT32B1 blocks
    LPC_SYSCON->SYSAHBCLKCTRL |= (SYSAHBCLKCTRL_CT32B0 | SYSAHBCLKCTRL_CT32B1 | SYSAHBCLKCTRL_GPIO);
}

void board_dali_tx_set(bool state)
{
    LPC_GPIO0->DIR |= (1U << 11U);
    LPC_IOCON->R_PIO0_11 = (IOCON_R_PIO0_11_FUNC_MASK & (1 << IOCON_R_PIO0_11_FUNC_SHIFT)) |
                           (IOCON_R_PIO0_11_MODE_MASK & (1 << IOCON_R_PIO0_11_MODE_SHIFT)) | (IOCON_R_PIO0_11_ADMODE);
    if (state)
        LPC_GPIO0->DATA |= (1U << 11U);
    else
        LPC_GPIO0->DATA &= ~(1U << 11U);
}

void board_dali_tx_timer_stop(void)
{
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR3I | TMR32B0MCR_MR3R | TMR32B0MCR_MR3S));
    LPC_TMR32B0->EMR = (TMR32B0EMR_EM2 | (TMR32B0EMR_EMC2_MASK & (0 << TMR32B0EMR_EMC2_SHIFT)));
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
    board_dali_tx_timer_next(count, NOTHING);
    // set prescaler to base rate
    LPC_TMR32B0->PR = (BOARD_AHB_CLOCK / DALI_TIMER_RATE_HZ) - 1;
    // set timer mode
    LPC_TMR32B0->CTCR = 0;
    // on MR3 match: IRQ
    LPC_TMR32B0->MCR = (LPC_TMR32B0->MCR & ~(TMR32B0MCR_MR3I | TMR32B0MCR_MR3R | TMR32B0MCR_MR3S)) | (TMR32B0MCR_MR3I);
    // on MR3 match: toggle output - start with DALI active
    LPC_TMR32B0->EMR = TMR32B0EMR_EM3 | (TMR32B0EMR_EMC3_MASK & (3 << TMR32B0EMR_EMC3_SHIFT));
    // outputs are controlled by EMx
    LPC_TMR32B0->PWMC = 0;
    // pin function: CT32B0_MAT3
    // function mode: no pull up/down resistors
    // hysteresis disabled
    // standard gpio
    LPC_IOCON->R_PIO0_11 = (IOCON_R_PIO0_11_FUNC_MASK & (3 << IOCON_R_PIO0_11_FUNC_SHIFT)) |
                           (IOCON_R_PIO0_11_MODE_MASK & (2 << IOCON_R_PIO0_11_MODE_SHIFT)) | (IOCON_R_PIO0_11_ADMODE);
    // start timer
    LPC_TMR32B0->TCR = TMR32B0TCR_CEN;
}

void TIMER32_0_IRQHandler(void)
{
    if (LPC_TMR32B0->IR & TMR32B0IR_MR3_INTERRUPT) {
        LPC_TMR32B0->IR = TMR32B0IR_MR3_INTERRUPT;
        dali_tx_irq_callback();
    }
}

void TIMER32_1_IRQHandler(void)
{
    if (LPC_TMR32B1->IR & TMR32B0IR_MR0_INTERRUPT) {
        LPC_TMR32B1->IR = TMR32B0IR_MR0_INTERRUPT;
        dali_rx_irq_stopbit_match_callback();
    }
    if (LPC_TMR32B1->IR & TMR32B0IR_MR1_INTERRUPT) {
        LPC_TMR32B1->IR = TMR32B0IR_MR1_INTERRUPT;
        dali_rx_irq_period_match_callback();
    }
    if (LPC_TMR32B1->IR & TMR32B0IR_MR2_INTERRUPT) {
        LPC_TMR32B1->IR = TMR32B0IR_MR2_INTERRUPT;
        dali_rx_irq_query_match_callback();
    }
    if (LPC_TMR32B1->IR & TMR32B0IR_CR0_INTERRUPT) {
        LPC_TMR32B1->IR = TMR32B0IR_CR0_INTERRUPT;
        dali_rx_irq_capture_callback();
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

void board_dali_rx_set_stopbit_match(uint32_t match_count)
{
    LPC_TMR32B1->MR0 = match_count;
}

void board_dali_rx_set_period_match(uint32_t match_count)
{
    LPC_TMR32B1->MR1 = match_count;
}

void board_dali_rx_set_query_match(uint32_t match_count)
{
    LPC_TMR32B1->MR2 = match_count;
}

void board_dali_rx_stopbit_match_enable(bool enable)
{
    if (enable) {
        LPC_TMR32B1->MCR |= (TMR32B0MCR_MR0I);
    } else {
        LPC_TMR32B1->MCR &= ~(TMR32B0MCR_MR0I);
    }
}

void board_dali_rx_period_match_enable(bool enable)
{
    if (enable) {
        LPC_TMR32B1->MCR |= (TMR32B0MCR_MR1I);
    } else {
        LPC_TMR32B1->MCR &= ~(TMR32B0MCR_MR1I);
    }
}

void board_dali_rx_query_match_enable(bool enable)
{
    if (enable) {
        LPC_TMR32B1->MCR |= (TMR32B0MCR_MR2I);
    } else {
        LPC_TMR32B1->MCR &= ~(TMR32B0MCR_MR2I);
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
    LPC_TMR32B1->CCR = (TMR32B0CCR_CAP0FE | TMR32B0CCR_CAP0RE | TMR32B0CCR_CAP0I);
    board_dali_rx_stopbit_match_enable(false);
    board_dali_rx_period_match_enable(false);
    // pin function: CT32B1_CAP0
    // function mode: enable pull up resistor
    // hysteresis disabled
    // digital function mode, standard gpio
    LPC_IOCON->R_PIO1_0 = (IOCON_R_PIO1_0_FUNC_MASK & (3 << IOCON_R_PIO1_0_FUNC_SHIFT)) |
                          (IOCON_R_PIO1_0_MODE_MASK & (2 << IOCON_R_PIO1_0_MODE_SHIFT)) | (IOCON_R_PIO1_0_ADMODE);
    // start timer
    LPC_TMR32B1->TCR = TMR32B0TCR_CEN;
}
