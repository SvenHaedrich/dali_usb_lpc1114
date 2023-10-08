import errno
import logging
import struct
import time

import usb

from .dali_interface import DaliInterface
from .frame import DaliFrame
from .status import DaliStatus

logger = logging.getLogger(__name__)


class DaliUsb(DaliInterface):
    _USB_VENDOR = 0x17B5
    _USB_PRODUCT = 0x0020

    _USB_CMD_INIT = 0x01
    _USB_CMD_BOOTLOADER = 0x02
    _USB_CMD_SEND = 0x12
    _USB_CMD_SEND_ANSWER = 0x15
    _USB_CMD_SET_IO_PINS = 0x20
    _USB_CMD_READ_IO_PINS = 0x21
    _USB_CMD_IDENTIFY = 0x22
    _USB_CMD_POWER = 0x40

    _USB_CTRL_DAPC = 0x04
    _USB_CTRL_DEV_TYPE = 0x80
    _USB_CTRL_SET_DTR = 0x10
    _USB_CTRL_TWICE = 0x20
    _USB_CTRL_ID = 0x40

    _USB_WRITE_TYPE_NO = 0x01
    _USB_WRITE_TYPE_8BIT = 0x02
    _USB_WRITE_TYPE_16BIT = 0x03
    _USB_WRITE_TYPE_25BIT = 0x04
    _USB_WRITE_TYPE_DSI = 0x05
    _USB_WRITE_TYPE_24BIT = 0x06
    _USB_WRITE_TYPE_STATUS = 0x07
    _USB_WRITE_TYPE_17BIT = 0x08

    _USB_READ_MODE_INFO = 0x01
    _USB_READ_MODE_OBSERVE = 0x11
    _USB_READ_MODE_RESPONSE = 0x12

    _USB_READ_TYPE_NO_FRAME = 0x71
    _USB_READ_TYPE_8BIT = 0x72
    _USB_READ_TYPE_16BIT = 0x73
    _USB_READ_TYPE_25BIT = 0x74
    _USB_READ_TYPE_DSI = 0x75
    _USB_READ_TYPE_24BIT = 0x76
    _USB_READ_TYPE_INFO = 0x77
    _USB_READ_TYPE_17BIT = 0x78

    _USB_STATUS_CHECKSUM = 0x01
    _USB_STATUS_SHORTED = 0x02
    _USB_STATUS_FRAME_ERROR = 0x03
    _USB_STATUS_OK = 0x04
    _USB_STATUS_DSI = 0x05
    _USB_STATUS_DALI = 0x06

    def __init__(self, vendor=_USB_VENDOR, product=_USB_PRODUCT):
        super().__init__()
        # lookup devices by _USB_VENDOR and _USB_PRODUCT
        self.interface = 0
        self.send_sequence_number = 1
        self.receive_sequence_number = None

        logger.debug("try to discover DALI interfaces")
        devices = [dev for dev in usb.core.find(find_all=True, idVendor=vendor, idProduct=product)]  # type: ignore

        # if not found
        if devices:
            logger.info(f"DALI interfaces found: {devices}")
        else:
            raise usb.core.USBError("DALI interface not found")

        # use first useable device on list
        i = 0
        while devices[i]:
            self.device = devices[i]
            i = i + 1
            self.device.reset()

            # detach kernel driver if necessary
            if self.device.is_kernel_driver_active(self.interface) is True:
                self.device.detach_kernel_driver(self.interface)

            self.device.set_configuration()
            usb.util.claim_interface(self.device, self.interface)
            cfg = self.device.get_active_configuration()
            interface = cfg[(0, 0)]  # type: ignore

            # get read and write endpoints
            self.ep_write = usb.util.find_descriptor(
                interface,
                custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT,
            )
            self.ep_read = usb.util.find_descriptor(
                interface,
                custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN,
            )
            if not self.ep_read or not self.ep_write:
                logger.info(f"could not determine read or write endpoint on {self.device}")
                continue

            # read pending messages and discard
            try:
                while True:
                    self.ep_read.read(self.ep_read.wMaxPacketSize, timeout=10)  # type: ignore
                    logger.info("DALI interface - disregard pending messages")
            except Exception:
                pass  # nosec B110

            return
        # cleanup
        self.device = None
        self.ep_read = None
        self.ep_write = None
        raise usb.core.USBError("No suitable USB device found!")

    def read_raw(self, timeout=None):
        return self.ep_read.read(self.ep_read.wMaxPacketSize, timeout=timeout)  # type: ignore

    def transmit(self, frame: DaliFrame, block: bool = False, is_query: bool = False):
        command = self._USB_CMD_SEND
        self.send_sequence_number = (self.send_sequence_number + 1) & 0xFF
        sequence = self.send_sequence_number
        control = self._USB_CTRL_TWICE if frame.send_twice else 0
        if frame.length == 24:
            ext = (frame.data >> 16) & 0xFF
            address_byte = (frame.data >> 8) & 0xFF
            opcode_byte = frame.data & 0xFF
            write_type = self._USB_WRITE_TYPE_24BIT
        elif frame.length == 16:
            ext = 0x00
            address_byte = (frame.data >> 8) & 0xFF
            opcode_byte = frame.data & 0xFF
            write_type = self._USB_WRITE_TYPE_16BIT
        elif frame.length == 8:
            ext = 0x00
            address_byte = 0x00
            opcode_byte = frame.data & 0xFF
            write_type = self._USB_WRITE_TYPE_8BIT
            logger.debug(f"time is now: {time.time()}")
        else:
            raise Exception(f"DALI commands must be 8,16 or 24 bit long. This is {frame.length} bit long")

        logger.debug(
            f"DALI>OUT: CMD=0x{command:02X} SEQ=0x{sequence:02X} TYC=0x{write_type:02X} "
            f"EXT=0x{ext:02X} ADR=0x{address_byte:02X} OCB=0x{opcode_byte:02X}"
        )
        buffer = struct.pack(
            "BBBBxBBB" + (64 - 8) * "x",
            command,
            sequence,
            control,
            write_type,
            ext,
            address_byte,
            opcode_byte,
        )
        result = self.ep_write.write(buffer)  # type: ignore
        self.last_transmit = frame.data

        if block:
            if not self.keep_running:
                raise Exception("receive must be active for blocking call to transmit.")
            else:
                self.get_next()
                if self.send_sequence_number != self.receive_sequence_number:
                    raise Exception("expected same sequence number.")
        return result

    def close(self):
        super().close()
        usb.util.dispose_resources(self.device)

    def read_data(self):
        try:
            usb_data = self.read_raw(timeout=100)
            if usb_data:
                read_type = usb_data[1]
                self.receive_sequence_number = usb_data[8]
                logger.debug(
                    f"DALI[IN]: SN=0x{usb_data[8]:02X} TY=0x{usb_data[1]:02X} "
                    f"EC=0x{usb_data[3]:02X} AD=0x{usb_data[4]:02X} OC=0x{usb_data[5]:02X}"
                )
                if read_type == self._USB_READ_TYPE_8BIT:
                    status = DaliStatus(status=DaliStatus.FRAME)
                    length = 8
                    dali_data = usb_data[5]
                    logger.debug(f"backframe at {time.time()}")
                elif read_type == self._USB_READ_TYPE_16BIT:
                    status = DaliStatus(status=DaliStatus.FRAME)
                    length = 16
                    dali_data = usb_data[5] + (usb_data[4] << 8)
                elif read_type == self._USB_READ_TYPE_24BIT:
                    status = DaliStatus(status=DaliStatus.FRAME)
                    length = 24
                    dali_data = usb_data[5] + (usb_data[4] << 8) + (usb_data[3] << 16)
                elif read_type == self._USB_READ_TYPE_NO_FRAME:
                    status = DaliStatus(status=DaliStatus.TIMEOUT)
                    length = 0
                    dali_data = 0
                elif read_type == self._USB_READ_TYPE_INFO:
                    length = 0
                    dali_data = 0
                    if usb_data[5] == self._USB_STATUS_OK:
                        status = DaliStatus(status=DaliStatus.OK)
                    elif usb_data[5] == self._USB_STATUS_FRAME_ERROR:
                        status = DaliStatus(status=DaliStatus.TIMING)
                    else:
                        status = DaliStatus(status=DaliStatus.GENERAL)
                else:
                    return
                self.queue.put(
                    DaliFrame(
                        timestamp=time.time(),
                        length=length,
                        data=dali_data,
                        status=status,
                    )
                )

        except usb.USBError as e:
            if e.errno not in (errno.ETIMEDOUT, errno.ENODEV):
                raise e
