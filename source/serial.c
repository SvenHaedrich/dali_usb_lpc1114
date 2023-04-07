#include <stdio.h>
#include <stdbool.h>
#include <limits.h> // ULONG_MAX

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lpc11xx.h" // UART registers
#include "bitfields.h"

#include "dali_101_lpc/dali_101.h"
#include "board/board.h"
#include "main.h"
#include "version.h"
#include "serial.h"

#define SERIAL_BUFFER_SIZE 20
#define SERIAL_IDX_CMD 0
#define SERIAL_IDX_ARG 1
#define SERIAL_CMD_SINGLE 'S'
#define SERIAL_CMD_TWICE 'T'
#define SERIAL_CMD_REPEAT 'R'
#define SERIAL_CMD_STATUS '!'
#define SERIAL_CMD_HELP '?'
#define SERIAL_CMD_START_SEQ 'Q'
#define SERIAL_CMD_NEXT_SEQ 'N'
#define SERIAL_CMD_EXECUTE_SEQ 'X'
#define SERIAL_CHAR_EOL 0x0d

#define SERIAL_TASK_STACKSIZE (3U * configMINIMAL_STACK_SIZE)
#define SERIAL_PRIORITY (tskIDLE_PRIORITY + 2U)
#define SERIAL_QUEUE_LENGTH (3U)
#define SERIAL_NOTIFY_PROCESSS (1U)

struct _serial {
    char rx_buffer[SERIAL_BUFFER_SIZE];
    uint8_t buffer_index;
    TaskHandle_t task_handle;
    QueueHandle_t queue_handle;
} serial = { 0 };

void serial_print_head(void)
{
    printf("DALI USB interface - SevenLab 2023\r\n");
    printf("Version %d.%d.%d \r\n", MAJOR_VERSION_SOFTWARE, MINOR_VERSION_SOFTWARE, BUGFIX_VERSION_SOFTWARE);
    printf("\r\n");
}

void serial_print_frame(const struct dali_rx_frame frame)
{
    const char c = frame.is_status ? '*' : '-';
    printf("{%08lx%c%02x %08lx}\r\n", frame.timestamp, c, frame.length, frame.data);
}

static void buffer_reset(void)
{
    serial.buffer_index = 0;
    serial.rx_buffer[serial.buffer_index] = '\000';
}

static void print_parameter_error(void)
{
    const struct dali_rx_frame frame = {
        .is_status = true, .timestamp = xTaskGetTickCount(), .length = DALI_ERROR_BAD_COMMAND, .data = 0
    };
    serial_print_frame(frame);
}

static void send_command(bool send_twice)
{
    int priority;
    unsigned int length;
    unsigned long data;
    const int n = sscanf(&serial.rx_buffer[SERIAL_IDX_ARG], "%d %x %lx", (int*)&priority, &length, &data);
    if (n != 3) {
        print_parameter_error();
    }
    const struct dali_tx_frame frame = {
        .repeat = send_twice ? 1 : 0, .priority = priority, .length = length, .data = data
    };
    xQueueSendToBack(serial.queue_handle, &frame, 0);
}

static void send_repeated_command(void)
{
    int priority;
    unsigned int length;
    unsigned int repeat;
    unsigned long data;
    const int n = sscanf(&serial.rx_buffer[SERIAL_IDX_ARG], "%d %x %x %lx", &priority, &repeat, &length, &data);
    if (n != 4) {
        print_parameter_error();
    }
    const struct dali_tx_frame frame = { .repeat = repeat, .priority = priority, .length = length, .data = data };
    xQueueSendToBack(serial.queue_handle, &frame, 0);
}

static void next_sequence(void)
{
    unsigned long period_us;
    const int n = sscanf(&serial.rx_buffer[SERIAL_IDX_ARG], "%lx", &period_us);
    if (n != 1) {
        print_parameter_error();
    }
    dali_101_sequence_next(period_us);
}

