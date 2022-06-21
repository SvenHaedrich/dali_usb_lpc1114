/**
  ******************************************************************************
  * @file      startup_stm32f091xc.s
  * @author    MCD Application Team
  * @brief     STM32F091xC devices vector table for GCC toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M0 processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  * 
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

  .syntax unified
  .cpu cortex-m0
  .fpu softvfp
  .thumb

.global g_pfnVectors
.global Default_Handler

/* start address for the initialization values of the .data section.
defined in linker script */
.word _sidata
/* start address for the .data section. defined in linker script */
.word _sdata
/* end address for the .data section. defined in linker script */
.word _edata
/* start address for the .bss section. defined in linker script */
.word _sbss
/* end address for the .bss section. defined in linker script */
.word _ebss

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0          /* set stack pointer */

/* Copy the data segment initializers from flash to SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit
  
/* Zero fill the bss segment. */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

/* Call the clock system intitialization function.*/
  bl  board_system_init
/* Call static constructors */
  bl __libc_init_array
/* Call the application's entry point.*/
  bl main

LoopForever:
    b LoopForever


.size Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/
    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler
/******************************************************************************
*
* The minimal vector table for a Cortex M0.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/
   .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors


g_pfnVectors:
  .word  _estack
  .word  Reset_Handler
  .word  NMI_Handler
  .word  HardFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  0
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler
  .word  WAKEUP_IRQHandler                 /* PIO0_0 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_1 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_2 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_3 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_4 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_5 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_6 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_7 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_8 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_9 Wakeup                */
  .word  WAKEUP_IRQHandler                 /* PIO0_10 Wakeup               */
  .word  WAKEUP_IRQHandler                 /* PIO0_11 Wakeup               */
  .word  WAKEUP_IRQHandler                 /* PIO1_0 Wakeup                */
  .word  CAN_IRQHandler                    /* CAN                          */
  .word  SSP1_IRQHandler                   /* SPI/SSP1                     */
  .word  I2C_IRQHandler                    /* I2C0 Interrupt               */
  .word  TIMER16_0_IRQHandler              /* 16-bit Timer 0               */
  .word  TIMER16_1_IRQHandler              /* RCC and CRS                  */
  .word  TIMER32_0_IRQHandler                /* EXTI Line 0 and 1            */
  .word  TIMER32_1_IRQHandler                /* EXTI Line 2 and 3            */
  .word  SSP0_IRQHandler               /* EXTI Line 4 to 15            */
  .word  UART_IRQHandler                    /* TSC                          */
  .word  0               /* DMA1 Channel 1               */
  .word  0  /* DMA1 Channel 2 and 3 & DMA2 Channel 1 and 2 */
  .word  ADC_IRQHandler  /* DMA1 Channel 4 to 7 & DMA2 Channel 3 to 5 */
  .word  WDT_IRQHandler              /* ADC1, COMP1 and COMP2         */
  .word  BOD_IRQHandler    /* TIM1 Break, Update, Trigger and Commutation */
  .word  0                /* TIM1 Capture Compare         */
  .word  PIOINT3_IRQHandler                   /* TIM2                         */
  .word  PIOINT2_IRQHandler                   /* TIM3                         */
  .word  PIOINT1_IRQHandler               /* TIM6 and DAC                 */
  .word  PIOINT0_IRQHandler                   /* TIM7                         */

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/

  .weak      NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak      HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak      SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak      PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak      SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

  .weak      WAKEUP_IRQHandler
  .thumb_set WAKEUP_IRQHandler,Default_Handler

  .weak      CAN_IRQHandler
  .thumb_set CAN_IRQHandler,Default_Handler

  .weak      SSP1_IRQHandler
  .thumb_set SSP1_IRQHandler,Default_Handler

  .weak      I2C_IRQHandler
  .thumb_set I2C_IRQHandler,Default_Handler

  .weak      TIMER16_0_IRQHandler
  .thumb_set TIMER16_0_IRQHandler,Default_Handler

  .weak      TIMER16_1_IRQHandler
  .thumb_set TIMER16_1_IRQHandler,Default_Handler

  .weak      TIMER32_0_IRQHandler
  .thumb_set TIMER32_0_IRQHandler,Default_Handler

  .weak      TIMER32_1_IRQHandler
  .thumb_set TIMER32_1_IRQHandler,Default_Handler

  .weak      SSP0_IRQHandler
  .thumb_set SSP0_IRQHandler,Default_Handler

  .weak      UART_IRQHandler
  .thumb_set UART_IRQHandler,Default_Handler

  .weak      ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Default_Handler

  .weak      WDT_IRQHandler
  .thumb_set WDT_IRQHandler,Default_Handler

  .weak      BOD_IRQHandler
  .thumb_set BOD_IRQHandler,Default_Handler

  .weak      PIOINT3_IRQHandler
  .thumb_set PIOINT3_IRQHandler,Default_Handler

  .weak      PIOINT2_IRQHandler
  .thumb_set PIOINT2_IRQHandler,Default_Handler

  .weak      PIOINT1_IRQHandler
  .thumb_set PIOINT1_IRQHandler,Default_Handler

  .weak      PIOINT0_IRQHandler
  .thumb_set PIOINT0_IRQHandler,Default_Handler

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/