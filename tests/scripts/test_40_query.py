import pytest
import logging

from dali_interface.dali_interface import DaliFrame, DaliStatus

logger = logging.getLogger(__name__)
timeout_time_sec = 2


def test_queries(dali_serial) -> None:
    test_cmd_1 = 0xFF01
    test_frame_1 = DaliFrame(length=16, data=test_cmd_1)
    result = dali_serial.query_reply(test_frame_1)
    assert result.status == DaliStatus.TIMEOUT
    test_cmd_2 = 0xFF02
    test_frame_2 = DaliFrame(length=16, data=test_cmd_2)
    dali_serial.transmit(test_frame_2, block=False)
    result = dali_serial.get(timeout_time_sec)
    assert result.status == DaliStatus.LOOPBACK
    assert result.length == 16
    assert result.data == test_cmd_2
