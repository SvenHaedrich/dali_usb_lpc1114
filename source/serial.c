// clang-format off
#include <stdio.h>    // for puts
#include <stdint.h>   // uintXX_t

#include "FreeRTOS.h" // for pdFALSE
#include "portmacro.h" // for BaseType_t, portYIELD_FROM_ISR

#include "lpc11xx.h" // UART registers
#include "bitfields.h"

#include "dali_101_lpc/dali_101.h"
#include "board/board.h" // irq priorities
#include "version.h"
#include "command.h"
#include "serial.h"
// clang-format on

// the version is known at compile time, so the banner needs no formatting
#define SERIAL_STRINGIFY_(x) #x
#define SERIAL_STRINGIFY(x) SERIAL_STRINGIFY_(x)
#define SERIAL_VERSION_STRING                                                                                          \
    "Version " SERIAL_STRINGIFY(MAJOR_VERSION_SOFTWARE) "." SERIAL_STRINGIFY(                                          \
        MINOR_VERSION_SOFTWARE) "." SERIAL_STRINGIFY(BUGFIX_VERSION_SOFTWARE) " \r\n\r"

// '{' + 8 timestamp + separator + 2 length + ' ' + 8 data + '}' + '\r' + '\0'
#define SERIAL_MESSAGE_SIZE (24U)

#define SERIAL_BAUDRATE_500000

#ifdef SERIAL_BAUDRATE_115200
#define SERIAL_DIVADD (2U)
#define SERIAL_MUL (15U)
#define SERIAL_DLM (0U)
#define SERIAL_DLL (23U)
#endif
#ifdef SERIAL_BAUDRATE_500000
#define SERIAL_DIVADD (1U)
#define SERIAL_MUL (2U)
#define SERIAL_DLM (0U)
#define SERIAL_DLL (4U)
#endif

void serial_print_head(void)
{
    puts("DALI USB interface - SevenLab 2026\r");
    puts(SERIAL_VERSION_STRING);
}

static char* serial_utoa(char* out, uint32_t value, uint_fast8_t digits)
{
    while (digits--) {
        *out++ = "0123456789ABCDEF"[(value >> (digits * 4)) & 0xF];
    }
    return out;
}

void serial_print_frame(const struct dali_rx_frame frame)
{
    const uint8_t length = (frame.status > DALI_OK) ? frame.status : frame.length;
    char message[SERIAL_MESSAGE_SIZE];
    char* next = message;

    *next++ = '{';
    next = serial_utoa(next, frame.timestamp, 8);
    *next++ = frame.loopback ? '>' : ':';
    next = serial_utoa(next, length, 2);
    *next++ = ' ';
    next = serial_utoa(next, frame.data, 8);
    *next++ = '}';
    *next++ = '\r';
    *next = '\000';
    puts(message);
}

void UART_IRQHandler(void)
{
    const uint8_t IIR_value = LPC_UART->IIR;
    const uint8_t IIR_initd = (IIR_value >> 1) & 7;

    if (IIR_initd == 2) {
        BaseType_t higher_priority_woken = pdFALSE;
        while (LPC_UART->LSR & U0LSR_RDR) {
            command_receive_from_isr(LPC_UART->RBR, &higher_priority_woken);
        }
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

static void serial_initialize_uart_interrupt(void)
{
    LPC_UART->IER |= 0x01;
    NVIC_SetPriority(UART_IRQn, IRQ_PRIO_NORM);
    NVIC_EnableIRQ(UART_IRQn);
}

static void serial_uart_init(void)
{
    // doc/hardware for details
    LPC_UART->LCR |= U0LCR_DLAB;
    LPC_UART->DLM = SERIAL_DLM;
    LPC_UART->DLL = SERIAL_DLL;
    LPC_UART->LCR &= ~U0LCR_DLAB;
    LPC_UART->FDR = (SERIAL_MUL << 4) | SERIAL_DIVADD;

    LPC_UART->LCR = (3 << U0LCR_WLS_SHIFT) & U0LCR_WLS_MASK;
    LPC_UART->FCR = U0FCR_FIFOEN;
    LPC_UART->TER = U0TER_TXEN;
}

void serial_init(void)
{
    serial_uart_init();
    serial_initialize_uart_interrupt();
}
