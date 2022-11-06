import pytest
import logging
import time

logger = logging.getLogger(__name__)

def read_one_frame(serial_conn, expected_data):
    reply = serial_conn.readline()
    start = reply.find(ord('{'))+1
    end = reply.find(ord('}'))
    assert start > 0
    assert end > 0
    payload = reply[start:end]
    timestamp = int(payload[0:8], 16) / 1000.0
    type = chr(payload[8])
    length = int(payload[9:11], 16)
    data = int(payload[12:20], 16)
    assert type == "-"
    assert length == 16
    assert data == expected_data
    return timestamp

@pytest.mark.parametrize("cmd, settling, data", [
    ("T0 10 FF00\r", 5500, 0xFF00), 
    ("T1 10 00FF\r", 13500, 0x00FF),
    ("T2 10 FF00\r", 14900, 0xFF00),
    ("T3 10 00FF\r", 16300, 0x00FF),
    ("T4 10 FF00\r", 17900, 0xFF00),
    ("T5 10 00FF\r", 19500, 0x00FF),
    ("R0 1 10 FF00\r", 5500, 0xFF00), 
    ("R1 1 10 00FF\r", 13500, 0x00FF),
    ("R2 1 10 FF00\r", 14900, 0xFF00),
    ("R3 1 10 00FF\r", 16300, 0x00FF),
    ("R4 1 10 FF00\r", 17900, 0xFF00),
    ("R5 1 10 00FF\r", 19500, 0x00FF),
    ])
def test_settling_priority(serial_conn, cmd, settling, data):
    serial_conn.write(cmd.encode("utf-8"))
    timestamp_1 = read_one_frame(serial_conn, data)
    timestamp_2 = read_one_frame(serial_conn, data)
    delta = timestamp_2 - timestamp_1
    fullbit_time = 833 / 1000000
    expected_delta = 17 * fullbit_time + (settling / 1000000)
    tolerance = 1 / 1000
    logger.debug(F"delta is {delta} expected is {expected_delta}")
    assert (abs(delta - expected_delta)) < tolerance

@pytest.mark.parametrize("repeat, data", [
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
    (255, 0xFFFF)
])
def test_repeat(serial_conn,repeat,data):
    cmd = F"R0 {repeat:x} 10 {data:x}\r"
    serial_conn.write(cmd.encode("utf-8"))
    for j in range(repeat+1):
        read_one_frame(serial_conn, data)
