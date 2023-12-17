#include <stdio.h>
#include <stdbool.h>
#include <limits.h> // ULONG_MAX

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "lpc11xx.h" // UART registers
#include "bitfields.h"

#include "dali_101_lpc/dali_101.h"
#include "board/led.h"
#include "board/board.h" // irq priorities
#include "version.h"
#include "serial.h"

#define SERIAL_BUFFER_SIZE 20
#define SERIAL_IDX_CMD 0
#define SERIAL_IDX_ARG 1
#define SERIAL_CMD_QUERY 'Q'
#define SERIAL_CMD_SEND 'S'
#define SERIAL_CMD_REPEAT 'R'
#define SERIAL_CMD_BACKFRAME 'Y'
#define SERIAL_CMD_STATUS '!'
#define SERIAL_CMD_HELP '?'
#define SERIAL_CMD_START_SEQ 'W'
#define SERIAL_CMD_NEXT_SEQ 'N'
#define SERIAL_CMD_EXECUTE_SEQ 'X'
#define SERIAL_CMD_CORRUPT 'I'
#define SERIAL_CHAR_TWICE '+'
#define SERIAL_CHAR_EOL 0x0d

#define SERIAL_TASK_STACKSIZE (3U * configMINIMAL_STACK_SIZE)
#define SERIAL_PRIORITY (tskIDLE_PRIORITY + 2U)
#define SERIAL_QUEUE_LENGTH (4U)
#define SERIAL_NOTIFY_PROCESSS (1U)

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

struct _serial {
    char* cmd_buffer;
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
    const char c = frame.loopback ? '>' : ':';
    const uint8_t length = (frame.status > DALI_OK) ? frame.status : frame.length;
    printf("{%08lx%c%02x %08lx}\r\n", frame.timestamp, c, length, frame.data);
}

static void print_parameter_error(void)
{
    const struct dali_rx_frame frame = {
        .timestamp = xTaskGetTickCount(),
        .status = DALI_ERROR_BAD_COMMAND,
    };
    serial_print_frame(frame);
}

static void print_queue_full_error(void)
{
    const struct dali_rx_frame frame = {
        .timestamp = xTaskGetTickCount(),
        .status = DALI_ERROR_QUEUE_FULL,
    };
    serial_print_frame(frame);
}

static void queue_frame(const struct dali_tx_frame frame)
{
    if (xQueueSendToBack(serial.queue_handle, &frame, 0) == errQUEUE_FULL) {
        print_queue_full_error();
    }
}

static void query_command(char* argument_buffer)
{
    int priority;
    char twice_indicator;
    unsigned int length;
    unsigned long data;
    const int n = sscanf(argument_buffer, "%d %x%c%lx", &priority, &length, &twice_indicator, &data);
    if (n != 4 || priority < DALI_PRIORITY_1 || priority > DALI_PRIORITY_5 || length > DALI_MAX_DATA_LENGTH) {
        print_parameter_error();
        return;
    }
    const struct dali_tx_frame frame = { .is_query = true,
                                         .is_corrupt = false,
                                         .repeat = (twice_indicator == SERIAL_CHAR_TWICE) ? 1 : 0,
                                         .priority = priority,
                                         .length = length,
                                         .data = data };
    queue_frame(frame);
}

static void send_forward_frame_command(char* argument_buffer)
{
    int priority;
    char twice_indicator;
    unsigned int length;
    unsigned long data;
    const int n = sscanf(argument_buffer, "%d %x%c%lx", &priority, &length, &twice_indicator, &data);
    if (n != 4 || priority < DALI_PRIORITY_1 || priority > DALI_PRIORITY_5 || length > DALI_MAX_DATA_LENGTH) {
        print_parameter_error();
        return;
    }
    const struct dali_tx_frame frame = { .is_query = false,
                                         .is_corrupt = false,
                                         .repeat = (twice_indicator == SERIAL_CHAR_TWICE) ? 1 : 0,
                                         .priority = priority,
                                         .length = length,
                                         .data = data };
    queue_frame(frame);
}

static void send_backframe_command(char* argument_buffer)
{
    uint32_t data;
    const int n = sscanf(argument_buffer, "%lx", &data);
    if (n != 1 || data > 0xFF) {
        print_parameter_error();
        return;
    }
    const struct dali_tx_frame frame = { .is_query = false,
                                         .is_corrupt = false,
                                         .repeat = 0, 
                                         .priority = DALI_BACKWARD_FRAME, 
                                         .length = 8, 
                                         .data = data };
    queue_frame(frame);
}

static void send_corrupt_frame_command(void)
{
    const struct dali_tx_frame frame = { .is_corrupt = true,
                                         .is_query = false,
                                         .repeat = 0,
                                         .priority = 0,
                                         .length = 0,
                                         .data = 0 };
    queue_frame(frame);
}

