#include <stdbool.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board/board.h"
#include "version.h"
#include "log/log.h"
#include "dali_101_lpc/dali_101.h"
#include "serial.h"

#if !(DALI_101_MAJOR_VERSION == 2U)
#error "expected DALI 101 major version 2."
#endif
#if !(DALI_101_MINOR_VERSION == 0U)
#error "expected DALI 101 minor version 0."
#endif

#define MAIN_TASK_STACKSIZE (2*configMINIMAL_STACK_SIZE)
#define MAIN_PRIORITY (tskIDLE_PRIORITY + 1)

__attribute__((noreturn)) static void main_task(__attribute__((unused))void* dummy)
{
    LOG_THIS_INVOCATION(LOG_FORCE);
    
    struct dali_rx_frame rx_frame;
    struct dali_tx_frame tx_frame;
    while(true) {
        if (dali_101_get(&rx_frame, 0)) {
            serial_print_frame(rx_frame);
        }
        if (serial_get(&tx_frame, 0)) {
            dali_101_send(tx_frame);
        }
        vTaskDelay((2/portTICK_PERIOD_MS));
    }
}

int main(void)
{
    serial_print_head();
    log_init();
    board_init();
    dali_101_init();
    serial_init();

    static StaticTask_t task_buffer;
    static StackType_t task_stack[MAIN_TASK_STACKSIZE];
    LOG_TEST(xTaskCreateStatic(main_task,"MAIN", MAIN_TASK_STACKSIZE, NULL, MAIN_PRIORITY, task_stack, &task_buffer));

    vTaskStartScheduler();
}