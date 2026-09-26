// clang-format off
#include <stdlib.h>   // strtoul, strtoull
#include <stdint.h>   // uintXX_t
#include <stdbool.h>  // for bool
#include <limits.h>   // UINT_MAX
#include <errno.h>    // for EAGAIN

#include "FreeRTOS.h" // tasks and queues
#include "task.h"
#include "queue.h"

#include "dali_101_lpc/dali_101.h"
#include "board/led.h"
#include "serial.h"
#include "command.h"
// clang-format on

#define COMMAND_BUFFER_SIZE 20
#define COMMAND_IDX_CMD 0
#define COMMAND_IDX_ARG 1
#define COMMAND_QUERY 'Q'
#define COMMAND_SEND 'S'
#define COMMAND_REPEAT 'R'
#define COMMAND_BACKFRAME 'Y'
#define COMMAND_HELP '?'
#define COMMAND_START_SEQ 'W'
#define COMMAND_NEXT_SEQ 'N'
#define COMMAND_EXECUTE_SEQ 'X'
#define COMMAND_CORRUPT 'I'
#define COMMAND_CHAR_TWICE '+'
#define COMMAND_CHAR_EOL 0x0d

#define COMMAND_TASK_STACKSIZE (3U * configMINIMAL_STACK_SIZE)
#define COMMAND_PRIORITY (tskIDLE_PRIORITY + 3U)
#define COMMAND_QUEUE_LENGTH (4U)
#define COMMAND_NOTIFY_PROCESS (1U)

struct _command {
    char* line;
    TaskHandle_t task_handle;
    QueueHandle_t queue_handle;
} command = { 0 };

static void report_status(enum dali_status status)
{
    const struct dali_rx_frame frame = {
        .timestamp = xTaskGetTickCount(),
        .status = status,
    };
    serial_print_frame(frame);
}

void command_report_cannot_process(void)
{
    report_status(DALI_ERROR_CAN_NOT_PROCESS);
}

static bool read_u64_hex_argument(char** position, uint64_t* value)
{
    const char* start = *position;
    *value = strtoull(start, position, 16);
    return (*position != start);
}

static bool read_u8_hex_argument(char** position, uint8_t* value)
{
    uint64_t wide_value;
    if (!read_u64_hex_argument(position, &wide_value) || wide_value > UINT8_MAX) {
        return false;
    }
    *value = (uint8_t)wide_value;
    return true;
}

static bool priority_or_length_illegal(uint8_t priority, uint8_t length)
{
    if (length > DALI_MAX_DATA_LENGTH) {
        return true;
    }
    if (priority < 1) {
        return true;
    }
    if (priority > 6) {
        return true;
    }
    return false;
}

static enum dali_frame_type get_forward_type(uint8_t priority)
{
    switch (priority) {
    case 1:
        return DALI_FRAME_FORWARD_1;
    case 2:
        return DALI_FRAME_FORWARD_2;
    case 3:
        return DALI_FRAME_FORWARD_3;
    case 4:
        return DALI_FRAME_FORWARD_4;
    case 5:
        return DALI_FRAME_FORWARD_5;
    case 6:
        return DALI_FRAME_BACK_TO_BACK;
    default:
        return DALI_FRAME_NONE;
    }
}

static enum dali_frame_type get_query_type(uint8_t priority)
{
    switch (priority) {
    case 1:
        return DALI_FRAME_QUERY_1;
    case 2:
        return DALI_FRAME_QUERY_2;
    case 3:
        return DALI_FRAME_QUERY_3;
    case 4:
        return DALI_FRAME_QUERY_4;
    case 5:
        return DALI_FRAME_QUERY_5;
    default:
        return DALI_FRAME_NONE;
    }
}

static bool data_illegal(uint64_t data, uint8_t length)
{
    if (length > DALI_MAX_DATA_LENGTH) {
        return true;
    }
    const uint64_t upper_limit = (uint64_t)1 << length;
    return (data >= upper_limit);
}

