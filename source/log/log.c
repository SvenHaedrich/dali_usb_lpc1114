#include <stdio.h>
#include <stdbool.h>

#include "log.h"

#ifndef NDEBUG
#include "SEGGER_RTT.h"
#define LOG_FUNC(fmt, ...) SEGGER_RTT_printf(0, fmt, ##__VA_ARGS__)
#else
#define LOG_FUNC(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

#ifndef NDEBUG
#include <stdarg.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "board/board.h"

// static SemaphoreHandle_t log_mutex;
// static StaticSemaphore_t log_mutex_buffer;
static uint32_t active_topics = 0;
static char* topic_names[] = { "HDRV", "UART", "INIT" };

static void log_lock(void)
{
    // if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
    //     if (core_isr_active()) {
    //         xSemaphoreTakeFromISR(log_mutex, NULL);
    //     } else {
    //         xSemaphoreTake(log_mutex, portMAX_DELAY);
    //     }
    // }
}

static void log_unlock(void)
{
    // if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()) {
    //     if (core_isr_active()) {
    //         xSemaphoreGiveFromISR(log_mutex, NULL);
    //     } else {
    //         xSemaphoreGive(log_mutex);
    //     }
    // }
}

static void log_print_time(void)
{
    // uint32_t time_now = xTaskGetTickCount();
    uint32_t time_now = 0;
    uint32_t seconds_now = (time_now / configTICK_RATE_HZ);
    uint32_t fraction_now = (time_now % configTICK_RATE_HZ);

    LOG_FUNC("[%ld.%03ld]:", seconds_now, fraction_now);
}

static void log_print_topic(uint32_t topic)
{
    for (uint_fast8_t i = 0; i < NELEMENTS(topic_names); i++) {
        if (topic & (1U << i))
            LOG_FUNC("%s:", topic_names[i]);
    }
}

void log_set_active_topics(uint32_t new_topics)
{
    active_topics = new_topics;
}

void log_msg(uint32_t topic, const char* s)
{
    if (!topic || (topic & active_topics)) {
        log_lock();
        log_print_time();
        log_print_topic(topic);
        LOG_FUNC("%s\r\n", s);
        log_unlock();
    }
}

int log_printf(uint32_t topic, const char* format, ...)
{
    int n = 0;
    char convert[128];
    va_list args;

    if (!topic || (topic & active_topics)) {
        va_start(args, format);
        n = vsprintf(convert, format, args);
        va_end(args);

        log_lock();
        log_print_time();
        log_print_topic(topic);
        LOG_FUNC("%s\r\n", convert);
        log_unlock();
    }
    return n;
}
#endif

void log_init(void)
{
#ifndef NDEBUG
    SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);
    log_set_active_topics(LOG_HDRV | LOG_LOW);
    LOG_THIS_INVOCATION(LOG_FORCE);
#endif
    // LOG_TEST(log_mutex = xSemaphoreCreateMutexStatic(&log_mutex_buffer));
}
