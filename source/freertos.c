#include <stdbool.h>    // for true
#include <stdint.h>     // for uint16_t
#include "FreeRTOS.h"   // for vAssertFailed
#include "board/led.h"  // for board_indicate_error
#include "portable.h"   // for vApplicationMallocFailedHook
#include "task.h"       // for TaskHandle_t, tskTaskControlBlock, vApplicationMallocFailedHook

void vApplicationStackOverflowHook(__attribute__((unused)) TaskHandle_t xTask, __attribute__((unused)) char* pcTaskName)
{
    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
    called if a stack overflow is detected. */
    board_indicate_error();
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
    board_indicate_error();
    while (true)
        ;
}

void vAssertFailed(__attribute__((unused)) const char* file, __attribute__((unused)) uint16_t line)
{
    board_indicate_error();
    while (true)
        ;
}
