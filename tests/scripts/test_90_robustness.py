"""Robustness tests that can leave the adapter locked up.

These are kept apart from the rest of the suite and marked `destructive`, so a
normal `./run_tests.sh` deselects them. A failure here is not a red test that the
next test recovers from: the adapter stops answering and someone has to power
cycle it, which would make every later test fail for the wrong reason.

Run them deliberately, on their own:

    ./run_tests.sh -m destructive

The tests talk to the port directly instead of using `DaliSerial`. They provoke
output message corruption on purpose, and `DaliSerial.parse()` raises
`UnboundLocalError` out of its receive thread on a malformed line, which would
show up here as a false "adapter is dead".
"""

import logging
import re
import time

import pytest
import serial  # type: ignore

logger = logging.getLogger(__name__)

PORTNAME = "/dev/ttyUSB0"
BAUDRATE = 500000

# an output message, see doc/messages.md
MESSAGE = re.compile(rb"^\{[0-9a-f]{8}[:>][0-9a-f]{2} [0-9a-f]{8}\}$")
LOOPBACK_INDEX = 9
DATA_SLICE = slice(13, 21)
STATUS_SLICE = slice(10, 12)

# 64 repetitions keep the transmitter busy for roughly 1.8 seconds
REPEAT_COMMAND = b"R1 40 10 5555\r"
EXPECTED_FRAMES = 0x40 + 1
# a command that cannot be parsed, every one of them costs the SERIAL task an
# error message
BAD_COMMAND = b"Sx\r"
BAD_COMMAND_COUNT = 4500
# a burst that ends while the MAIN task is still starved, so that the frames
# dropped at its tail have no later frame behind them
SHORT_REPEAT_COMMAND = b"R1 c 10 5555\r"
SHORT_EXPECTED_FRAMES = 0xC + 1
TAIL_BAD_COMMAND_COUNT = 6000
PROBE_COMMAND = b"S1 10 A5A5\r"
PROBE_DATA = 0xA5A5
DALI_QUEUE_FULL = 0xA5


def drain(port, duration):
    """read and discard for a fixed time"""
    raw = b""
    end = time.time() + duration
    while time.time() < end:
        raw += port.read(port.in_waiting or 1)
    return raw


def drain_until_quiet(port, quiet_sec=1.5, limit_sec=30.0):
    """read until nothing arrives for quiet_sec, or limit_sec has passed"""
    raw = b""
    deadline = time.time() + limit_sec
    last_input = time.time()
    while time.time() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            raw += chunk
            last_input = time.time()
        elif (time.time() - last_input) > quiet_sec:
            break
    return raw


def messages(raw):
    return [line for line in raw.replace(b"\r", b"\n").split(b"\n") if line.strip()]


def loopback_frames(lines):
    return [
        line
        for line in lines
        if MESSAGE.match(line) and line[LOOPBACK_INDEX : LOOPBACK_INDEX + 1] == b">"
    ]


def is_responsive(port, attempts=3):
    """send a forward frame and look for its loopback message"""
    for attempt in range(attempts):
        drain(port, 0.3)
        port.write(PROBE_COMMAND)
        port.flush()
        for line in loopback_frames(messages(drain(port, 1.0))):
            if int(line[DATA_SLICE], 16) == PROBE_DATA:
                return True
        logger.debug(f"no loopback message on attempt {attempt + 1}")
    return False


@pytest.fixture
def port():
    port = serial.Serial(PORTNAME, BAUDRATE, timeout=0.1)
    yield port
    port.close()