static void send_repeated_command(char* argument_buffer)
{
    int priority;
    unsigned int length;
    unsigned int repeat;
    unsigned long data;
    const int n = sscanf(argument_buffer, "%d %x %x %lx", &priority, &repeat, &length, &data);
    if (n != 4 || priority < DALI_PRIORITY_1 || priority > DALI_PRIORITY_5 || length > DALI_MAX_DATA_LENGTH) {
        print_parameter_error();
        return;
    }
    const struct dali_tx_frame frame = { .repeat = repeat, .priority = priority, .length = length, .data = data };
    queue_frame(frame);
}

static void next_sequence(char* argument_buffer)
{
    unsigned long period_us;
    const int n = sscanf(argument_buffer, "%lx", &period_us);
    if (n != 1) {
        print_parameter_error();
        return;
    }
    dali_101_sequence_next(period_us);
}

static void start_sequence(char* argument_buffer)
{
    unsigned long period_us;
    const int n = sscanf(argument_buffer, "%lx", &period_us);
    if (n != 1) {
        print_parameter_error();
        return;
    }
    dali_101_sequence_start();
    dali_101_sequence_next(period_us);
}

__attribute__((noreturn)) static void serial_task(__attribute__((unused)) void* dummy)
{
    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, ULONG_MAX, &notifications, portMAX_DELAY);
        if (result == pdPASS) {
            switch (serial.cmd_buffer[SERIAL_IDX_CMD]) {
            case SERIAL_CMD_QUERY:
                board_flash(LED_SERIAL);
                query_command(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_SEND:
                board_flash(LED_SERIAL);
                send_forward_frame_command(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_BACKFRAME:
                board_flash(LED_SERIAL);
                send_backframe_command(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_CORRUPT:
                board_flash(LED_SERIAL);
                send_corrupt_frame_command();
                break;
            case SERIAL_CMD_REPEAT:
                board_flash(LED_SERIAL);
                send_repeated_command(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_STATUS:
                board_flash(LED_SERIAL);
                dali_101_request_status_frame();
                break;
            case SERIAL_CMD_HELP:
                board_flash(LED_SERIAL);
                serial_print_head();
                break;
            case SERIAL_CMD_START_SEQ:
                board_flash(LED_SERIAL);
                start_sequence(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_NEXT_SEQ:
                board_flash(LED_SERIAL);
                next_sequence(&serial.cmd_buffer[SERIAL_IDX_ARG]);
                break;
            case SERIAL_CMD_EXECUTE_SEQ:
                board_flash(LED_SERIAL);
                dali_101_sequence_execute();
                break;
            }
        }
    }
}

static char* other_buffer(char* active, char* one, char* two)
{
    if (active == one) {
        return two;
    }
    return one;
}

void UART_IRQHandler(void)
{
    static char rx_buffer_1[SERIAL_BUFFER_SIZE];
    static char rx_buffer_2[SERIAL_BUFFER_SIZE];
    static char* active_buffer = rx_buffer_1;
    static uint8_t buffer_index;

    const uint8_t IIR_value = LPC_UART->IIR;
    const uint8_t IIR_initd = (IIR_value >> 1) & 7;

    if (IIR_initd == 2) {
        BaseType_t higher_priority_woken = pdFALSE;
        while (LPC_UART->LSR & 1) {
            const char c = LPC_UART->RBR;
            switch (c) {
            case SERIAL_CMD_SEND:
            case SERIAL_CMD_QUERY:
            case SERIAL_CMD_REPEAT:
            case SERIAL_CMD_NEXT_SEQ:
            case SERIAL_CMD_START_SEQ:
            case SERIAL_CMD_BACKFRAME:
            case SERIAL_CMD_EXECUTE_SEQ:
            case SERIAL_CMD_CORRUPT:
                buffer_index = 0;
                active_buffer[0] = c;
                break;
            case SERIAL_CMD_STATUS:
            case SERIAL_CMD_HELP:
                active_buffer[0] = c;
                active_buffer[1] = '\000';
                serial.cmd_buffer = active_buffer;
                xTaskNotifyFromISR(serial.task_handle, SERIAL_NOTIFY_PROCESSS, eSetBits, &higher_priority_woken);
                active_buffer = other_buffer(active_buffer, rx_buffer_1, rx_buffer_2);
                buffer_index = 0;
                break;
            case SERIAL_CHAR_EOL:
                active_buffer[buffer_index] = '\000';
                serial.cmd_buffer = active_buffer;
                xTaskNotifyFromISR(serial.task_handle, SERIAL_NOTIFY_PROCESSS, eSetBits, &higher_priority_woken);
                active_buffer = other_buffer(active_buffer, rx_buffer_1, rx_buffer_2);
                buffer_index = 0;
                break;
            default:
                active_buffer[buffer_index] = c;
            }
            if (buffer_index < (SERIAL_BUFFER_SIZE - 1))
                buffer_index++;
        }
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