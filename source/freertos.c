#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"

#include "log/log.h"
#include "board/board.h"

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
    called if a stack overflow is detected. */
    board_error();
    LOG_PRINTF(LOG_FORCE, "** stack overflow in task %s detected", pcTaskName);
    while (true)
        ;
}

void vApplicationMallocFailedHook(void)
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created. It is also called by various parts of the
    demo application. If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    board_error();
    LOG_PRINTF(LOG_FORCE, "** memory allocation failed");
    while (true)
        ;
}

void vAssertFailed(const char* file, uint16_t line)
{
    LOG_PRINTF(LOG_FORCE, "** assert failed %s (line: %d)", file, line);
    while (true)
        ;
}
