add_library(board OBJECT 	
	board.c
	led.c
	dali.c
	watchdog.c
)

# the watchdog would reset a core that J-Link holds halted, so only release builds arm it
target_compile_definitions(board PRIVATE $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:BOARD_WATCHDOG>)
