cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER_WORKS 1)
project(dali_usb C ASM)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/source/system/link_lpc1114_302.ld)
set(COMMON_FLAGS "-Wall -Wextra -g3 -std=gnu11 -pedantic-errors -mcpu=cortex-m0 -ffunction-sections")
set(CMAKE_C_FLAGS "${COMMON_FLAGS}")

add_link_options(-mcpu=cortex-m0)
add_link_options(-T ${LINKER_SCRIPT})
add_link_options(-Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-Wl,--gc-sections -static --specs=nano.specs -mthumb)
add_link_options(-Wl,--start-group -lc -lm -Wl,--end-group)

set(FREERTOS_PATH "thirdparty/FreeRTOS-Kernel")
include_directories(
        source
        thirdparty/cmsis-core-lpc111x/cmsis-core-lpc111x
        thirdparty/cmsis-core/cmsis-core
        ${FREERTOS_PATH}/Source/CMSIS_RTOS
        ${FREERTOS_PATH}/include
        ${FREERTOS_PATH}/portable/GCC/ARM_CM0
        )

add_subdirectory(source/dali_101_lpc)
add_subdirectory(source/board)
add_subdirectory(source/system)

list(APPEND SOURCE_FILES
        source/main.c
        source/serial.c
        source/freertos.c
        ${FREERTOS_PATH}/croutine.c
        ${FREERTOS_PATH}/event_groups.c
        ${FREERTOS_PATH}/list.c
        ${FREERTOS_PATH}/queue.c
        ${FREERTOS_PATH}/stream_buffer.c
        ${FREERTOS_PATH}/tasks.c
        ${FREERTOS_PATH}/timers.c
        ${FREERTOS_PATH}/portable/GCC/ARM_CM0/port.c
        )

add_executable(${PROJECT_NAME}.elf ${SOURCE_FILES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf PUBLIC dali_101 board system)

set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
)