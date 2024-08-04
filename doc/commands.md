## Serial Parameters

Baudrate: 500,000 Baud \
Data bits: 8 \
Start bit: 1 \
Stop bit: 1 

## Query `Q`

Send a DALI forward frame and report the systems reaction. A backframe message is allways generated.

    'Q' <priority> ' ' <bits> (' '|'+') <data> EOL

    'Q'        : command code
    <priority> : inter frame timing used 1..5 as defined in IEC 62386-101:2022 Table 22. 
                 priority = 6 sends a frame immediately after the stop condition 
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    ' ' | '+'  : a plus indicates that the forward frame is send twice
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d

## Send Frame `S`

Send a DALI forward frame.

    'S' <priority> ' ' <bits> (' '|'+') <data> EOL

    'S'        : command code
    <priority> : inter frame timing used 1..5 as defined in IEC 62386-101:2022 Table 22
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    ' ' | '+'  : a plus indicates that the frame is send twice
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d

## Repeat Frame `R`

Send identical DALI frames repeated times. Note that sending repeated frames twice is not supported.

    'R' <priority> ' ' <repeat> ' ' <bits> ' ' <data> EOL

    'R'        : command code
    <priority> : inter frame timing used 1..5
    <repeat>   : number of repetitions in hex presentation 
    <bits>     : number of data bits to send 0..32 in hex presentation (0..20)
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d

## Send Backward Frame `Y`

Send a backframe.

    'Y' <value>

    'Y'     : command code
    <value> : value to transmit in hex presentation (00..FF)
    EOL     : end of line = 0x0d

## Send Corrupt Backward Frame `I`

Send a corrupt backward frame as described in IEC 62386-101:2022 9.6.2. 

    'I'     : command code
    EOL     : end of line = 0x0d

## Request Information `?`

Print information about the firmware. No end of line character required.

## Start Sequence `W`

Start the defintion of a sequence.

    'W' <period> EOL

    'W'      : command code
    <period> : time in microseconds, given in hex representation.
    EOL      : end of line = 0x0d

## Next Sequence Step `N`

Continue to define the timing for a sequence.

    'N' <period> EOL

    'N'      : command code
    <period> : time in microseconds, given in hex 
               representation.
    EOL      : end of line = 0x0d

## Exexute Sequence `X`

Execute a defined sequence.

    'X'     : command code
    EOL     : end of line = 0x0d

