# Changelog

## v4.0.0 - 2026-10-17

- The command interface follows `doc/commands.md` strictly: malformed lines, bad arguments, over-long lines and `?` with trailing text are refused (`0xA0`, `0xA3`), and an empty line no longer replays the last command.
- A command line that arrives before the previous one was processed is reported as `0xA0` instead of lost silently. Leave 0.5 ms between commands (was 0.2 ms).
- `W` and `X` act immediately, and a sequence refuses periods the timer cannot express.
- A full DALI receive queue is reported as `0xA5` instead of hanging the adapter.
- The query timeout `0x81` follows IEC 62386-101:2022 8.2.5, 13.4 ms after the last edge of the query, and `W` cutting a query no longer reports a false `0x81`.
- Settling times: a frame waiting behind an error frame is sent, and `I` waits like a backward frame.
- Serial output is interrupt driven and no longer uses `printf`; status codes the firmware never sends are gone from the interface.
- The PLL runs from the crystal, and the flash uses the documented three wait clocks.
- Release builds arm a 60 s watchdog; Debug builds do not.
- FreeRTOS V11.3.1. The test suite has 219 tests against a real adapter, and its dependencies live in `tests/pyproject.toml`.