__attribute__((noreturn)) static void serial_task(__attribute__((unused)) void* dummy)
{
    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifications, portMAX_DELAY);
        if (result == pdPASS) {
            board_flash_rx();
            switch (serial.rx_buffer[SERIAL_IDX_CMD]) {
            case SERIAL_CMD_SINGLE:
                send_command(false);
                break;
            case SERIAL_CMD_TWICE:
                send_command(true);
                break;
            case SERIAL_CMD_REPEAT:
                send_repeated_command();
                break;
            case SERIAL_CMD_STATUS:
                board_flash_tx();
                dali_101_request_status_frame();
                break;
            case SERIAL_CMD_HELP:
                board_flash_tx();
                serial_print_head();
                break;
            case SERIAL_CMD_START_SEQ:
                board_flash_tx();
                dali_101_sequence_start();
                break;
            case SERIAL_CMD_NEXT_SEQ:
                board_flash_tx();
                next_sequence();
                break;
            case SERIAL_CMD_EXECUTE_SEQ:
                board_flash_tx();
                dali_101_sequence_execute();
                break;
            }
            buffer_reset();
        }
    }
}

void UART_IRQHandler(void)
{
    uint8_t IIR_value = LPC_UART->IIR;
    uint8_t IIR_initd = (IIR_value >> 1) & 7;

    if (IIR_initd == 2) {
        BaseType_t higher_priority_woken = pdFALSE;
        const char c = LPC_UART->RBR;
        switch (c) {
        case SERIAL_CMD_SINGLE:
        case SERIAL_CMD_TWICE:
        case SERIAL_CMD_REPEAT:
        case SERIAL_CMD_NEXT_SEQ:
            serial.buffer_index = 0;
            serial.rx_buffer[serial.buffer_index] = c;
            break;
        case SERIAL_CMD_START_SEQ:
        case SERIAL_CMD_EXECUTE_SEQ:
        case SERIAL_CMD_STATUS:
        case SERIAL_CMD_HELP:
            serial.buffer_index = 0;
            serial.rx_buffer[serial.buffer_index++] = c;
            serial.rx_buffer[serial.buffer_index] = '\000';
            xTaskNotifyFromISR(serial.task_handle, SERIAL_NOTIFY_PROCESSS, eSetBits, &higher_priority_woken);
            break;
        case SERIAL_CHAR_EOL:
            serial.rx_buffer[serial.buffer_index] = '\000';
            xTaskNotifyFromISR(serial.task_handle, SERIAL_NOTIFY_PROCESSS, eSetBits, &higher_priority_woken);
            break;
        default:
            serial.rx_buffer[serial.buffer_index] = c;
        }
        if (serial.buffer_index < (SERIAL_BUFFER_SIZE - 1))
            serial.buffer_index++;
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

bool serial_get(struct dali_tx_frame* frame, TickType_t wait)
{
    const BaseType_t rc = xQueueReceive(serial.queue_handle, frame, wait);
    return (rc == pdPASS);
}

static void serial_initialize_uart_interrupt(void)
{
    LPC_UART->IER |= 0x01;
    NVIC_SetPriority(UART_IRQn, 1);
    NVIC_EnableIRQ(UART_IRQn);
}

static void serial_uart_init(void)
{
    // see sample 13.5.15.1.2 from user manual
    // uart clock = 12 MHz
    // desired baudrate = 115200
    // DIVADDVAL = 5
    // MULVAL = 8
    // DLM = 0
    // DLL = 4
    LPC_UART->LCR |= U0LCR_DLAB;
    LPC_UART->DLM = 0;
    LPC_UART->DLL = 4;
    LPC_UART->LCR &= ~U0LCR_DLAB;
    LPC_UART->FDR = (8 << 4) | (5);

    LPC_UART->LCR = (3 << U0LCR_WLS_SHIFT) & U0LCR_WLS_MASK;
    LPC_UART->FCR = U0FCR_FIFOEN;
    LPC_UART->TER = U0TER_TXEN;
}

void serial_init(void)
{
    static StaticTask_t task_buffer;
    static StackType_t task_stack[SERIAL_TASK_STACKSIZE];
    serial.task_handle = xTaskCreateStatic(
        serial_task, "SERIAL", SERIAL_TASK_STACKSIZE, NULL, SERIAL_PRIORITY, task_stack, &task_buffer);
    configASSERT(serial.task_handle);

    static uint8_t queue_storage[SERIAL_QUEUE_LENGTH * sizeof(struct dali_tx_frame)];
    static StaticQueue_t queue_buffer;
    serial.queue_handle =
        xQueueCreateStatic(SERIAL_QUEUE_LENGTH, sizeof(struct dali_tx_frame), queue_storage, &queue_buffer);
    configASSERT(serial.queue_handle);

    serial_uart_init();
    serial_initialize_uart_interrupt();
}