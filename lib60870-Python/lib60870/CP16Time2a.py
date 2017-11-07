import logging
from lib60870 import lib60870
import ctypes
from ctypes import c_int, c_uint64, c_void_p, c_bool, c_uint8

lib = lib60870.get_library()

logger = logging.getLogger(__name__)


class CP16Time2a(ctypes.Structure):
    _fields_ = [("encodedValue", c_uint8 * 3)]

    def __init__(self):
        self.encodedValue = (c_uint8 * 3)()

    def __repr__(self):
        return "CP24Time2a({})".format(self.get_millisecond())

    def __str__(self):
        return "{:5}".format(
            self.get_minute(),
            self.get_second(),
            self.get_millisecond(),
            "I" if self.is_invalid() else "",
            "S" if self.is_substituted() else "")

    def get_millisecond(self):
        lib.CP16Time2a_getEplapsedTimeInMs.restype = c_int
        return lib.CP16Time2a_getEplapsedTimeInMs(pCP16Time2a(self))

    def set_millisecond(self, value):
        assert(0 <= value < 60000)
        lib.CP16Time2a_setEplapsedTimeInMs(pCP16Time2a(self), c_int(value))
pCP16Time2a = ctypes.POINTER(CP16Time2a)
