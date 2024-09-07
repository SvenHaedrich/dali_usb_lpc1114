cmake_minimum_required(VERSION 3.28)

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

include_directories(
        source
        thirdparty/cmsis-core-lpc111x/cmsis-core-lpc111x
        thirdparty/cmsis-core/cmsis-core
        thirdparty/FreeRTOS-Kernel/include
        thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM0
        )


# FreeRTOS-Kernel
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config
    INTERFACE
    "source"
)
set(FREERTOS_KERNEL_PATH "thirdparty/FreeRTOS-Kernel")
set(FREERTOS_PORT "GCC_ARM_CM0" CACHE STRING "" FORCE)


# Adding the FreeRTOS-Kernel subdirectory
add_subdirectory(${FREERTOS_KERNEL_PATH} FreeRTOS-Kernel)
target_compile_options(freertos_kernel PRIVATE
    ### Gnu/Clang C Options
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Werror>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wconversion>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>

    # Suppressions required to build clean with clang.
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-unused-macros>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-missing-variable-declarations>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-covered-switch-default>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-cast-align> 
    
    # Suppression required to build clean with gcc
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-maybe-uninitialized>
    )

add_subdirectory(source/dali_101_lpc)
add_subdirectory(source/board)
add_subdirectory(source/system)

list(APPEND SOURCE_FILES
        source/main.c
        source/serial.c
        source/freertos.c
        thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM0/portasm.c
        )

add_executable(${PROJECT_NAME}.elf ${SOURCE_FILES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf PUBLIC dali_101 board system freertos_kernel freertos_config)

set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
)