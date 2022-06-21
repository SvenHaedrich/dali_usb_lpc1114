
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

    .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler

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
  .word  I2C_IRQHandler                    /* I2C0                         */
  .word  TIMER16_0_IRQHandler              /* 16-bit Timer 0               */
  .word  TIMER16_1_IRQHandler              /* 16-bit Timer 1               */
  .word  TIMER32_0_IRQHandler              /* 32-bit Timer 0               */
  .word  TIMER32_1_IRQHandler              /* 32-bit Timer 1               */
  .word  SSP0_IRQHandler                   /* SSP0                         */
  .word  UART_IRQHandler                   /* UART                         */
  .word  0                                 /* reserved                     */
  .word  0                                 /* reserved                     */
  .word  ADC_IRQHandler                    /* ADC                          */
  .word  WDT_IRQHandler                    /* WDT                          */
  .word  BOD_IRQHandler                    /* BOD                          */
  .word  0                                 /* reserved                     */
  .word  PIOINT3_IRQHandler                /* PIOINT3                      */
  .word  PIOINT2_IRQHandler                /* PIOINT2                      */
  .word  PIOINT1_IRQHandler                /* PIOINT1                      */
  .word  PIOINT0_IRQHandler                /* PIOINT0                      */


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
