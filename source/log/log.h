#pragma once
#include <stdio.h>

#ifdef __GNUC__
#define NOUSE(x) NOUSE_##x __attribute__((__unused__))
#else
#define NOUSE(x) NOUSE_##x
#endif

#define NELEMENTS(x) (sizeof(x) / sizeof(x[0]))

#define LOG_FORCE 0
#define LOG_HDRV 0x00000001UL
#define LOG_UART 0x00000002UL
#define LOG_INIT 0x00000004UL

/* the following works only when SOURCE_PATH_SIZE is the length of the absolute source path location
 * you can add these lines to cmake:
 * string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
 * add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")
 */

#ifdef NDEBUG
#define LOG_THIS_INVOCATION(t) ((void)0)
#define LOG_MSG(t, x) ((void)0)
#define LOG_TEST(x) x
#define LOG_TEST_PASS(x) x
#define LOG_TEST_OK(x) x
#define LOG_ASSERT(x) ((void)0)
#define LOG_PRINTF(...) ((void)0)
#define LOG_ISR_MSG(...) ((void)0)
#define LOG_BUILD_VERSION "## Release build\r\n"
#else
#include <stdbool.h>

#define LOG_THIS_INVOCATION(t) log_msg(t, __FUNCTION__)
#define LOG_MSG(t, x) log_msg(t, x)
#define LOG_ASSERT(x)                                                                                                  \
    if (0 == (x))                                                                                                      \
    log_printf(LOG_FORCE, "assert failed in file: %s (%d)", ((char*)__FILE__ + SOURCE_PATH_SIZE), __LINE__)
#define LOG_TEST(x)                                                                                                    \
    if (0 == (x))                                                                                                      \
    log_printf(LOG_FORCE, "test failed in file: %s (%d)", ((char*)__FILE__ + SOURCE_PATH_SIZE), __LINE__)
#define LOG_TEST_PASS(x)                                                                                               \
    if (pdPASS != (x))                                                                                                 \
    log_printf(LOG_FORCE, "test failed in file: %s (%d)", ((char*)__FILE__ + SOURCE_PATH_SIZE), __LINE__)
#define LOG_TEST_OK(x)                                                                                                 \
    if (HAL_OK != (x))                                                                                                 \
    log_printf(LOG_FORCE, "test failed in file: %s (%d)", ((char*)__FILE__ + SOURCE_PATH_SIZE), __LINE__)
#define LOG_PRINTF(...) log_printf(__VA_ARGS__)
#define LOG_BUILD_VERSION "## Debug build\r\n"
void log_set_active_topics(uint32_t new_topics);
void log_msg(uint32_t topics, const char* s);
int log_printf(uint32_t topics, const char* format, ...);
#endif

void log_show_information(void);
void log_init(void);
