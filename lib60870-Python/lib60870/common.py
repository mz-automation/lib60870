import logging
from lib60870 import lib60870
import ctypes
from ctypes import c_int, c_uint64, c_void_p, c_bool, c_uint8

lib = lib60870.get_library()

logger = logging.getLogger(__name__)


class BinaryCounterReading(ctypes.Structure):
    _fields_ = [("encodedValue", c_uint8 * 5)]
pBinaryCounterReading = ctypes.POINTER(BinaryCounterReading)


class ConnectionParameters(ctypes.Structure):
    _fields_ = [
        ("sizeOfTypeId", c_int),
        ("sizeOfVSQ", c_int),
        ("sizeOfCOT", c_int),
        ("originatorAddress", c_int),
        ("sizeOfCA", c_int),
        ("sizeOfIOA", c_int)
        ]

    def __repr__(self):
        output = "{}(".format(type(self).__name__)
        output += ", ".join(["{}={}".format(field[0],  getattr(self, field[0])) for field in self._fields_])
        return output + ")"

    @property
    def pointer(self):
        return pConnectionParameters(self)

pConnectionParameters = ctypes.POINTER(ConnectionParameters)


default_connection_parameters = ConnectionParameters(
    sizeOfTypeId=1,
    sizeOfVSQ=1,
    sizeOfCOT=2,
    originatorAddress=0,
    sizeOfCA=2,
    sizeOfIOA=3)
