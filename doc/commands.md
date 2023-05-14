## Query `Q`

Send a DALI forward frame and report the systems reaction. A backframe message is allways generated. The command syntax is

    'Q' <priority> ' ' <bits> (' '|'+') <data> EOL

    'Q'        : command code
    <priority> : inter frame timing used 1..5
    <bits>     : number of data bits to send 0..32d in hex presentation (0..20)
    ' ' | '+'  : a plus indicates that the forward frame is send twice
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d
- - - 

## Send Frame `S`

Send a DALI forward frame. The command syntax is

    'S' <priority> ' ' <bits> (' '|'+') <data> EOL

    'S'        : command code
    <priority> : inter frame timing used 1..5
    <bits>     : number of data bits to send 0..32d in hex presentation (0..20)
    ' ' | '+'  : a plus indicates that the frame is send twice
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d
- - -

## Repeat Frame `R`

Send an identical DALI repeated times. Note that sending repeated frames twice is not supported. The command syntax is

    'R' <priority> ' ' <repeat> ' ' <bits> ' ' <data> EOL

    'R'        : command code
    <priority> : inter frame timing used 1..5
    <repeat>   : number of repetitions in hex presentation 
    <bits>     : number of data bits to send 0..32d in hex presentation (0..20)
    <data>     : frame data to send in hex presentation
    EOL        : end of line = 0x0d

- - -
## Send Backframe `B`

Send a backframe. The command syntax is

    'B' <value>

    'B'     : command code
    <value> : value to transmit in hex presentation (00..FF)

## Request Status `!`

Request a status frame.
- - - 
## Request Information `?`

Print information about the firmware.
- - -
## Start Sequence `D`

Start the defintion of a sequence.

    'D' <period> EOL

    'D'      : command code
    <period> : time in microseconds, given in hex 
               representation.

- - -
## Next Sequence Step `N`

Continue to define the timing for a sequence.

    'N' <period> EOL

    'N'      : command code
    <period> : time in microseconds, given in hex 
               representation.
- - -
## Exexute Sequence `X`

Execute a defined sequence.
- - -

## Priorities

| Priority | Meaning                   |
|----------|---------------------------|
|        1 | Forward frame priority 1  |
|        2 | Forward frame priority 2  |
|        3 | Forward frame priority 3  |
|        4 | Forward frame priority 4  |
|        5 | Forward frame priority 5  |
