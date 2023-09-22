
### Hardware Configuration

OSC - 12 MHz

PIO1_6 - UART Rx \
PIO1_7 - UART Tx \
PIO0_11 - CT32B0_MAT3 - DALI TX \
PIO1_0 - CT32B1_CAP0 - DALI RX - enable internal pull-up \
PIO2_4 - LED - heartbeat \
PIO2_5 - LED - serial activ \
PIO2_6 - LED - DALI activ 

### Baudrate

For details see chapter 13.5.15 UART Fractional Divider Register
in UM10398 User Manual.

UART_PCLK is at 48 MHz (somehow it was not possible to set UARTCLKDIV to 24 MHz)

Settings for 115,200 Baud:

| Register  | Value | Restriction |
|-----------|-------|-------------|
| DIVADDVAL | 2     | 0 .. 14, < MULVAL |
| MULVAL    | 15    | 0 .. 15     |
| DLM       | 0     | 0 .. 15     |
| DLL       | 23    | 3 .. 15     |

calculated baudrate: 115089 Baud

Settings for 500,000 Baud:

| Register  | Value | Restriction |
|-----------|-------|-------------|
| DIVADDVAL | 1     | 0 .. 14, < MULVAL |
| MULVAL    | 2     | 0 .. 15     |
| DLM       | 0     | 0 .. 15     |
| DLL       | 4     | 3 .. 15     |

calculated baudrate: 500000 Baud

### LED Usage

DALI
Heartbeat LED. When the DALI Bus is idle (high) the blink frequency is faster (approximately 10 Hz).

Rx
Inidcates that a serial command was received and processed. The blink duration is a fixed time period and does not correlate with processor activity.

Tx 
Indicated that a DALI frame was received and processed. The blink duration is a fixed time period and does not correlate with processor activity.