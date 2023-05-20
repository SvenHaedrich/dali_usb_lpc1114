import logging

logger = logging.getLogger(__name__)


class DaliMock:
    def __init__(self):
        logger.debug("initialize mock interface")
        self.last_transmit = None
        self.data = None

    def start_receive(self):
        logger.debug("start receive")

    def get_next(self, timeout=None):
        logger.debug("get next")
        return

    def transmit(self, length=0, data=0, priority=1, send_twice=False):
        logger.debug("transmit")
        if send_twice:
            print(f"T{priority} {length:X} {data:X}")
        else:
            print(f"S{priority} {length:X} {data:X}")
        self.last_transmit = data
        self.data = data
        self.length = length

    def close(self):
        logger.debug("close mock interface")
