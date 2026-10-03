import logging
import time

import pytest
from dali_interface.dali_interface import DaliFrame, DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def test_queries(dali_serial) -> None:
    test_cmd_1 = 0xFF01
    test_frame_1 = DaliFrame(length=16, data=test_cmd_1)
    result = dali_serial.query_reply(test_frame_1)
    assert result.status == DaliStatus.TIMEOUT
    assert result.message == "TIMEOUT"
    test_cmd_2 = 0xFF02
    test_frame_2 = DaliFrame(length=16, data=test_cmd_2)
    dali_serial.transmit(test_frame_2, block=False)
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 16
    assert result.data == test_cmd_2


def collect(dali_serial):
    frames = []
    while True:
        result = dali_serial.get(0.3)
        if "queue is empty" in result.message:
            return frames
        frames.append(result)


@pytest.mark.parametrize(
    "follower,is_answer",
    [
        # settling time on the bus from the query's last edge, Figure 13
        ("S6 10 1000", True),  # 2,45 ms
        ("Y55", True),  # 5,5 ms
        ("I", True),  # 5,5 ms
        ("S1 10 1000", False),  # 13,5 ms
        ("R1 1 10 1000", False),  # 13,5 ms
        ("Q1 10 FF02", False),  # 13,5 ms
        ("S2 10 1000", False),  # 14,9 ms
        ("S3 10 1000", False),  # 16,3 ms
    ],
)
def test_a_frame_answers_a_query_only_inside_the_backward_window(
    dali_serial, follower, is_answer
):
    """IEC 62386-101:2022 8.2.5: a frame that starts within 13,4 ms settling time
    after a query is its backward frame, even a corrupt one. A later frame is not,
    and the query has to time out before it starts.
    """
    dali_serial.flush_queue()
    dali_serial.port.write(b"Q1 10 FF01\r")
    deadline = time.perf_counter() + 0.0005
    while time.perf_counter() < deadline:
        pass
    dali_serial.port.write(f"{follower}\r".encode("ascii"))
    frames = collect(dali_serial)
    shape = [(f.status, hex(f.length), hex(f.data)) for f in frames]

    assert frames[0].status == DaliStatus.LOOPBACK and frames[0].data == 0xFF01, shape
    if is_answer:
        # the answer is reported as it was received, I as a 0x83 timing error
        assert len(frames) == 2 and frames[1].message != "TIMEOUT", shape
    else:
        assert (
            frames[1].status == DaliStatus.TIMEOUT and frames[1].message == "TIMEOUT"
        ), shape
        assert frames[2].status == DaliStatus.LOOPBACK, shape


def test_a_query_cut_by_a_sequence_does_not_time_out(dali_serial):
    """W stops the query on the wire, so nothing is left to time out."""
    dali_serial.flush_queue()
    for command in ("Q1 10 FF01", "W1a1", "N1a1", "X"):
        dali_serial.port.write(f"{command}\r".encode("ascii"))
        deadline = time.perf_counter() + 0.0005
        while time.perf_counter() < deadline:
            pass
    frames = collect(dali_serial)
    shape = [(f.status, hex(f.length), hex(f.data)) for f in frames]

    assert frames, shape
    assert not any(f.length == 16 and f.data == 0xFF01 for f in frames), (
        f"the query was not cut: {shape}"
    )
    assert all(f.message != "TIMEOUT" for f in frames), shape
