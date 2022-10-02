# DALI USB LPC1114FBD48

## Description

This project contains the code for the USB DALI interface.
This version is for NXP LPC1114  microcontroller.

## Setup

## Prerequisites

* LPC1114FBD48
* GCC cross compiler

Get compiler

```bash
sudo apt-get install gcc-arm-none-eabi
```
Get cmake

```bash
sudo apt-get install cmake
```

Debug build
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DLOGBACKEND_RTT=ON
cmake --build build --clean-first
```

Release build
```bash
cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --clean-first
```

If your arm-none-eabi gcc is not in your path, add the following to your initial cmake call:
```bash
-DARM_NONE_EABI_TOOLCHAIN_BIN_PATH=/$PATH_TO_TOOLCHAIN/bin/
```

### Hardware Configuration

OSC - 12 MHz

PIO1_6 - UART Rx
PIO1_7 - UART Tx
PIO0_1 - CT32B0_MAT2 - DALI TX \
PA9 - UART TX \
PA10 - UART RX
PB14 - DALI RX \

## Memory Map

## Anatomy

## Known Issues