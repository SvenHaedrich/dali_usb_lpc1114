/*
      _____                      __          __
     / ___/___ _   _____  ____  / /   ____ _/ /_
     \__ \/ _ \ | / / _ \/ __ \/ /   / __ `/ __ \
    ___/ /  __/ |/ /  __/ / / / /___/ /_/ / /_/ /
   /____/\___/|___/\___/_/ /_/_____/\__,_/_.___/

   DALI USB Adapter
*/

// clang-format off
#include <stdbool.h>               // for false, true
#include <stdlib.h>                // for NULL
#include "FreeRTOS.h"              // for configMINIMAL_STACK_SIZE, StaticT...
#include "board/board.h"           // for board_init
#include "board/led.h"             // for board_flash, LED_DALI
#include "dali_101_lpc/dali_101.h" // for dali_101_get, dali_101_init, dali...
#include "portmacro.h"             // for StackType_t
#include "command.h"               // for command_execute_pending, command_...
#include "serial.h"                // for serial_init, serial_print_frame, s...
#include "task.h"                  // for vTaskStartScheduler, xTaskCreateS...
// clang-format on

#define MAIN_TASK_STACKSIZE (2U * configMINIMAL_STACK_SIZE)
#define MAIN_PRIORITY (tskIDLE_PRIORITY + 1)

__attribute__((noreturn)) static void main_task(__attribute__((unused)) void* dummy)
{
    struct dali_rx_frame rx_frame;
    while (true) {
        if (dali_101_get(&rx_frame, 0, false) == 0) {
            board_flash(LED_DALI);
            serial_print_frame(rx_frame);
        }
        if (dali_101_is_ready_for_command()) {
            command_execute_pending();
        }
    }
}

int main(void)
{
    board_init();
    dali_101_init();
    command_init();
    serial_init();
    serial_print_head();

    static StaticTask_t task_buffer;
    static StackType_t task_stack[MAIN_TASK_STACKSIZE];
    const TaskHandle_t task_handle =
        xTaskCreateStatic(main_task, "MAIN", MAIN_TASK_STACKSIZE, NULL, MAIN_PRIORITY, task_stack, &task_buffer);
    configASSERT(task_handle);

    vTaskStartScheduler();
}
