## Setup

## Prerequisites

* LPC1114FBD48/302
* GCC cross compiler

Get compiler

    sudo apt-get install gcc-arm-none-eabi

Get cmake

    sudo apt-get install cmake

Debug build

Assuming that `arm-none-eabi-gcc` was installed to `/usr/bin`

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first

Release build

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first
