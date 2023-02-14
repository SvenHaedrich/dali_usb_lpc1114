import pytest
import logging
import time

logger = logging.getLogger(__name__)
timeout_time_sec = 2

def test_version(serial_conn):
    command = "?"
    serial_conn.write(command.encode("utf-8"))
    timeout = time.time() + timeout_time_sec 
    while time.time() < timeout:
        if serial_conn.inWaiting() >= 0:
            line = serial_conn.readline()
            if line.find(b"Version")==0:
                logger.debug(F"read line: {line}")
                line.decode("utf-8")
                try:
                    major = line[8] - ord("0")
                    minor = line[10] - ord("0")  
                    bugfix = line[12] - ord("0")
                    logger.debug(F"found Version information {major}.{minor}.{bugfix}")
                except ValueError:
                    continue
    assert major == 1
    assert minor == 5
    assert bugfix >= 4

def test_8bit_loopback(serial_conn):
    for i in range(0,0x100):
        command = F"S1 08 {i:02x}\r"
        serial_conn.write(command.encode("utf-8"))
        reply = serial_conn.readline()
        logger.debug(F"read line: {reply}")
        start = reply.find(ord('{'))+1
        end = reply.find(ord('}'))
        assert start > 0
        assert end > 0
        payload = reply[start:end]
        type = chr(payload[8])
        length = int(payload[9:11], 16)
        data = int(payload[12:20], 16)
        assert type == "-"
        assert length == 8
        assert data == i

@pytest.mark.parametrize("code",[0x0000,0x0001,0x0002,0x0004,0x0008,0x000F,0x0010,0x0020,0x0040,
    0x0080,0x00F0,0x0100,0x0200,0x0400,0x0800,0x0F00,0x1000,0x2000,0x4000,0x8000,0xF000,0x5555,
    0xAAAA,0xFFFF])
def test_16bit_pattern(serial_conn,code):
    command = F"S1 10 {code:04x}\r"
    serial_conn.write(command.encode("utf-8"))
    reply = serial_conn.readline()
    logger.debug(F"read line: {reply}")
    start = reply.find(ord('{'))+1
    end = reply.find(ord('}'))
    assert start > 0
    assert end > 0
    payload = reply[start:end]
    type = chr(payload[8])
    length = int(payload[9:11], 16)
    data = int(payload[12:20], 16)
    assert type == "-"
    assert length == 16
    assert data == code

@pytest.mark.parametrize("code",[0x00000000,0x00000001,0x00000002,0x00000004,
    0x00000008,0x0000000F,0x00000010,0x00000020,0x00000040,0x00000080,0x000000F0,
    0x00000100,0x00000200,0x00000400,0x00000800,0x00000F00,0x00001000,0x00002000,
    0x00004000,0x00008000,0x0000F000,0x00010000,0x00020000,0x00040000,0x00080000,
    0x00100000,0x00200000,0x00400000,0x00800000,0x00F00000,0x01000000,0x02000000,
    0x04000000,0x08000000,0x0F000000,0x10000000,0x20000000,0x40000000,0x80000000,
    0xF0000000,0x55555555,0xAAAAAAAA,0xFFFFFFF])
def test_32bit_pattern(serial_conn,code):
    command = F"S1 20 {code:08x}\r"
    serial_conn.write(command.encode("utf-8"))
    reply = serial_conn.readline()
    logger.debug(F"read line: {reply}")
    start = reply.find(ord('{'))+1
    end = reply.find(ord('}'))
    assert start > 0
    assert end > 0
    payload = reply[start:end]
    type = chr(payload[8])
    length = int(payload[9:11], 16)
    data = int(payload[12:20], 16)
    assert type == "-"
    assert length == 32
    assert data == code
