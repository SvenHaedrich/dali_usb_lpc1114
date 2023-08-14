/*
      _____                      __          __
     / ___/___ _   _____  ____  / /   ____ _/ /_
     \__ \/ _ \ | / / _ \/ __ \/ /   / __ `/ __ \
    ___/ /  __/ |/ /  __/ / / / /___/ /_/ / /_/ /
   /____/\___/|___/\___/_/ /_/_____/\__,_/_.___/

   DALI USB Adapter
*/

#include <stdbool.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board/board.h"
#include "board/led.h"
#include "version.h"
#include "dali_101_lpc/dali_101.h"
#include "serial.h"

#if !(DALI_101_MAJOR_VERSION == 3U)
#error "expected DALI 101 major version 3."
#endif
#if !(DALI_101_MINOR_VERSION == 1U)
#error "expected DALI 101 minor version 1."
#endif

#define MAIN_TASK_STACKSIZE (2 * configMINIMAL_STACK_SIZE)
#define MAIN_PRIORITY (tskIDLE_PRIORITY + 1)

__attribute__((noreturn)) static void main_task(__attribute__((unused)) void* dummy)
{
    struct dali_rx_frame rx_frame;
    struct dali_tx_frame tx_frame;
    while (true) {
        if (dali_101_get(&rx_frame, 0)) {
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

int main(void)
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