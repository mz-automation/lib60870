import logging
from lib60870 import lib60870
import ctypes
from ctypes import c_int, c_uint64, c_void_p, c_bool, c_uint8

lib = lib60870.get_library()

logger = logging.getLogger(__name__)


class CP56Time2a(ctypes.Structure):
    _fields_ = [("encodedValue", c_uint8 * 7)]

    def __init__(self, ms_timestamp=None):
        self.encodedValue = (c_uint8 * 7)()
        if ms_timestamp is not None:
            self.from_timestamp(ms_timestamp)

    def __eq__(self, other):
        a = self.encodedValue
        b = other.encodedValue
        return len(a) == len(b) and all(x == y for x, y in zip(a,b))

    def from_timestamp(self, ms_timestamp):
        lib.CP56Time2a_setFromMsTimestamp(pCP56Time2a(self), c_uint64(ms_timestamp))

    def __repr__(self):
        return "CP56Time2a({})".format(self.to_ms_timestamp())

    def __str__(self):
        return "{:02}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}{}{}{}".format(
            self.get_year(),
            self.get_month(),
            self.get_day_of_month(),
            self.get_hour(),
            self.get_minute(),
            self.get_second(),
            self.get_millisecond(),
            "+01:00" if self.is_summer_time() else "",
            "I" if self.is_invalid() else "",
            "S" if self.is_substituted() else "")

    def set_from_ms_timestamp(self, timestamp):
        lib.CP56Time2a_setFromMsTimestamp(pCP56Time2a(self), c_uint64(timestamp))

    def to_ms_timestamp(self):
        lib.CP56Time2a_toMsTimestamp.restype = c_uint64
        return lib.CP56Time2a_toMsTimestamp(pCP56Time2a(self))

    def get_millisecond(self):
        lib.CP56Time2a_getMillisecond.restype = c_int
        return lib.CP56Time2a_getMillisecond(pCP56Time2a(self))

    def set_millisecond(self, value):
        assert(0 <= value < 1000)
        lib.CP56Time2a_setMillisecond(pCP56Time2a(self), c_int(value))

    def get_second(self):
        lib.CP56Time2a_getSecond.restype = c_int
        return lib.CP56Time2a_getSecond(pCP56Time2a(self))

    def set_second(self, value):
        assert(0 <= value < 60)
        lib.CP56Time2a_setSecond(pCP56Time2a(self), c_int(value))

    def get_minute(self):
        lib.CP56Time2a_getMinute.restype = c_int
        return lib.CP56Time2a_getMinute(pCP56Time2a(self))

    def set_minute(self, value):
        assert(0 <= value < 60)
        lib.CP56Time2a_setMinute(pCP56Time2a(self), c_int(value))

    def get_hour(self):
        lib.CP56Time2a_getHour.restype = c_int
        return lib.CP56Time2a_getHour(pCP56Time2a(self))

    def set_hour(self, value):
        assert(0 <= value < 24)
        lib.CP56Time2a_setHour(pCP56Time2a(self), c_int(value))

    def get_day_of_week(self):
        lib.CP56Time2a_getDayOfWeek.restype = c_int
        return lib.CP56Time2a_getDayOfWeek(pCP56Time2a(self))

    def set_day_of_week(self, value):
        assert(0 < value <= 7)
        lib.CP56Time2a_setDayOfWeek(pCP56Time2a(self), c_int(value))

    def get_day_of_month(self):
        lib.CP56Time2a_getDayOfMonth.restype = c_int
        return lib.CP56Time2a_getDayOfMonth(pCP56Time2a(self))

    def set_day_of_month(self, value):
        assert(0 < value <= 31)
        lib.CP56Time2a_setDayOfMonth(pCP56Time2a(self), c_int(value))

    def get_month(self):
        lib.CP56Time2a_getMonth.restype = c_int
        return lib.CP56Time2a_getMonth(pCP56Time2a(self))

    def set_month(self, value):
        assert(0 < value <= 12)
        lib.CP56Time2a_setMonth(pCP56Time2a(self), c_int(value))

    def get_year(self):
        lib.CP56Time2a_getYear.restype = c_int
        return lib.CP56Time2a_getYear(pCP56Time2a(self))

    def set_year(self, value):
        assert(0 <= value < 100)
        lib.CP56Time2a_setYear(pCP56Time2a(self), c_int(value))

    def is_summer_time(self):
        lib.CP56Time2a_isSummerTime.restype = c_bool
        return lib.CP56Time2a_isSummerTime(pCP56Time2a(self))

    def set_summer_time(self, value):
        lib.CP56Time2a_setSummerTime(pCP56Time2a(self), c_bool(value))

    def is_invalid(self):
        lib.CP56Time2a_isInvalid.restype = c_bool
        return lib.CP56Time2a_isInvalid(pCP56Time2a(self))

    def set_invalid(self, value):
        lib.CP56Time2a_setInvalid(pCP56Time2a(self), c_bool(value))

    def is_substituted(self):
        lib.CP56Time2a_isSubstituted.restype = c_bool
        return lib.CP56Time2a_isSubstituted(pCP56Time2a(self))

    def set_substituted(self):
        lib.CP56Time2a_setSubstituted(pCP56Time2a(self), c_bool(value))

    @property
    def pointer(self):
        return pCP56Time2a(self)

pCP56Time2a = ctypes.POINTER(CP56Time2a)
