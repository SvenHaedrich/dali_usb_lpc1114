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

int main(void)
{
    printf("DALI USB interface - SevenLab 2022\r\n");
    printf("Version %d.%d.%d \r\n", MAJOR_VERSION_SOFTWARE, MINOR_VERSION_SOFTWARE, BUGFIX_VERSION_SOFTWARE);
    printf(LOG_BUILD_VERSION);

    log_init();
    board_init();
    dali_101_init();
    serial_init();

    vTaskStartScheduler();
}