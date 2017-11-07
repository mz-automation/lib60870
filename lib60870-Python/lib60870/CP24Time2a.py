import logging
from lib60870 import lib60870
import ctypes
from ctypes import c_int, c_uint64, c_void_p, c_bool, c_uint8

lib = lib60870.get_library()

logger = logging.getLogger(__name__)


class CP24Time2a(ctypes.Structure):
    _fields_ = [("encodedValue", c_uint8 * 3)]

    def __init__(self):
        self.encodedValue = (c_uint8 * 3)()

    def __repr__(self):
        return "CP24Time2a({})".format(self.to_ms_timestamp())

    def __str__(self):
        return "{:02}:{:02}.{:03}{}{}".format(
            self.get_minute(),
            self.get_second(),
            self.get_millisecond(),
            "I" if self.is_invalid() else "",
            "S" if self.is_substituted() else "")

    def get_millisecond(self):
        lib.CP24Time2a_getMillisecond.restype = c_int
        return lib.CP24Time2a_getMillisecond(pCP24Time2a(self))

    def set_millisecond(self, value):
        assert(0 <= value < 1000)
        lib.CP24Time2a_setMillisecond(pCP24Time2a(self), c_int(value))

    def get_second(self):
        lib.CP24Time2a_getSecond.restype = c_int
        return lib.CP24Time2a_getSecond(pCP24Time2a(self))

    def set_second(self, value):
        assert(0 <= value < 60)
        lib.CP24Time2a_setSecond(pCP24Time2a(self), c_int(value))

    def get_minute(self):
        lib.CP24Time2a_getMinute.restype = c_int
        return lib.CP24Time2a_getMinute(pCP24Time2a(self))

    def set_minute(self, value):
        assert(0 <= value < 60)
        lib.CP24Time2a_setMinute(pCP24Time2a(self), c_int(value))

    def is_invalid(self):
        lib.CP24Time2a_isInvalid.restype = c_bool
        return lib.CP24Time2a_isInvalid(pCP24Time2a(self))

    def set_invalid(self, value):
        lib.CP24Time2a_setInvalid(pCP24Time2a(self), c_bool(value))

    def is_substituted(self):
        lib.CP24Time2a_isSubstituted.restype = c_bool
        return lib.CP24Time2a_isSubstituted(pCP24Time2a(self))

    def set_substituted(self):
        lib.CP24Time2a_setSubstituted(pCP24Time2a(self), c_bool(value))
pCP24Time2a = ctypes.POINTER(CP24Time2a)
