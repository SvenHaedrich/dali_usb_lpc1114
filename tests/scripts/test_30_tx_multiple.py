import pytest
import logging
from dali_interface.dali_interface import DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2


@pytest.mark.parametrize(
    "cmd, settling, data",
    [
        ("S1 10+00FF\r", 13500, 0x00FF),
        ("S2 10+FF00\r", 14900, 0xFF00),
        ("S3 10+00FF\r", 16300, 0x00FF),
        ("S4 10+FF00\r", 17900, 0xFF00),
        ("S5 10+00FF\r", 19500, 0x00FF),
        ("R1 1 10 00FF\r", 13500, 0x00FF),
        ("R2 1 10 FF00\r", 14900, 0xFF00),
        ("R3 1 10 00FF\r", 16300, 0x00FF),
        ("R4 1 10 FF00\r", 17900, 0xFF00),
        ("R5 1 10 00FF\r", 19500, 0x00FF),
    ],
)
def test_settling_priority(dali_serial, cmd, settling, data):
    dali_serial.port.write(cmd.encode("utf-8"))
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    timestamp_1 = result.timestamp
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    timestamp_2 = result.timestamp
    delta = timestamp_2 - timestamp_1
    fullbit_time = 833 / 1000000
    expected_delta = 17 * fullbit_time + (settling / 1000000)
    tolerance = 1 / 1000
    logger.debug(f"delta is {delta} expected is {expected_delta}")
    assert (abs(delta - expected_delta)) < tolerance


@pytest.mark.parametrize(
    "repeat, data",
    [
        (0, 0x1000),
        (1, 0x1111),
        (2, 0x2020),
        (3, 0x3300),
        (4, 0x4004),
        (8, 0x8000),
        (16, 0xF000),
        (32, 0x1234),
        (64, 0x5FF5),
        (127, 0x1212),
        (255, 0xFFFF),
    ],
)
def test_repeat(dali_serial, repeat, data):
    cmd = f"R1 {repeat:x} 10 {data:x}\r"
    dali_serial.port.write(cmd.encode("utf-8"))
    for j in range(repeat + 1):
        result = dali_serial.get(timeout_time_sec)
        assert result.status == DaliStatus.LOOPBACK
        assert result.data == data
