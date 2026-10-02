# Commands

End every full command with an End of Line (EOL, 0x0D) character.
All described commands need some time to process, wait 0.2 ms before you transmit the next command.
In case commands are sent too fast error code 0xA2 will be reported.

## Serial Parameters

Baudrate: 500,000 Baud \
Data bits: 8 \
Start bit: 1 \
Stop bit: 1

## Error Handling

If an error is detected during command processing it will trigger a DALI frame message with the respective [error code](messages.md).

## Query `Q`

Send a DALI forward frame and report the systems reaction. A frame message is always generated.

    'Q' <priority> ' ' <bits> ' ' <data>

    'Q'        : command code
    <priority> : inter frame timing used. In the range 1..5 as defined in IEC 62386-101:2022 Table 22.
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    <data>     : frame data to send in hex presentation

## Send Frame `S`

Send a DALI forward frame.

    'S' <priority> ' ' <bits> (' '|'+') <data>

    'S'        : command code
    <priority> : inter frame timing used. In the range 1..5 as defined in IEC 62386-101:2022 Table 22.
                 priority = 6 sends the frame immediately after the stop condition
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    ' ' | '+'  : a plus indicates that the frame is sent twice
    <data>     : frame data to send in hex presentation

## Repeat Frame `R`

Send identical DALI frames multiple times. Note that sending repeated frames twice is not supported.

    'R' <priority> ' ' <repeat> ' ' <bits> ' ' <data>

    'R'        : command code
    <priority> : inter frame timing used. In the range 1..5 as defined in IEC 62386-101:2022 Table 22.
                 priority = 6 sends the frames immediately after the stop condition
    <repeat>   : number of additional repetitions in hex presentation (00..FF)
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    <data>     : frame data to send in hex presentation

## Send Backward Frame `Y`

Send a backward frame.

    'Y' <value>

    'Y'     : command code
    <value> : value to transmit in hex presentation (00..FF)

## Send Corrupt Backward Frame `I`

Send a corrupt backward frame as described in IEC 62386-101:2022 9.6.2.

    'I'     : command code

## Request Information `?`

Print information about the firmware.

    '?'     : command code

The output will be similar to this message:

    DALI USB interface - SevenLab 2026
    Version X.Y.Z

## Start Sequence `W`

Start the definition of a sequence. This command stops ongoing transmissions immediately and might leave a corrupt DALI frame behind which will be reported as such.

    'W' <period>

    'W'      : command code
    <period> : time in microseconds, given in hex representation. Minimal time is 25 microseconds, maximum total time of the sequence is limited to about 71 minutes.

## Next Sequence Step `N`

Continue to define the timing for a sequence. The maximum number of Next Sequence Steps is 66 per transmission.

    'N' <period>

    'N'      : command code
    <period> : time in microseconds, given in hex representation. Minimal time is 25 microseconds, maximum total time of the sequence is limited to about 71 minutes.

## Execute Sequence `X`

Execute a defined sequence. The command is executed immediately and might collide with ongoing transmissions on the DALI bus.

    'X'     : command code
