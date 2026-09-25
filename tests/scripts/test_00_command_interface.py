import logging
import time

import pytest
from dali_interface.dali_interface import DaliStatus
from dali_interface.serial import DaliSerial

logger = logging.getLogger(__name__)
timeout_time_sec = 2
time_for_command_processing = 0.0005


def test_version():
    serial = DaliSerial("/dev/ttyUSB0", start_receive=False)
    command = "?"
    serial.port.write(command.encode("ascii"))
    timeout = time.time() + timeout_time_sec
    while time.time() < timeout:
        if serial.port.inWaiting() >= 0:
            line = serial.port.readline()
            logger.debug(f"read line: {line}")
            if line.find(b"Version") == 0:
                line.decode("ascii", errors="replace")
                try:
                    major = line[8] - ord("0")
                    minor = line[10] - ord("0")
                    bugfix = line[12] - ord("0")
                    logger.debug(f"found Version information {major}.{minor}.{bugfix}")
                    break
                except ValueError:
                    continue
    assert major == 3
    assert minor == 7
    assert bugfix >= 0
    serial.close()


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
    ],
)
def test_bad_parameter(dali_serial, command, expected_result, detailed_code):
    dali_serial.port.write(command.encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == expected_result
    assert result.length == detailed_code


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
    time.sleep(timeout_time_sec)
    test_cmd = "S1 10 FF0A\r"
    queue_size = 5
    overfill = 5
    for i in range(queue_size + overfill):
        dali_serial.port.write(test_cmd.encode("ascii"))
        time.sleep(time_for_command_processing)
    for i in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
        if result.status == DaliStatus.LOOPBACK:
            continue
        break
    assert result.status == DaliStatus.INTERFACE
    # read until no message is left
    for i in range(queue_size + overfill):
        result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMEOUT


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


def test_sequence_next_without_start(dali_serial):
    """`N` without a preceding `W` has no sequence to add to."""
    dali_serial.port.write("N64\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.INTERFACE
    assert result.length == 0xA0


def test_overflowed_sequence_is_not_executed(dali_serial):
    """A sequence that did not fit must not be sent in its truncated form."""
    dali_serial.port.write("W1a4\r".encode("ascii"))
    time.sleep(time_for_command_processing)
    for _ in range(70):
        dali_serial.port.write("N1a4\r".encode("ascii"))
        time.sleep(time_for_command_processing)
    time.sleep(0.2)
    dali_serial.flush_queue()

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
