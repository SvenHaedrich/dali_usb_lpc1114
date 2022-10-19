#include <stdio.h>
#include <stdbool.h>
#include <limits.h> // ULONG_MAX

#include "FreeRTOS.h"
#include "task.h"

#include "lpc11xx.h"    // UART registers
#include "bitfields.h"

#include "dali_101_lpc/dali_101.h"
#include "board/board.h"
#include "log/log.h"
#include "frame_pump.h"
#include "main.h"
#include "serial.h"

#define SERIAL_BUFFER_SIZE 20
#define SERIAL_IDX_CMD 0
#define SERIAL_IDX_ARG 1
#define SERIAL_CMD_SINGLE 'S'
#define SERIAL_CMD_TWICE 'T'
#define SERIAL_CMD_REPEAT 'R'
#define SERIAL_CMD_SPECIAL '!'
#define SERIAL_CMD_HELP '?'
#define SERIAL_CHAR_EOL 0x0d

#define SERIAL_TASK_STACKSIZE (3U * configMINIMAL_STACK_SIZE)

#if !(DALI_101_MAJOR_VERSION == 2U)
#error "expected DALI 101 major version 2."
#endif
#if !(DALI_101_MINOR_VERSION == 0U)
#error "expected DALI 101 minor version 0."
#endif

#define NOTIFY_PROCESS_BUFFER (1U)

static char serial_rx_buffer[SERIAL_BUFFER_SIZE];
static uint8_t serial_buffer_index = 0;
static volatile bool end_of_line = false;
static TaskHandle_t serial_thread;

static void serial_print_help(void)
{
    LOG_THIS_INVOCATION(LOG_UART);

    printf("DALI USB Sevenlab\r\n\r\n");
    printf("OUTPUT: {ttttttttEss dddddddd}\r\n");
    printf("where\r\n");
    printf("       tttttttt - timestamp, each tick equals 1 ms, hex representation with fixed length of 8 digits\r\n");
    printf("       E        - either '-' state ok, or '*' error detected\r\n");
    printf("       ss       - number of bits received, hex representation with fixed length of 2 digits\r\n");
    printf("       dddddddd - data received, hex representation with fixed length of 8 digits\r\n");
    printf("INPUT: Sp l d <cr>   - send single DALI frame\r\n");
    printf("       Tp l d <cr>   - send DALI frame twice\r\n");
    printf("       Rp r l d <cr> - repeat DALI frame\r\n");
    printf("       !             - print information\r\n");
    printf("       ?             - show help text\r\n");
    printf("where\r\n");
    printf("      p - frame priority 0=backward frame..6=priority 5\r\n");
    printf("      l - number of data bits to send 1..0x20 in hexadecimal representation\r\n");
    printf("      d - data to send, hexadecimal representation\r\n");
    printf("      r - number of repeats\r\n");
    printf("SAMPLE:\r\n");
    printf("     S1 10 ff00 - send BROADCAST OFF\r\n");
}

static void serial_buffer_reset(void)
{
    serial_buffer_index = 0;
    serial_rx_buffer[serial_buffer_index] = '\000';
}

static void send_command(bool send_twice)
{
    LOG_THIS_INVOCATION(LOG_UART);

    int priority;
    unsigned int length;
    unsigned long data;
    sscanf(&serial_rx_buffer[SERIAL_IDX_ARG], "%d %x %lx", (int*) &priority, &length, &data);
    const struct dali_tx_frame frame = { 
        .send_twice = send_twice,
        .repeat = 0,
        .priority = priority,
        .length = length,
        .data = data 
    };
    frame_pump_enqueue_tx_frame(frame);
}

static void send_repeated_command(void)
{
    LOG_THIS_INVOCATION(LOG_UART);

    int priority;
    unsigned int length;
    unsigned int repeat;
    unsigned long data;
    sscanf(&serial_rx_buffer[SERIAL_IDX_ARG],
           "%d %x %x %lx",
           &priority,
           &repeat,
           &length,
           &data);
    if (repeat) {
        const struct dali_tx_frame frame = { 
            .send_twice = false,
            .repeat = repeat,
            .priority = priority,
            .length = length,
            .data = data 
        };
        frame_pump_enqueue_tx_frame(frame);
    }
}

void serial_print_frame(struct dali_rx_frame frame)
{
    printf("{%08lx-%02x %08lx}\r\n", frame.timestamp, frame.length, frame.data);
}

// static void serial_print_error(struct dali_rx_frame frame)
// {
//     uint32_t data = 0;

//     data |= (frame.length) & 0xff;
//     data |= (frame.error_timer_usec & 0xfffff) << 8;
//     printf("{%08lx*%02x %08lx}\r\n", frame.timestamp, frame.error_code, data);
// }

__attribute__((noreturn)) static void serial_worker_thread(__attribute__((unused))void* dummy)
{
    LOG_THIS_INVOCATION(LOG_FORCE);

    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifications, portMAX_DELAY);
        if (result == pdPASS) {
            board_flash_rx();
            switch (serial_rx_buffer[SERIAL_IDX_CMD]) {
            case SERIAL_CMD_SINGLE:
                send_command(false);
                break;
            case SERIAL_CMD_TWICE:
                send_command(true);
                break;
            case SERIAL_CMD_REPEAT:
                send_repeated_command();
                break;
            case SERIAL_CMD_SPECIAL:
                board_flash_tx();
//                log_show_information();
                break;
            case SERIAL_CMD_HELP:
                board_flash_tx();
                serial_print_help();
                break;
            }
            serial_buffer_reset();
        }
    }
}

void UART_IRQHandler(void)
{
    if (((LPC_UART->IIR & U0IIR_INTID_MASK)>>U0IIR_INTID_SHIFT)==1U) {
        BaseType_t higher_priority_woken = pdFALSE;
        const char c = LPC_UART->RBR;
        switch (c) {
        case SERIAL_CMD_SINGLE:
        case SERIAL_CMD_TWICE:
        case SERIAL_CMD_REPEAT:
            serial_buffer_index = 0;
            serial_rx_buffer[serial_buffer_index] = c;
            break;
        case SERIAL_CMD_SPECIAL:
        case SERIAL_CMD_HELP:
            serial_buffer_index = 0;
            serial_rx_buffer[serial_buffer_index++] = c;
            serial_rx_buffer[serial_buffer_index] = '\000';
            xTaskNotifyFromISR(serial_thread, NOTIFY_PROCESS_BUFFER, eSetBits, &higher_priority_woken);
            break;
        case SERIAL_CHAR_EOL:
            serial_rx_buffer[serial_buffer_index] = '\000';
            xTaskNotifyFromISR(serial_thread, NOTIFY_PROCESS_BUFFER, eSetBits, &higher_priority_woken);
            break;
        default:
            serial_rx_buffer[serial_buffer_index] = c;
        }
        if (serial_buffer_index <= (SERIAL_BUFFER_SIZE - 1))
            serial_buffer_index++;
    }
}

static void serial_initialize_uart_interrupt(void)
{
    LPC_UART->IER |= 0x01; 
    NVIC_EnableIRQ(UART_IRQn);
}

void serial_init(void)
{
    LOG_THIS_INVOCATION(LOG_FORCE);

    static StaticTask_t buffer;
    static StackType_t stack[SERIAL_TASK_STACKSIZE];
    LOG_TEST(serial_thread = xTaskCreateStatic(serial_worker_thread,
                               "UART",
                               SERIAL_TASK_STACKSIZE,
                               NULL,
                               tskIDLE_PRIORITY + 1,
                               stack,
                               &buffer));
    serial_initialize_uart_interrupt();
}