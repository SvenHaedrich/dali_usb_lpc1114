import logging
import time

import pytest
from dali_interface.dali_interface import DaliStatus
from dali_interface.serial import DaliSerial

logger = logging.getLogger(__name__)
timeout_time_sec = 2
# doc/commands.md asks for 0.2 ms between commands; the suite keeps a margin
time_for_command_processing = 0.0005


def test_version():
    """`?` is a command like any other and carries its EOL, doc/commands.md.

    The documented answer is two lines, the banner and the version, and one
    command has to produce exactly one of them.
    """
    serial = DaliSerial("/dev/ttyUSB0", start_receive=False)
    serial.port.reset_input_buffer()
    serial.port.write("?\r".encode("ascii"))
    deadline = time.time() + timeout_time_sec
    raw = b""
    while time.time() < deadline:
        raw += serial.port.read(serial.port.in_waiting or 1)
    serial.close()

    lines = raw.decode("ascii", errors="replace").splitlines()
    logger.debug(f"information message: {lines}")
    banners = [line for line in lines if line.startswith("DALI USB interface")]
    versions = [line for line in lines if line.startswith("Version ")]
    assert len(banners) == 1, f"one command, one banner, got {banners}"
    assert len(versions) == 1, f"one command, one version line, got {versions}"

    major, minor, bugfix = (int(part) for part in versions[0].split()[1].split("."))
    logger.debug(f"found Version information {major}.{minor}.{bugfix}")
    assert major == 3
    assert minor == 7
    assert bugfix >= 0


