## Setup

## Local Build

* LPC1114FBD48/302
* GCC cross compiler

Get compiler

    sudo apt-get install gcc-arm-none-eabi

Note: Ubuntu's default `arm-none-eabi-gcc` (Version 12.2) fails to build a correct binary.

Get cmake

    sudo apt-get install cmake

Debug build

Assuming that `arm-none-eabi-gcc` was installed to `/usr/bin`

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first

Release build

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first

## Gihub Action Build

This https://github.com/carlosperate/arm-none-eabi-gcc-action gihub action is used to
set up a specific version of the `arm-none-eabo-gcc` compiler toolchain.


## Flashing the Program

You can flash the local build using

    ./flash.sh

Or any other tooling. Anyhow, notice that the LPC1114 checks for a valid user code. The
criterion for valid user code (see 26.3.3 of UM10389 LPC111x User manual) is that the 
reserved Cortex-M0 exception vector location #7 (offset 0x0000 001C in the vector table)
contains the 2's complement of the check-sum of table entries 0 through 6. The on-chip
bootloader checks for this and starts the UART ISP command if the test fails.
`JLinkExe` automatically _validates_ the user code, so an immedaite readback of the
flash content will always differ from the binary file.

