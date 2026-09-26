// clang-format off
#include <stdint.h>   // uintXX_t
#include <string.h>   // for memcpy

#include "FreeRTOS.h"  // for pdFALSE
#include "portmacro.h" // for BaseType_t, portYIELD_FROM_ISR
#include "task.h"      // for taskDISABLE_INTERRUPTS

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
        MINOR_VERSION_SOFTWARE) "." SERIAL_STRINGIFY(BUGFIX_VERSION_SOFTWARE) " \r\n\r\n"

// '{' + 8 timestamp + separator + 2 length + ' ' + 8 data + '}' + CR + LF
#define SERIAL_MESSAGE_SIZE (24U)

/* Everything this adapter sends is a whole message: a frame report is always
   24 bytes, the longest banner line is 36. So the transmitter holds slots, not
   a stream of bytes - a message is either in a slot or it is not, which is what
   keeps two tasks from interleaving inside one message without a lock. The
   queueing that matters already happens upstream, in the receive queue that
   reports 0xA5 and the command queue that reports 0xA2, so a task that finds
   every slot taken waits rather than adding a second place to lose messages. */
#define SERIAL_SLOT_SIZE (40U)
#define SERIAL_SLOTS (4U)

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

#define SERIAL_IER_RBR (0x01U)
#define SERIAL_IER_THRE (0x02U)
#define SERIAL_IIR_THRE (1U)
#define SERIAL_IIR_RDA (2U)

static struct _serial_tx {
    char slot[SERIAL_SLOTS][SERIAL_SLOT_SIZE];
    uint8_t length[SERIAL_SLOTS];
    uint8_t head; // the slot the interrupt is emitting
    uint8_t tail; // the next slot a task may fill
    uint8_t sent; // bytes of the head slot already handed to the FIFO
    volatile uint8_t used;
} serial_tx;

static void serial_fill_transmit_fifo(void)
{
    /* LSR is asked before every byte rather than assuming a whole empty FIFO.
       The transmitter is primed from a task as well as from the interrupt, and
       the two disagreed about how much room was left, which put a stray byte
       into the stream roughly once per burst. */
    while (LPC_UART->LSR & U0LSR_THRE) {
        if (serial_tx.used == 0) {
            LPC_UART->IER &= ~SERIAL_IER_THRE;
            return;
        }
        if (serial_tx.sent < serial_tx.length[serial_tx.head]) {
            LPC_UART->THR = serial_tx.slot[serial_tx.head][serial_tx.sent++];
            continue;
        }
        serial_tx.sent = 0;
        serial_tx.head = (uint8_t)((serial_tx.head + 1U) % SERIAL_SLOTS);
        serial_tx.used--;
    }
}

/* Slots are freed by the transmit interrupt and not by another task, so waiting
   for one cannot deadlock whichever task we are, and it works before the
   scheduler has started - which main() needs for the banner. Interrupts are
   masked directly rather than through taskENTER_CRITICAL(), whose nesting count
   is not initialised until the scheduler runs. */
static void serial_send(const char* message, uint8_t length)
{
    taskDISABLE_INTERRUPTS();
    while (serial_tx.used >= SERIAL_SLOTS) {
        // let the interrupt free a slot, then look again - testing and taking
        // a slot has to be one step, or two tasks take the same one
        taskENABLE_INTERRUPTS();
        taskDISABLE_INTERRUPTS();
    }
    memcpy(serial_tx.slot[serial_tx.tail], message, length);
    serial_tx.length[serial_tx.tail] = length;
    serial_tx.tail = (uint8_t)((serial_tx.tail + 1U) % SERIAL_SLOTS);
    serial_tx.used++;
    LPC_UART->IER |= SERIAL_IER_THRE;
    /* Enabling the interrupt raises nothing while the holding register is
       already empty, so the first message of a burst has to be handed over
       here; the interrupt carries the rest. */
    if (LPC_UART->LSR & U0LSR_THRE) {
        serial_fill_transmit_fifo();
    }
    taskENABLE_INTERRUPTS();
}

void serial_print_head(void)
{
    static const char banner[] = "DALI USB interface - SevenLab 2026\r\n";
    serial_send(banner, sizeof(banner) - 1U);
    serial_send(SERIAL_VERSION_STRING, sizeof(SERIAL_VERSION_STRING) - 1U);
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
    *next++ = '\n';
    serial_send(message, (uint8_t)(next - message));
}

void UART_IRQHandler(void)
{
    const uint8_t IIR_value = LPC_UART->IIR;
    const uint8_t IIR_initd = (IIR_value >> 1) & 7;

    if (IIR_initd == SERIAL_IIR_THRE) {
        serial_fill_transmit_fifo();
        return;
    }
    if (IIR_initd == SERIAL_IIR_RDA) {
        BaseType_t higher_priority_woken = pdFALSE;
        while (LPC_UART->LSR & U0LSR_RDR) {
            command_receive_from_isr(LPC_UART->RBR, &higher_priority_woken);
        }
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

static void serial_initialize_uart_interrupt(void)
{
    LPC_UART->IER |= SERIAL_IER_RBR;
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