@pytest.mark.parametrize(
    "command,expected_result,detailed_code",
    [
        ("Sx\r", DaliStatus.INTERFACE, 0xA3),
        ("Y200\r", DaliStatus.INTERFACE, 0xA3),
        ("S0 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 A3CD\r", DaliStatus.LOOPBACK, 0x10),
        ("S7 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S8 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S9 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S2 10 11111\r", DaliStatus.INTERFACE, 0xA3),
        # an argument that is missing must not be read from whatever follows it
        ("S1\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10\r", DaliStatus.INTERFACE, 0xA3),
        ("Q1 10\r", DaliStatus.INTERFACE, 0xA3),
        ("R1 1 10\r", DaliStatus.INTERFACE, 0xA3),
        ("Y\r", DaliStatus.INTERFACE, 0xA3),
        # the largest value a frame can carry is (1 << bits) - 1
        ("S1 4 10\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 8 100\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 10000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 20 100000000\r", DaliStatus.INTERFACE, 0xA3),
        # characters the documented grammar does not allow
        ("S1 10x1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 1000junk\r", DaliStatus.INTERFACE, 0xA3),
        ("S0x1 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S+1 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10  1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 1 0 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 1000+\r", DaliStatus.INTERFACE, 0xA3),
        ("S1 10 -1000\r", DaliStatus.INTERFACE, 0xA3),
        ("Q1 10x1000\r", DaliStatus.INTERFACE, 0xA3),
        # doc/commands.md gives Q a priority of 1..5, there is no back to back query
        ("Q6 10 FF00\r", DaliStatus.INTERFACE, 0xA3),
        # a query is never sent twice - send the forward frame explicitly instead
        ("Q1 10+FF00\r", DaliStatus.INTERFACE, 0xA3),
        ("Y10junk\r", DaliStatus.INTERFACE, 0xA3),
        ("R1 1 10 1000junk\r", DaliStatus.INTERFACE, 0xA3),
        ("R1  1 10 1000\r", DaliStatus.INTERFACE, 0xA3),
        ("Wzz\r", DaliStatus.INTERFACE, 0xA3),
        ("N1a1junk\r", DaliStatus.INTERFACE, 0xA3),
        ("I5\r", DaliStatus.INTERFACE, 0xA3),
        ("X5\r", DaliStatus.INTERFACE, 0xA3),
    ],
)
def test_bad_parameter(dali_serial, command, expected_result, detailed_code):
    dali_serial.port.write(command.encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == expected_result
    assert result.length == detailed_code


def test_send_twice_is_still_accepted_for_s(dali_serial):
    """`+` stays legal on S - only Q rejects it."""
    dali_serial.port.write("S1 10+A3CD\r".encode("ascii"))
    for _ in range(2):
        result = dali_serial.get(timeout_time_sec)
        assert result.status == DaliStatus.LOOPBACK
        assert result.length == 0x10
        assert result.data == 0xA3CD


def test_a_query_sent_twice_is_refused(dali_serial):
    """The repeated query never armed its backward frame timeout, so it
    answered nothing at all. The combination is refused now."""
    dali_serial.port.write("Q1 10+FF00\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA3
    # a plain query still answers
    dali_serial.port.write("Q1 10 FF00\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMEOUT


def test_input_queue(dali_serial):
    cmd_one = "S1 10 FF01\r"
    cmd_two = "S1 10 FF02\r"
    dali_serial.port.write(cmd_one.encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write(cmd_two.encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 0x10
    assert result.data == 0xFF01
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 0x10
    assert result.data == 0xFF02


def test_queue_overflow(dali_serial):
    """More frames than the command queue holds answers 0xA2, doc/messages.md."""
    time.sleep(timeout_time_sec)
    test_cmd = "S1 10 FF0A\r"
    queue_size = 5
    overfill = 5
    for _ in range(queue_size + overfill):
        dali_serial.port.write(test_cmd.encode("ascii"))
        time.sleep(time_for_command_processing)
    for _ in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
        if result.status == DaliStatus.LOOPBACK:
            continue
        break
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA2
    # read until no message is left
    for _ in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMEOUT


def test_commands_sent_too_fast_are_refused(dali_serial):
    """doc/commands.md asks for 0.2 ms between commands and promises 0xA2 when
    that is not honoured. A whole sequence written in one go honours nothing.
    """
    dali_serial.flush_queue()
    dali_serial.port.write(b"W1a1\r" + b"N1a1\r" * 16 + b"X\r")

    codes = []
    for _ in range(8):
        result = dali_serial.get(timeout_time_sec)
        if "queue is empty" in result.message:
            break
        codes.append(result.length)
    assert 0xA2 in codes, (
        f"expected a full command queue, got {[hex(c) for c in codes]}"
    )

    # and the adapter carries on
    time.sleep(0.2)
    dali_serial.flush_queue()
    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0x5A5A


def test_sequence_execute_without_definition(dali_serial):
    """`X` on its own must be refused instead of replaying the last frame.

    `tx.index_max` counts the phases in the transmit buffer and says nothing
    about whether they are a sequence or the frame that was sent last, so `X`
    used to put the previous DALI command back on the bus.
    """
    dali_serial.port.write("S1 10 ABCD\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0xABCD

    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_a_sequence_command_cuts_a_frame_in_flight(dali_serial):
    """`W` stops an ongoing transmission immediately, doc/commands.md.

    The frame on the wire is left corrupt and reported as such, so the loopback
    the host would otherwise have received never arrives. The document does not
    name the code, so the test only asks that the frame did not survive intact.
    """
    dali_serial.flush_queue()
    dali_serial.port.write("S1 20 12345678\r".encode("ascii"))
    time.sleep(0.010)  # a 32 bit frame occupies the bus for about 30 ms
    dali_serial.port.write("W1a1\r".encode("ascii"))

    result = dali_serial.get(timeout_time_sec)
    survived = (
        result.status == DaliStatus.LOOPBACK
        and result.length == 0x20
        and result.data == 0x12345678
    )
    assert not survived, "the sequence command did not cut the frame"

    # discard whatever the interrupted frame left behind, and prove the bus works
    time.sleep(0.1)
    dali_serial.flush_queue()
    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0x5A5A


def test_a_sequence_runs_after_it_cut_a_frame(dali_serial):
    """Cutting a frame must not cost the sequence itself.

    `W` is allowed to interrupt the frame ahead of it, but the sequence it
    starts still has to be defined, executed and reported.
    """
    dali_serial.flush_queue()
    for command in ("S1 10 1000", "W1a1", "N1a1", "X"):
        dali_serial.port.write(f"{command}\r".encode("ascii"))
        time.sleep(time_for_command_processing)

    # one phase survives `X`, so the sequence puts a start bit and nothing else
    # on the bus and comes back as a frame of zero data bits
    for _ in range(4):
        result = dali_serial.get(timeout_time_sec)
        if result.status == DaliStatus.LOOPBACK and result.length == 0x00:
            break
    else:
        raise AssertionError("the sequence was never executed")


def test_an_empty_line_is_ignored(dali_serial):
    """A bare EOL is not a command and answers nothing at all.

    The receive index was reset when a line ended and then advanced again by the
    shared tail, so an empty line was terminated at index 1 and index 0 still
    held the command letter of the line before last. Only `X` and `I` take no
    arguments, so those are the two that were executed a second time - and `I`
    put a corrupt backward frame on the bus for every empty line.
    """
    dali_serial.flush_queue()
    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    for _ in range(4):
        dali_serial.port.write("\r".encode("ascii"))
        time.sleep(time_for_command_processing)
    result = dali_serial.get(0.5)
    assert "queue is empty" in result.message, f"an empty line was answered: {result}"

    # `I` is the dangerous one, it reaches the bus
    dali_serial.port.write("I\r".encode("ascii"))
    dali_serial.get(timeout_time_sec)
    dali_serial.flush_queue()

    for _ in range(4):
        dali_serial.port.write("\r".encode("ascii"))
        time.sleep(time_for_command_processing)
    result = dali_serial.get(0.5)
    assert "queue is empty" in result.message, (
        f"an empty line put a frame on the bus: {result}"
    )


def test_sequence_next_without_start(dali_serial):
    """`N` without a preceding `W` has no sequence to add to."""
    dali_serial.port.write("N64\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_sequence_accepts_sixty_six_steps(dali_serial):
    """doc/commands.md allows 66 `N` steps per transmission."""
    dali_serial.flush_queue()
    dali_serial.port.write("W1a4\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    for _ in range(66):
        dali_serial.port.write("N1a4\r".encode("ascii"))
        time.sleep(time_for_command_processing)

    result = dali_serial.get(0.5)
    assert "queue is empty" in result.message, f"a step was refused: {result}"

    # leave nothing behind for the next test
    dali_serial.port.write("X\r".encode("ascii"))
    time.sleep(0.2)
    dali_serial.flush_queue()


def test_sequence_refuses_the_sixty_seventh_step(dali_serial):
    """The step after the documented limit is refused, and takes the sequence
    with it so that no truncated form can be sent."""
    dali_serial.flush_queue()
    dali_serial.port.write("W1a4\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    for _ in range(66):
        dali_serial.port.write("N1a4\r".encode("ascii"))
        time.sleep(time_for_command_processing)
    dali_serial.flush_queue()

    dali_serial.port.write("N1a4\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_rejected_sequence_does_not_stall_the_transmitter(dali_serial):
    """Reporting a rejected command must not disturb frame reception.

    The report used to travel through `queue_error_frame()`, which leaves
    `rx.status` at `ERROR_IN_FRAME`. Called from the SERIAL task that stopped
    `rx_schedule_transmission()` from ever starting a transmission again, so two
    bytes were enough to silence the adapter until it was reset.
    """
    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE

    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0x5A5A


@pytest.mark.parametrize(
    "period",
    [
        "1",  # underflows the rise and fall compensation
        "17",  # 23 us, the largest value that still underflows
        "18",  # 24 us, exactly the compensation, leaves a match count that never advances
    ],
)
def test_sequence_period_too_short_is_refused(dali_serial, period):
    """A period the transmitter cannot express must be refused, not executed.

    `add_signal_phase()` subtracts the rise and fall time from every even phase
    without checking, so anything at or below that wrapped to a match about 2^32
    microseconds away. The transmit pin is asserted before the first match, so the
    bus stayed held until the adapter was reflashed - long enough to trip the
    failure detection of every device on the segment.
    """
    dali_serial.port.write(f"W{period}\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    # the sequence was never started, so there is nothing to execute
    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    # and the bus is still free
    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0x5A5A


def test_sequence_longer_than_the_counter_is_refused(dali_serial):
    """The counts are absolute and the timer does not roll over.

    A single period is only bounded by the counter, so each of these fits on its
    own; it is the running total that does not. The phase that would take the
    sequence past the end of the counter has to be refused.
    """
    dali_serial.port.write("W7fffffff\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("N7fffffff\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.flush_queue()

    dali_serial.port.write("N7fffffff\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    # the sequence went with it, so there is nothing left to execute
    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_sequence_next_out_of_range_invalidates_the_sequence(dali_serial):
    """A bad `N` must take the whole sequence with it, not just report itself."""
    dali_serial.port.write("W1a1\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("N1\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0

    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_shortest_usable_sequence_period_is_accepted(dali_serial):
    """One microsecond above the compensation still has to be sent."""
    dali_serial.port.write("W19\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("N19\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("X\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.length != 0xA0


@pytest.mark.parametrize(
    "bits,data",
    [(4, 0xF), (8, 0xFF), (0x10, 0xFFFF), (0x18, 0xFFFFFF), (0x20, 0xFFFFFFFF)],
)
def test_largest_value_per_length(dali_serial, bits, data):
    """(1 << bits) - 1 still fits and has to be accepted."""
    dali_serial.port.write(f"S1 {bits:x} {data:x}\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == bits
    assert result.data == data


def test_truncated_command_does_not_repeat_an_earlier_one(dali_serial):
    """A command that stops early must be refused, not completed from stale bytes.

    The receive buffers alternate and are only terminated, never cleared, so
    reading past the terminator picks up the command from two back.
    """
    for data in (0xABCD, 0x1234):
        dali_serial.port.write(f"S1 10 {data:04x}\r".encode("ascii"))
        result = dali_serial.get(timeout_time_sec)
        assert result.status == DaliStatus.LOOPBACK
        assert result.data == data

    dali_serial.port.write("S1 10\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE, (
        "the truncated command was sent as a frame"
    )
    assert result.length == 0xA3


@pytest.mark.parametrize(
    "command",
    [
        "S1 10 00000000001234",  # 20 characters, one too many
        "S1 10 0000000000001234",  # 22, cut inside <data> it used to send 0x0001
        "R1 1 10 00000000001234",
        "W" + "0" * 16 + "186A0",
        "Y" + "0" * 30 + "55",
    ],
)
def test_a_command_longer_than_19_characters_is_refused(dali_serial, command):
    """A command line holds 19 characters, doc/commands.md, and a longer one
    answers 0xA0 and puts nothing on the bus.

    The line used to be cut at 19 characters. A cut inside a hex field still
    parsed, so a different frame went out and the host was told nothing.
    """
    assert len(command) > 19
    dali_serial.flush_queue()
    dali_serial.port.write(f"{command}\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE, f"{command} was executed: {result}"
    assert result.length == 0xA0

    result = dali_serial.get(0.3)
    assert "queue is empty" in result.message, f"{command} reached the bus: {result}"


def test_a_command_of_19_characters_is_accepted(dali_serial):
    """The longest line that fits is still a command."""
    command = "S1 10 0000000001234"
    assert len(command) == 19
    dali_serial.flush_queue()
    dali_serial.port.write(f"{command}\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 0x10
    assert result.data == 0x1234


def test_the_command_after_an_overlong_one_is_executed(dali_serial):
    """Refusing a long line must not cost the line that follows it."""
    dali_serial.flush_queue()
    dali_serial.port.write(("S1 10 " + "0" * 100 + "1234\r").encode("ascii"))
    time.sleep(time_for_command_processing)
    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))

    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.data == 0x5A5A
