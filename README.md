# DALI USB LPC1114FBD48

## Description

This project contains the code for the USB DALI interface.
This version is for NXP LPC1114  microcontroller.

## Usage

### Commands

- - -
#### Single Frame `S`


Send a single DALI frame. The command syntax is

    'S' <priority> ' ' <bits> ' ' <data> EOL

    <priority> : inter frame timing used
    <bits>     : number of data bits to send
    <data>     : frame data to send
    EOL        : end of line = 0x0d

Example

    S2 10 FF00
    {00702fc6-10 0000ff00}

Send the command OFF to all control gears. Note that the connector will receive back the requested command.
- - - 

#### Send Twice Frame `T`

Send an identical DALI twice. The command syntax is

    'T' <priority> ' ' <bits> ' ' <data> EOL

    <priority> : inter frame timing used
    <bits>     : number of data bits to send
    <data>     : frame data to send
    EOL        : end of line = 0x0d

Example 

    T2 10 FF20
    {00710129-10 0000ff20}
    {00710146-10 0000ff20}

Send the command RESET to all control gears. Note that the connector will receive back the requested command.
- - -

#### Repeat Frame `R`

Send an identical DALI repeated times. The command syntax is

    'R' <priority> ' ' <repeat> ' ' <bits> ' ' <data> EOL

    <priority> : inter frame timing used
    <repeat>   : number of repetitions
    <bits>     : number of data bits to send
    <data>     : frame data to send
    EOL        : end of line = 0x0d

If repeat is 0, the command will be sent once.
Example 

    R2 1 10 FF20
    {0134f7e5-10 0000ff20}
    {0134f802-10 0000ff20}

Send the command RESET to all control gears. Note that the connector will receive back the requested command.
- - -
#### Request Status `!`

Request a status frame.
- - - 
#### Request Information `?`

Print information about the firmware.
- - -
#### Start Sequence `Q`

Start the defintion of a sequence.
- - -
#### Next Sequence Step `N`

Define the timing for the next step in the sequence.

    'N' ' ' <period> EOL

    <period> : time in microseconds, given in hex 
               representation.
- - -
#### Exexute Sequence `X`

Execute a defined sequence.
- - -

#### Priorities

| Priority | Meaning                   |
|----------|---------------------------|
|        0 | Backward frame priority   |
|        1 | Forward frame priority 1  |
|        2 | Forward frame priority 2  |
|        3 | Forward frame priority 3  |
|        4 | Forward frame priority 4  |
|        5 | Forward frame priority 5  |

### Results

Output messages use the following format (except for the help message) 

    '{' <timestamp> <status> <length> ' ' <data> '}'

    <timestamp> : integer number, 
                each tick represents 1 millisecond, 
                number is given in hex presentation, 
                fixed length of 8 digits
    <status>    : either a 
                "-" (minus) indicating normal frame, or 
                "*" (asteriks) indicating a status frame

for normal frames

    <bits>      : data bits received, 
                number is given in hex presentation, 
                fixed length of 2 digits
    <data>      : received data payload, 
                number is given in hex presentation, 
                fixed length of 8 digits

in case of a status frame

    <bits>      : status code 
                number is given in hex presentation, 
                fixed length of 2 digits
    <data>      : additional error information, 
                number is given in hex presentation, 
                fixed length of 8 digits

Status Codes

 | Status Code | Description                      | Information in `data`     |
 |------|----------------------------------|---------------------------|
 |   00 | No error                         | N/A                       |
 |   01 | Bad start bit timing             | Observed bit timing in µs |
 |   02 | Bad data bit timing              | Observed bit timing in µs |
 |   03 | Collision detected (loopback)    | N/A                       |
 |   04 | Collision detected (no change)   | N/A                       |
 |   05 | Collision detected (wrong state) | N/A                       |
 |   06 | Settling time violation          | N/A                       |
 |   0B | System is idle                   | N/A                       |
 |   0C | System has failure (bus low)     | N/A                       |
 |   0D | System has recovered             | N/A                       |
 |   14 | Can not process command          | N/A                       |
 |   15 | Bad argument to command          | N/A                       |
 |   16 | Queue is full                    | N/A                       |
 |   17 | Bad command                      | N/A                       |


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

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DLOGBACKEND_RTT=ON -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first

Release build

    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc
    cmake --build build --clean-first


### Hardware Configuration

OSC - 12 MHz

PIO1_6 - UART Rx \
PIO1_7 - UART Tx \
PIO0_11 - CT32B0_MAT3 - DALI TX \
PIO1_0 - CT32B1_CAP0 - DALI RX - enable internal pull-up \
PIO2_4 - LED - heartbeat \
PIO2_5 - LED - serial activ \
PIO2_6 - LED - DALI activ 

## Tests

Prepare virtual environment, than run the tests.
```bash
python3 -m venv venv
./run_tests.sh --log-level=debug

```
The test set-up requires a connected DALI-USB device, connected to a DALI power supply with its DALI inputs.

## TO DO
* Collsion detection
