/*
      _____                      __          __
     / ___/___ _   _____  ____  / /   ____ _/ /_
     \__ \/ _ \ | / / _ \/ __ \/ /   / __ `/ __ \
    ___/ /  __/ |/ /  __/ / / / /___/ /_/ / /_/ /
   /____/\___/|___/\___/_/ /_/_____/\__,_/_.___/

   DALI USB Adapter
*/

#include <stdbool.h>                // for false, true
#include <stdlib.h>                 // for NULL
#include "FreeRTOS.h"               // for configMINIMAL_STACK_SIZE, StaticT...
#include "board/board.h"            // for board_init
#include "board/led.h"              // for board_flash, LED_DALI
#include "dali_101_lpc/dali_101.h"  // for dali_101_get, dali_101_init, dali...
#include "portmacro.h"              // for StackType_t
#include "serial.h"                 // for serial_get, serial_init, serial_p...
#include "task.h"                   // for vTaskStartScheduler, xTaskCreateS...

#if !(DALI_101_MAJOR_VERSION == 4U)
#error "expected DALI 101 major version 4."
#endif
#if !(DALI_101_MINOR_VERSION == 1U)
#error "expected DALI 101 minor version 1."
#endif

#define MAIN_TASK_STACKSIZE (2U * configMINIMAL_STACK_SIZE)
#define MAIN_PRIORITY (tskIDLE_PRIORITY + 1)

__attribute__((noreturn)) static void main_task(__attribute__((unused)) void* dummy)
{
    struct dali_rx_frame rx_frame;
    struct dali_tx_frame tx_frame;
    while (true) {
        if (dali_101_get(&rx_frame, 0, false)) {
            board_flash(LED_DALI);
            serial_print_frame(rx_frame);
        }
        if (dali_101_tx_is_idle()) {
            if (serial_get(&tx_frame, 0)) {
                dali_101_send(tx_frame);
            }
        }
    }
}

__attribute__((noreturn)) int main(void)
{
    board_init();
    dali_101_init();
    serial_init();
    serial_print_head();

    static StaticTask_t task_buffer;
    static StackType_t task_stack[MAIN_TASK_STACKSIZE];
    xTaskCreateStatic(main_task, "MAIN", MAIN_TASK_STACKSIZE, NULL, MAIN_PRIORITY, task_stack, &task_buffer);

    vTaskStartScheduler();
}
