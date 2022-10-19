#include <stdbool.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board/board.h"
#include "version.h"
#include "log/log.h"
#include "dali_101_lpc/dali_101.h"
#include "serial.h"
#include "frame_pump.h"

int main(void)
{
    printf("DALI USB interface - SevenLab 2022\r\n");
    printf("Version %d.%d.%d \r\n", MAJOR_VERSION_SOFTWARE, MINOR_VERSION_SOFTWARE, BUGFIX_VERSION_SOFTWARE);
    printf(LOG_BUILD_VERSION);

    log_init();
    board_init();
    dali_tx_init();
    dali_rx_init();
    frame_pump_init();
    serial_init();

    vTaskStartScheduler();
}