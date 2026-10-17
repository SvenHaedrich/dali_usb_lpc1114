"""A frame queued behind an error frame must still reach the bus.

The adapter is the only transmitter on the bench, so nothing on the bus releases
a transmitter that locks up here. Only a `W` that reaches the head of the command
queue does, and every test here starts with one. A frame command written after a
failure is held behind the locked frame instead, so this file runs late.
"""

import logging
import time

import pytest
from dali_interface.dali_interface import DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2
time_for_command_processing = 0.0005

# the last period is a 100 us active phase, so the receiver reports the
# sequence as a data timing error, 0x83
BAD_SEQUENCE = ("W1a1", "N1a1", "N64", "X")
BACKWARD_SETTLING_SEC = 0.0055
BAD_SEQUENCE_SEC = 0.0009


def send_behind_an_error_frame(dali_serial, command):
    dali_serial.flush_queue()
    time.sleep(0.1)
    for line in BAD_SEQUENCE + (command,):
        dali_serial.port.write(f"{line}\r".encode("ascii"))
        time.sleep(time_for_command_processing)

    error = dali_serial.get(timeout_time_sec)
    assert error.status == DaliStatus.TIMING, (
        f"the sequence was not reported as an error: {error}"
    )
    return error


def assert_transmitter_alive(dali_serial):
    dali_serial.flush_queue()
    dali_serial.port.write("S1 10 5A5A\r".encode("ascii"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK, f"the transmitter is locked: {result}"
    assert result.data == 0x5A5A


def test_a_backward_frame_behind_an_error_frame_is_sent(dali_serial):
    """`Y` is handed over while the receiver is still inside the error frame.

    It used to be timed from the last frame that was received intact, which
    lay in the past, and the settling match was set about 71 minutes ahead.
    """
    error = send_behind_an_error_frame(dali_serial, "Y55")

    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK, f"Y was never sent: {result}"
    assert result.length == 8
    assert result.data == 0x55
    # timestamps are whole milliseconds, so allow one of them to be lost
    delta = result.timestamp - error.timestamp
    logger.debug(f"Y started {delta * 1000:.0f} ms after the error frame")
    assert delta >= BAD_SEQUENCE_SEC + BACKWARD_SETTLING_SEC - 0.001, (
        "Y did not wait for its settling time"
    )
    assert delta < 0.020

    assert_transmitter_alive(dali_serial)


def test_a_corrupt_frame_behind_an_error_frame_is_sent(dali_serial):
    """`I` takes the same path, and owes the same settling time as `Y`.

    A corrupt backward frame is still a backward frame, doc/commands.md.
    """
    error = send_behind_an_error_frame(dali_serial, "I")

    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.TIMING, f"I was never sent: {result}"
    # the receiver reports the stretched phase of the corrupt frame, about 1500 us
    assert 1400 < (result.data >> 8) < 1600, f"not the corrupt frame: {result}"
    delta = result.timestamp - error.timestamp
    logger.debug(f"I started {delta * 1000:.0f} ms after the error frame")
    assert delta >= BAD_SEQUENCE_SEC + BACKWARD_SETTLING_SEC - 0.001, (
        "I did not wait for its settling time"
    )
    assert delta < 0.020

    assert_transmitter_alive(dali_serial)


@pytest.mark.parametrize("command", ["S1 10 7777", "S5 10 7777"])
def test_a_forward_frame_behind_an_error_frame_is_sent(dali_serial, command):
    """The reference: forward frames are timed from the last edge and never stalled."""
    send_behind_an_error_frame(dali_serial, command)

    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK, f"{command} was never sent: {result}"
    assert result.data == 0x7777

    assert_transmitter_alive(dali_serial)
