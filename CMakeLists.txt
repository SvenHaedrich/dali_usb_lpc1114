cmake_minimum_required(VERSION 3.10)


# Configure the project
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_SYSTEM_NAME generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING 1)
project(dali_usb C ASM)

include(arm-gcc-toolchain.cmake)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/source/system/link_lpc1114_302.ld)
set(COMMON_FLAGS "-Wall -g3 -std=gnu11 -mcpu=cortex-m0 -ffunction-sections")

set(CMAKE_C_FLAGS "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")

string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

add_link_options(-mcpu=cortex-m0)
add_link_options(-T ${LINKER_SCRIPT})
add_link_options(-Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-Wl,--gc-sections -static --specs=nano.specs -mfloat-abi=soft -mthumb)
add_link_options(-Wl,--start-group -lc -lm -Wl,--end-group)

add_custom_target(upload COMMAND make)

file(GLOB_RECURSE SOURCE_FILES
        "source/*.c"
        "source/system/start_lpc1114.s"
        # "./thirdparty/FreeRTOS-Kernel/croutine.c"
        # "./thirdparty/FreeRTOS-Kernel/event_groups.c"
        # "./thirdparty/FreeRTOS-Kernel/list.c"
        # "./thirdparty/FreeRTOS-Kernel/queue.c"
        # "./thirdparty/FreeRTOS-Kernel/stream_buffer.c"
        # "./thirdparty/FreeRTOS-Kernel/tasks.c"
        # "./thirdparty/FreeRTOS-Kernel/timers.c"
        # "./thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM0/port.c" 
        )

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    message("RTT enabled")
    list(APPEND SOURCE_FILES
        "source/log/SEGGER_RTT.c"
        "source/log/SEGGER_RTT_printf.c"
        )
    include_directories("source/log/RTT")
    add_definitions(-DLOGBACKEND_RTT)
endif()

include_directories(
        source
        thirdparty/cmsis-core-lpc111x/cmsis-core-lpc111x
        thirdparty/cmsis-core/cmsis-core
        thirdarty/FreeRTOS-Kernel/Source/CMSIS_RTOS
        thirdparty/FreeRTOS-Kernel/include
        thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM0
        )

add_executable(${PROJECT_NAME}.elf ${SOURCE_FILES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf m)

set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_SIZE_UTIL} --format=berkely $<TARGET_FILE:${PROJECT_NAME}.elf>)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
)