@pytest.mark.destructive
def test_survives_rx_queue_overload(port):
    """The adapter has to stay alive when the MAIN task cannot drain the queue.

    `main_task` is the only reader of the DALI receive queue and runs at the
    lowest priority. Two things starve it:

    * a repeated frame keeps the transmitter busy without the MAIN task, the
      repeat loop is driven from the DALI RX task (`manage_tx`)
    * every rejected command makes the SERIAL task, which has a higher priority,
      busy wait in an unbuffered `printf` for about 440 microseconds

    The DALI RX task has the highest priority and keeps writing the loopback
    frames into a queue that holds five entries. Frame six finds the queue full.
    `queue_frame()` in `dali_101_rx.c` has to survive that and report status
    `0xA5`, instead of running into `configASSERT(false)` which lights all LEDs
    and loops forever - the adapter would be gone until it is power cycled.

    Both halves are checked: the adapter still answers, and the loss was
    reported. Staying alive while dropping frames in silence is the behaviour
    this test exists to catch.
    """
    assert is_responsive(port), "adapter does not answer before the test starts"

    port.write(REPEAT_COMMAND)
    port.flush()
    time.sleep(0.1)

    port.write(BAD_COMMAND * BAD_COMMAND_COUNT)
    port.flush()
    logger.info(f"sent {BAD_COMMAND_COUNT} commands that cannot be parsed")

    lines = messages(drain_until_quiet(port))
    frames = loopback_frames(lines)
    reported_full = [
        line
        for line in lines
        if MESSAGE.match(line) and int(line[STATUS_SLICE], 16) == DALI_QUEUE_FULL
    ]
    logger.info(
        f"{len(frames)} frames reported, "
        f"{len(reported_full)} of them report a full receive queue"
    )

    assert is_responsive(port), (
        "THE ADAPTER IS LOCKED UP AND NEEDS A MANUAL POWER CYCLE. "
        "The DALI receive queue overflowed while the MAIN task was starved and "
        "queue_frame() in dali_101_rx.c ran into configASSERT(false) instead of "
        f"reporting status 0x{DALI_QUEUE_FULL:02X}."
    )

    assert len(frames) < EXPECTED_FRAMES, (
        f"all {EXPECTED_FRAMES} frames came back, so the queue never ran full and "
        "the test proved nothing. The MAIN task is no longer starved - raise "
        "BAD_COMMAND_COUNT, or drop this test if the starvation is gone for good."
    )

    assert reported_full, (
        f"{EXPECTED_FRAMES - len(frames)} frames were lost and not one message "
        f"reports status 0x{DALI_QUEUE_FULL:02X}. The adapter survived but kept the "
        "loss to itself, which leaves the host unable to tell that its stream has "
        "a gap."
    )


@pytest.mark.destructive
def test_reports_loss_at_the_end_of_a_burst(port):
    """A frame dropped as the last one on the bus still has to be reported.

    The overflow is noticed inside the DALI RX task, and everything that writes
    to the queue is driven by bus activity. A report that waits for the next
    frame to come along is never sent when there is no next frame: the bus falls
    quiet, the MAIN task drains the queue and the host is left with a gap it
    cannot see. `queue_frame_for_send()` therefore keeps one queue slot free and
    reports the loss straight away.

    The burst here is short and the flood outlasts it, so the last frames are
    dropped and nothing follows them.
    """
    assert is_responsive(port), "adapter does not answer before the test starts"

    port.write(SHORT_REPEAT_COMMAND)
    port.flush()
    time.sleep(0.1)
    port.write(BAD_COMMAND * TAIL_BAD_COMMAND_COUNT)
    port.flush()

    lines = messages(drain_until_quiet(port))
    frames = loopback_frames(lines)
    reported_full = [
        line
        for line in lines
        if MESSAGE.match(line) and int(line[STATUS_SLICE], 16) == DALI_QUEUE_FULL
    ]
    lost = SHORT_EXPECTED_FRAMES - len(frames)
    logger.info(
        f"{len(frames)} frames reported, {lost} lost, {len(reported_full)} reported full"
    )

    assert lost > 0, (
        f"all {SHORT_EXPECTED_FRAMES} frames came back, so the queue never ran full "
        "and the test proved nothing. Raise TAIL_BAD_COMMAND_COUNT."
    )

    assert reported_full, (
        f"{lost} frames were dropped at the end of the burst and never reported. "
        "The overflow is only remembered and waits for a frame that never comes."
    )
