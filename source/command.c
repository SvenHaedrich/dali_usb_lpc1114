// clang-format off
#include <stdlib.h>   // for NULL
#include <stdint.h>   // uintXX_t
#include <stdbool.h>  // for bool
#include <limits.h>   // UINT_MAX

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

/* Everything a command asks the transmitter to do travels through one queue, so
   that MAIN carries it out in the order the host sent it. W, N and X used to run
   straight from this task, at a higher priority than MAIN, and cut into whatever
   MAIN was transmitting or building. */
enum command_kind {
    COMMAND_KIND_FRAME,
    COMMAND_KIND_SEQUENCE_START,
    COMMAND_KIND_SEQUENCE_NEXT,
    COMMAND_KIND_SEQUENCE_EXECUTE,
    COMMAND_KIND_BANNER, // answered by this task, never queued
};

struct command_item {
    union {
        struct dali_tx_frame frame;
        uint32_t period_us;
    } argument;
    enum command_kind kind;
};

struct _command {
    char* line;
    bool line_too_long; // more characters arrived than the buffer holds
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

static void queue_item(const struct command_item item)
{
    if (xQueueSendToBack(command.queue_handle, &item, 0) == errQUEUE_FULL) {
        report_status(DALI_ERROR_QUEUE_FULL);
    }
}

/* Every command that carries arguments has the same shape: hex fields in a fixed
   order, one space between them, ending at the terminator. They differ only in
   which fields they take and what is built from them, so the shape is described
   once, in a table, and read by one parser. */

enum argument {
    ARGUMENT_NONE = 0, // zero fills the unused slots of the table below
    ARGUMENT_PRIORITY,
    ARGUMENT_REPEAT,
    ARGUMENT_LENGTH,
    ARGUMENT_DATA,
    ARGUMENT_PERIOD,
};

#define ARGUMENTS_MAX (5U) // four fields and the terminator

enum frame_kind {
    FRAME_KIND_NONE = 0,
    FRAME_KIND_FORWARD, // the priority picks DALI_FRAME_FORWARD_n or back to back
    FRAME_KIND_QUERY,   // the priority picks DALI_FRAME_QUERY_n
    FRAME_KIND_BACKWARD,
    FRAME_KIND_CORRUPT,
};

struct command_spec {
    char letter;
    enum argument argument[ARGUMENTS_MAX];
    enum command_kind kind;
    enum frame_kind frame_kind;
    uint8_t priority_max;  // 5 for a query, 6 where a back to back frame is allowed
    uint8_t fixed_length;  // for the commands that carry no <bits> field
    bool twice_allowed;    // the separator before <data> may be '+'
    bool data_fits_length; // <data> has to fit into <bits>
};

/* The grammar of doc/commands.md. Adding a command is a row here. */
static const struct command_spec command_specs[] = {
    { .letter = COMMAND_QUERY,
      .argument = { ARGUMENT_PRIORITY, ARGUMENT_LENGTH, ARGUMENT_DATA },
      .kind = COMMAND_KIND_FRAME,
      .frame_kind = FRAME_KIND_QUERY,
      .priority_max = 5, // there is no back to back query
      .data_fits_length = true },
    { .letter = COMMAND_SEND,
      .argument = { ARGUMENT_PRIORITY, ARGUMENT_LENGTH, ARGUMENT_DATA },
      .kind = COMMAND_KIND_FRAME,
      .frame_kind = FRAME_KIND_FORWARD,
      .priority_max = 6,
      .twice_allowed = true,
      .data_fits_length = true },
    { .letter = COMMAND_REPEAT,
      .argument = { ARGUMENT_PRIORITY, ARGUMENT_REPEAT, ARGUMENT_LENGTH, ARGUMENT_DATA },
      .kind = COMMAND_KIND_FRAME,
      .frame_kind = FRAME_KIND_FORWARD,
      .priority_max = 6,
      .data_fits_length = true },
    { .letter = COMMAND_BACKFRAME,
      .argument = { ARGUMENT_DATA },
      .kind = COMMAND_KIND_FRAME,
      .frame_kind = FRAME_KIND_BACKWARD,
      .fixed_length = 8,
      .data_fits_length = true },
    { .letter = COMMAND_CORRUPT, .kind = COMMAND_KIND_FRAME, .frame_kind = FRAME_KIND_CORRUPT },
    { .letter = COMMAND_START_SEQ, .argument = { ARGUMENT_PERIOD }, .kind = COMMAND_KIND_SEQUENCE_START },
    { .letter = COMMAND_NEXT_SEQ, .argument = { ARGUMENT_PERIOD }, .kind = COMMAND_KIND_SEQUENCE_NEXT },
    { .letter = COMMAND_EXECUTE_SEQ, .kind = COMMAND_KIND_SEQUENCE_EXECUTE },
    { .letter = COMMAND_HELP, .kind = COMMAND_KIND_BANNER },
};

struct arguments {
    uint64_t data;
    uint32_t period_us;
    uint8_t priority;
    uint8_t repeat;
    uint8_t length;
};

static bool is_hex_digit(char character)
{
    return ((character >= '0' && character <= '9') || (character >= 'A' && character <= 'F') ||
            (character >= 'a' && character <= 'f'));
}

static uint_fast8_t hex_digit_value(char character)
{
    if (character <= '9') {
        return (uint_fast8_t)(character - '0');
    }
    if (character <= 'F') {
        return (uint_fast8_t)(character - 'A' + 10);
    }
    return (uint_fast8_t)(character - 'a' + 10);
}

/* Reads one hex field, in either case, and refuses a field that is empty or does
   not fit. strtoull used to do this and took a sign, an 0x prefix and leading
   whitespace with it. */
static bool read_hex(char** position, uint64_t* value)
{
    const char* start = *position;
    uint64_t result = 0;

    while (is_hex_digit(**position)) {
        if (result > (UINT64_MAX >> 4)) {
            return false;
        }
        result = (result << 4) | hex_digit_value(**position);
        (*position)++;
    }
    if (*position == start) {
        return false;
    }
    *value = result;
    return true;
}

static bool at_end_of_command(const char* position)
{
    return (*position == '\000');
}

/* One space, except before the <data> of a command that takes the twice
   indicator, where a '+' asks for the frame to go out twice. */
static bool
read_separator(const struct command_spec* spec, enum argument next, char** position, struct arguments* arguments)
{
    if (**position == ' ') {
        (*position)++;
        return true;
    }
    if (spec->twice_allowed && next == ARGUMENT_DATA && **position == COMMAND_CHAR_TWICE) {
        arguments->repeat = 1; // sending the frame twice is one repetition
        (*position)++;
        return true;
    }
    return false;
}

/* The limits doc/commands.md states. <data> is the only one that depends on
   another field, and <bits> always precedes it. */
static bool
read_argument(const struct command_spec* spec, enum argument which, char** position, struct arguments* arguments)
{
    uint64_t value;

    if (!read_hex(position, &value)) {
        return false;
    }
    switch (which) {
    case ARGUMENT_PRIORITY:
        if (value < 1 || value > spec->priority_max) {
            return false;
        }
        arguments->priority = (uint8_t)value;
        return true;
    case ARGUMENT_REPEAT:
        if (value > UINT8_MAX) {
            return false;
        }
        arguments->repeat = (uint8_t)value;
        return true;
    case ARGUMENT_LENGTH:
        if (value > DALI_MAX_DATA_LENGTH) {
            return false;
        }
        arguments->length = (uint8_t)value;
        return true;
    case ARGUMENT_DATA:
        // the largest value <bits> can carry is (1 << bits) - 1
        if (spec->data_fits_length && value >= ((uint64_t)1 << arguments->length)) {
            return false;
        }
        if (value > UINT32_MAX) {
            return false;
        }
        arguments->data = value;
        return true;
    case ARGUMENT_PERIOD:
        if (value > UINT32_MAX) {
            return false;
        }
        arguments->period_us = (uint32_t)value;
        return true;
    default:
        return false;
    }
}

static bool read_arguments(const struct command_spec* spec, char* position, struct arguments* arguments)
{
    *arguments = (struct arguments){ .length = spec->fixed_length };

    for (uint_fast8_t index = 0; index < ARGUMENTS_MAX; index++) {
        const enum argument which = spec->argument[index];
        if (which == ARGUMENT_NONE) {
            break;
        }
        if (index && !read_separator(spec, which, &position, arguments)) {
            return false;
        }
        if (!read_argument(spec, which, &position, arguments)) {
            return false;
        }
    }
    return at_end_of_command(position);
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

static struct dali_tx_frame build_frame(const struct command_spec* spec, const struct arguments* arguments)
{
    switch (spec->frame_kind) {
    case FRAME_KIND_FORWARD:
        return (struct dali_tx_frame){ .type = get_forward_type(arguments->priority),
                                       .repeat = arguments->repeat,
                                       .length = arguments->length,
                                       .data = (uint32_t)arguments->data };
    case FRAME_KIND_QUERY:
        return (struct dali_tx_frame){ .type = get_query_type(arguments->priority),
                                       .repeat = 0,
                                       .length = arguments->length,
                                       .data = (uint32_t)arguments->data };
    case FRAME_KIND_BACKWARD:
        return (struct dali_tx_frame){ .type = DALI_FRAME_BACKWARD,
                                       .repeat = 0,
                                       .length = arguments->length, // fixed_length of the table row
                                       .data = (uint32_t)arguments->data };
    case FRAME_KIND_CORRUPT:
        return (struct dali_tx_frame){ .type = DALI_FRAME_CORRUPT, .repeat = 0, .length = 0, .data = 0 };
    case FRAME_KIND_NONE:
        break; // a row that asks for no frame never reaches here
    }
    return (struct dali_tx_frame){ .type = DALI_FRAME_NONE, .repeat = 0, .length = 0, .data = 0 };
}

static const struct command_spec* find_command(char letter)
{
    for (uint_fast8_t index = 0; index < (sizeof(command_specs) / sizeof(command_specs[0])); index++) {
        if (command_specs[index].letter == letter) {
            return &command_specs[index];
        }
    }
    return NULL;
}

static void execute_command(const struct command_spec* spec, char* argument_buffer)
{
    struct arguments arguments;

    if (spec->kind == COMMAND_KIND_BANNER) {
        serial_print_head();
        return;
    }
    if (!read_arguments(spec, argument_buffer, &arguments)) {
        report_status(DALI_ERROR_BAD_COMMAND);
        return;
    }
    if (spec->kind == COMMAND_KIND_FRAME) {
        queue_item(
            (struct command_item){ .kind = COMMAND_KIND_FRAME, .argument.frame = build_frame(spec, &arguments) });
        return;
    }
    queue_item((struct command_item){ .kind = spec->kind, .argument.period_us = arguments.period_us });
}

__attribute__((noreturn)) static void command_task(__attribute__((unused)) void* dummy)
{
    while (true) {
        uint32_t notifications;
        const BaseType_t result = xTaskNotifyWait(pdFALSE, UINT_MAX, &notifications, portMAX_DELAY);
        if (result != pdPASS) {
            continue;
        }
        const struct command_spec* spec = find_command(command.line[COMMAND_IDX_CMD]);
        if (spec == NULL) {
            continue; // a line that never started with a command letter
        }
        board_flash(LED_SERIAL);
        if (command.line_too_long) {
            report_status(DALI_ERROR_CAN_NOT_PROCESS);
            continue;
        }
        execute_command(spec, &command.line[COMMAND_IDX_ARG]);
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
    static bool too_long;

    if (character == COMMAND_CHAR_EOL) {
        active_buffer[buffer_index] = '\000';
        command.line = active_buffer;
        command.line_too_long = too_long;
        xTaskNotifyFromISR(command.task_handle, COMMAND_NOTIFY_PROCESS, eSetBits, higher_priority_woken);
        active_buffer = other_buffer(active_buffer, rx_buffer_1, rx_buffer_2);
        buffer_index = 0;
        too_long = false;
    } else if (find_command(character) != NULL) {
        // a command letter starts a new line wherever it arrives
        active_buffer[0] = character;
        buffer_index = 1;
        too_long = false;
    } else if (buffer_index < (COMMAND_BUFFER_SIZE - 1)) {
        // only appending advances the index, so an empty line cannot inherit a stale letter
        active_buffer[buffer_index++] = character;
    } else {
        too_long = true;
    }
}

void command_execute_pending(void)
{
    struct command_item item;
    if (xQueuePeek(command.queue_handle, &item, 0) != pdPASS) {
        return;
    }
    // a frame waits for the transmitter so that it cannot cut into another one;
    // a sequence does not, W is documented to stop whatever is on the wire
    if (item.kind == COMMAND_KIND_FRAME && !dali_101_is_ready_for_command()) {
        return;
    }
    (void)xQueueReceive(command.queue_handle, &item, 0);
    int rc = 0; // every kind below assigns it, but the switch names no default
    switch (item.kind) {
    case COMMAND_KIND_FRAME:
        rc = dali_101_send(item.argument.frame);
        break;
    case COMMAND_KIND_SEQUENCE_START:
        dali_101_sequence_start();
        rc = dali_101_sequence_next(item.argument.period_us);
        break;
    case COMMAND_KIND_SEQUENCE_NEXT:
        rc = dali_101_sequence_next(item.argument.period_us);
        break;
    case COMMAND_KIND_SEQUENCE_EXECUTE:
        rc = dali_101_sequence_execute();
        break;
    case COMMAND_KIND_BANNER:
        return; // printed by the COMMAND task, it never reaches this queue
    }
    if (rc < 0) {
        report_status(DALI_ERROR_CAN_NOT_PROCESS);
    }
}

void command_init(void)
{
    static StaticTask_t task_buffer;
    static StackType_t task_stack[COMMAND_TASK_STACKSIZE];
    command.task_handle = xTaskCreateStatic(
        command_task, "COMMAND", COMMAND_TASK_STACKSIZE, NULL, COMMAND_PRIORITY, task_stack, &task_buffer);
    configASSERT(command.task_handle);

    static uint8_t queue_storage[COMMAND_QUEUE_LENGTH * sizeof(struct command_item)];
    static StaticQueue_t queue_buffer;
    command.queue_handle =
        xQueueCreateStatic(COMMAND_QUEUE_LENGTH, sizeof(struct command_item), queue_storage, &queue_buffer);
    configASSERT(command.queue_handle);
}