static void queue_frame(const struct dali_tx_frame frame)
{
    if (xQueueSendToBack(command.queue_handle, &frame, 0) == errQUEUE_FULL) {
        report_status(DALI_ERROR_QUEUE_FULL);
    }
}

static void query_command(char* argument_buffer)
{
    char* position = argument_buffer;
    uint8_t priority;
    uint8_t length;
    uint64_t data;

    if (!read_u8_hex_argument(&position, &priority) || !read_u8_hex_argument(&position, &length)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const char twice_indicator = *position;
    if (twice_indicator == '\000') {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    position++;
    if (!read_u64_hex_argument(&position, &data)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    if (priority_or_length_illegal(priority, length) || data_illegal(data, length)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const struct dali_tx_frame frame = { .type = get_query_type(priority),
                                         .repeat = (twice_indicator == COMMAND_CHAR_TWICE) ? 1 : 0,
                                         .length = length,
                                         .data = (uint32_t)data };
    queue_frame(frame);
}

static void send_forward_frame_command(char* argument_buffer)
{
    char* position = argument_buffer;
    uint8_t priority;
    uint8_t length;
    uint64_t data;

    if (!read_u8_hex_argument(&position, &priority) || !read_u8_hex_argument(&position, &length)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const char twice_indicator = *position;
    if (twice_indicator == '\000') {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    position++;
    if (!read_u64_hex_argument(&position, &data)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    if (priority_or_length_illegal(priority, length) || data_illegal(data, length)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const struct dali_tx_frame frame = { .type = get_forward_type(priority),
                                         .repeat = (twice_indicator == COMMAND_CHAR_TWICE) ? 1 : 0,
                                         .length = length,
                                         .data = (uint32_t)data };
    queue_frame(frame);
}

static void send_backframe_command(char* argument_buffer)
{
    char* position = argument_buffer;
    uint8_t data;

    if (!read_u8_hex_argument(&position, &data)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const struct dali_tx_frame frame = { .type = DALI_FRAME_BACKWARD, .repeat = 0, .length = 8, .data = data };
    queue_frame(frame);
}

static void send_corrupt_frame_command(void)
{
    const struct dali_tx_frame frame = { .type = DALI_FRAME_CORRUPT, .repeat = 0, .length = 0, .data = 0 };
    queue_frame(frame);
}

static void send_repeated_command(char* argument_buffer)
{
    char* position = argument_buffer;
    uint8_t priority;
    uint8_t repeat;
    uint8_t length;
    uint64_t data;

    if (!read_u8_hex_argument(&position, &priority) || !read_u8_hex_argument(&position, &repeat) ||
        !read_u8_hex_argument(&position, &length) || !read_u64_hex_argument(&position, &data)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    if (priority_or_length_illegal(priority, length) || data_illegal(data, length)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    const struct dali_tx_frame frame = {
        .type = get_forward_type(priority), .repeat = repeat, .length = length, .data = (uint32_t)data
    };
    queue_frame(frame);
}

static void next_sequence(char* argument_buffer)
{
    char* end_of_read;
    const uint32_t period_us = strtoul(argument_buffer, &end_of_read, 16);
    if (period_us == 0) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    if (dali_101_sequence_next(period_us) < 0) {
        report_status(DALI_ERROR_CAN_NOT_PROCESS);
    }
}

static void start_sequence(char* argument_buffer)
{
    char* end_of_read;
    const uint32_t period_us = strtoul(argument_buffer, &end_of_read, 16);
    if (period_us == 0) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    dali_101_sequence_start();
    if (dali_101_sequence_next(period_us) < 0) {
        report_status(DALI_ERROR_CAN_NOT_PROCESS);
    }
}

__attribute__((noreturn)) static void command_task(__attribute__((unused)) void* dummy)
{
    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, UINT_MAX, &notifications, portMAX_DELAY);
        if (result == pdPASS) {
            switch (command.line[COMMAND_IDX_CMD]) {
            case COMMAND_QUERY:
                board_flash(LED_SERIAL);
                query_command(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_SEND:
                board_flash(LED_SERIAL);
                send_forward_frame_command(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_BACKFRAME:
                board_flash(LED_SERIAL);
                send_backframe_command(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_CORRUPT:
                board_flash(LED_SERIAL);
                send_corrupt_frame_command();
                break;
            case COMMAND_REPEAT:
                board_flash(LED_SERIAL);
                send_repeated_command(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_HELP:
                board_flash(LED_SERIAL);
                serial_print_head();
                break;
            case COMMAND_START_SEQ:
                board_flash(LED_SERIAL);
                start_sequence(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_NEXT_SEQ:
                board_flash(LED_SERIAL);
                next_sequence(&command.line[COMMAND_IDX_ARG]);
                break;
            case COMMAND_EXECUTE_SEQ:
                board_flash(LED_SERIAL);
                if (dali_101_sequence_execute() < 0) {
                    report_status(DALI_ERROR_CAN_NOT_PROCESS);
                }
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

void command_receive_from_isr(char character, BaseType_t* higher_priority_woken)
{
    static char rx_buffer_1[COMMAND_BUFFER_SIZE];
    static char rx_buffer_2[COMMAND_BUFFER_SIZE];
    static char* active_buffer = rx_buffer_1;
    static uint8_t buffer_index;

    switch (character) {
    case COMMAND_SEND:
    case COMMAND_QUERY:
    case COMMAND_REPEAT:
    case COMMAND_NEXT_SEQ:
    case COMMAND_START_SEQ:
    case COMMAND_BACKFRAME:
    case COMMAND_EXECUTE_SEQ:
    case COMMAND_CORRUPT:
        buffer_index = 0;
        active_buffer[0] = character;
        break;
    case COMMAND_HELP:
        active_buffer[0] = character;
        active_buffer[1] = '\000';
        command.line = active_buffer;
        xTaskNotifyFromISR(command.task_handle, COMMAND_NOTIFY_PROCESS, eSetBits, higher_priority_woken);
        active_buffer = other_buffer(active_buffer, rx_buffer_1, rx_buffer_2);
        buffer_index = 0;
        break;
    case COMMAND_CHAR_EOL:
        active_buffer[buffer_index] = '\000';
        command.line = active_buffer;
        xTaskNotifyFromISR(command.task_handle, COMMAND_NOTIFY_PROCESS, eSetBits, higher_priority_woken);
        active_buffer = other_buffer(active_buffer, rx_buffer_1, rx_buffer_2);
        buffer_index = 0;
        break;
    default:
        active_buffer[buffer_index] = character;
    }
    if (buffer_index < (COMMAND_BUFFER_SIZE - 1))
        buffer_index++;
}

int command_get(struct dali_tx_frame* frame, TickType_t wait)
{
    const BaseType_t rc = xQueueReceive(command.queue_handle, frame, wait);
    return (rc == pdPASS) ? 0 : -EAGAIN;
}

void command_init(void)
{
    static StaticTask_t task_buffer;
    static StackType_t task_stack[COMMAND_TASK_STACKSIZE];
    command.task_handle = xTaskCreateStatic(
        command_task, "COMMAND", COMMAND_TASK_STACKSIZE, NULL, COMMAND_PRIORITY, task_stack, &task_buffer);
    configASSERT(command.task_handle);

    static uint8_t queue_storage[COMMAND_QUEUE_LENGTH * sizeof(struct dali_tx_frame)];
    static StaticQueue_t queue_buffer;
    command.queue_handle =
        xQueueCreateStatic(COMMAND_QUEUE_LENGTH, sizeof(struct dali_tx_frame), queue_storage, &queue_buffer);
    configASSERT(command.queue_handle);
}
