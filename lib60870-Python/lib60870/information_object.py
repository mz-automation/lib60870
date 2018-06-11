from lib60870.common import *
from lib60870.lib60870 import QualityDescriptor
from lib60870.CP16Time2a import CP16Time2a, pCP16Time2a
from lib60870.CP24Time2a import CP24Time2a, pCP24Time2a
from lib60870.CP56Time2a import CP56Time2a, pCP56Time2a

import ctypes
from ctypes import c_int, c_uint16, c_uint32, c_uint64, c_void_p, c_bool, c_uint8, c_void_p, c_float

lib = lib60870.get_library()
logger = logging.getLogger(__name__)

# type aliases
c_enum = c_int
cTypeId = c_enum
DoublePointValue = c_enum
StepCommandValue = c_enum
StartEvent = c_uint8
OutputCircuitInfo = c_uint8
QualifierOfRPC = c_uint8
QualifierOfParameterActivation = c_uint8

EncodeFunction = c_void_p  # TODO
DestroyFunction = ctypes.CFUNCTYPE(None, c_void_p)


class InformationObjectVFP(ctypes.Structure):
    _fields_ = [
        ('encode', EncodeFunction),
        ('destroy', DestroyFunction)
    ]

pInformationObjectVFT = ctypes.POINTER(InformationObjectVFP)


class IOBase():
    """
    Python base class for all InformationObject-derived types
    Contains convenience functions for printing and common
    functions to retrieve the typeID and object address
    """
    def __str__(self):
        return "{}: {}, {}".format(type(self).__name__, self.get_type_id().name, self.get_object_address())

    def __repr__(self):
        output = "{}(".format(type(self).__name__)
        output += ", ".join(["{}={}".format(field[0],  repr(getattr(self, field[0]))) for field in self._fields_])
        return output + ")"

    def __eq__(self, other):
        for field in self._fields_ :
            if field[0] == "virtualFunctionTable":
                continue
            if field not in other._fields_:
                return False
            if not getattr(self, field[0]) == getattr(other, field[0]):
                return False
        return True

    def __del__(self):
        pass

    def get_type_id(self):
        return lib60870.TypeID(self.type)

    def get_object_address(self):
        return self.objectAddress

    def get_pointer_type(self):
        return ctypes.POINTER(type(self))

    def get_value(self):
        return None

    def get_timestamp(self):
        return None

    def clone(self):
        c = self.__class__.__new__(self.__class__)
        for field in c._fields_:
            value = getattr(self, field[0])
            setattr(c, field[0], value)
        return c

    @property
    def pointer(self):
        return self.get_pointer_type()(self)

    # Call library destroy function, only to be used if the InformationObject
    # was allocated by the C-library
    def destroy(self):
        io_p = pInformationObject(self)
        destroy_function = io_p.contents.virtualFunctionTable.contents.destroy
        return destroy_function(io_p)


class StatusAndStatusChangeDetection(ctypes.Structure):
    _fields_ = [
        ('encodedValue', ctypes.c_uint8 * 4)
        ]

    def get_stn(self):
        lib.StatusAndStatusChangeDetection_getSTn.restype = c_uint16
        return lib.StatusAndStatusChangeDetection_getSTn(pStatusAndStatusChangeDetection(self))

    def get_cdn(self):
        lib.StatusAndStatusChangeDetection_getCDn.restype = c_uint16
        return lib.StatusAndStatusChangeDetection_getCDn(pStatusAndStatusChangeDetection(self))

    def set_stn(self, value):
        lib.StatusAndStatusChangeDetection_setSTn(
            pStatusAndStatusChangeDetection(self),
            c_uint16(value))

    def get_st(self, index):
        lib.StatusAndStatusChangeDetection_getST.restype = c_bool
        return lib.StatusAndStatusChangeDetection_getST(
            pStatusAndStatusChangeDetection(self),
            c_int(index))

    def get_cd(self, index):
        lib.StatusAndStatusChangeDetection_getCD.restype = c_bool
        return lib.StatusAndStatusChangeDetection_getCD(
            pStatusAndStatusChangeDetection(self),
            c_int(index))

pStatusAndStatusChangeDetection = ctypes.POINTER(StatusAndStatusChangeDetection)


class SingleEvent(ctypes.c_uint8):
    def set_event_state(self, eventState):
        lib.SingleEvent_setEventState(
            pSingleEvent(self),
            pEventState(eventState))

    def get_event_state(self):
        lib.SingleEvent_getEventState.restype = pEventState
        return lib.SingleEvent_getEventState(pSingleEvent(self))

    def set_qdp(self, qdp):
        lib.SingleEvent_setQDP(
            pSingleEvent(self),
            c_int(qdp))

    def get_qdp(self):
        lib.SingleEvent_getQDP.restype = c_uint8
        value = lib.SingleEvent_getQDP(pSingleEvent(self))
        return QualityDescriptor(value)

pSingleEvent = ctypes.POINTER(SingleEvent)


class InformationObject(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT)
        ]

    def __init__(self, ioa):
        self.objectAddress = ioa
        self.virtualFunctionTable = None
        self.type = lib60870.TypeID.INVALID.c_value

    def get_object_address(self):
        lib.InformationObject_getObjectAddress.restype = c_int
        return lib.InformationObject_getObjectAddress(pInformationObject(self))

    def set_object_address(self, ioa):
        lib.InformationObject_setObjectAddress(
            pInformationObject(self),
            c_int(ioa))

    def encode_base(self, frame, parameters, isSequence):
        lib.InformationObject_encodeBase(
            pInformationObject(self),
            pFrame(frame),
            parameters.pointer,
            c_bool(isSequence))

    def parse_object_address(parameters, msg, startIndex):
        lib.InformationObject_ParseObjectAddress.restype = c_int
        return lib.InformationObject_ParseObjectAddress(
            parameters.pointer,
            ctypes.POINTER(c_uint8)(msg),
            c_int(startIndex))


pInformationObject = ctypes.POINTER(InformationObject)



class SinglePointInformation(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_bool),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, quality):
        self.virtualFunctionTable = ctypes.cast(lib.singlePointInformationVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_SP_NA_1.c_value
        self.create(ioa, value, quality)

    def create(self, ioa, value, quality):
        lib.SinglePointInformation_create.restype = pSinglePointInformation
        return lib.SinglePointInformation_create(
            pSinglePointInformation(self),
            c_int(ioa),
            c_bool(value),
            c_int(quality))

    def get_value(self):
        lib.SinglePointInformation_getValue.restype = c_bool
        return lib.SinglePointInformation_getValue(pSinglePointInformation(self))

    def get_quality(self):
        lib.SinglePointInformation_getQuality.restype = c_uint8
        value = lib.SinglePointInformation_getQuality(pSinglePointInformation(self))
        return QualityDescriptor(value)

pSinglePointInformation = ctypes.POINTER(SinglePointInformation)


class SinglePointWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_bool),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.singlePointWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_SP_TA_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.SinglePointWithCP24Time2a_create.restype = pSinglePointWithCP24Time2a
        return lib.SinglePointWithCP24Time2a_create(
            pSinglePointWithCP24Time2a(self),
            c_int(ioa),
            c_bool(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.SinglePointWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.SinglePointWithCP24Time2a_getTimestamp(pSinglePointWithCP24Time2a(self)).contents

    def get_value(self):
        lib.SinglePointInformation_getValue.restype = c_bool
        return lib.SinglePointInformation_getValue(pSinglePointInformation(self))

    def get_quality(self):
        lib.SinglePointInformation_getQuality.restype = c_uint8
        value = lib.SinglePointInformation_getQuality(pSinglePointInformation(self))
        return QualityDescriptor(value)

pSinglePointWithCP24Time2a = ctypes.POINTER(SinglePointWithCP24Time2a)


class DoublePointInformation(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', DoublePointValue),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, quality):
        self.virtualFunctionTable = ctypes.cast(lib.doublePointInformationVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_DP_NA_1.c_value
        self.create(ioa, value, quality)

    def create(self, ioa, value, quality):
        lib.DoublePointInformation_create.restype = pDoublePointInformation
        return lib.DoublePointInformation_create(
            pDoublePointInformation(self),
            c_int(ioa),
            DoublePointValue(value),
            c_int(quality)).contents

    def get_value(self):
        lib.DoublePointInformation_getValue.restype = DoublePointValue
        return lib.DoublePointInformation_getValue(pDoublePointInformation(self))

    def get_quality(self):
        lib.DoublePointInformation_getQuality.restype = c_uint8
        value = lib.DoublePointInformation_getQuality(pDoublePointInformation(self))
        return QualityDescriptor(value)

pDoublePointInformation = ctypes.POINTER(DoublePointInformation)


class DoublePointWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', DoublePointValue),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.doublePointWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_DP_TA_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.DoublePointWithCP24Time2a_create.restype = pDoublePointWithCP24Time2a
        return lib.DoublePointWithCP24Time2a_create(
            pDoublePointWithCP24Time2a(self),
            c_int(ioa),
            DoublePointValue(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.DoublePointWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.DoublePointWithCP24Time2a_getTimestamp(pDoublePointWithCP24Time2a(self)).contents

    def get_value(self):
        lib.DoublePointInformation_getValue.restype = DoublePointValue
        return lib.DoublePointInformation_getValue(pDoublePointInformation(self))

    def get_quality(self):
        lib.DoublePointInformation_getQuality.restype = c_uint8
        value = lib.DoublePointInformation_getQuality(pDoublePointInformation(self))
        return QualityDescriptor(value)

pDoublePointWithCP24Time2a = ctypes.POINTER(DoublePointWithCP24Time2a)


class StepPositionInformation(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('vti', c_uint8),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, isTransient, quality):
        self.virtualFunctionTable = ctypes.cast(lib.stepPositionInformationVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ST_NA_1.c_value
        self.create(ioa, value, isTransient, quality)

    def create(self, ioa, value, isTransient, quality):
        lib.StepPositionInformation_create.restype = pStepPositionInformation
        return lib.StepPositionInformation_create(
            pStepPositionInformation(self),
            c_int(ioa),
            c_int(value),
            c_bool(isTransient),
            c_int(quality)).contents

    def get_object_address(self):
        lib.StepPositionInformation_getObjectAddress.restype = c_int
        return lib.StepPositionInformation_getObjectAddress(pStepPositionInformation(self))

    def get_value(self):
        lib.StepPositionInformation_getValue.restype = c_int
        return lib.StepPositionInformation_getValue(pStepPositionInformation(self))

    def is_transient(self):
        lib.StepPositionInformation_isTransient.restype = c_bool
        return lib.StepPositionInformation_isTransient(pStepPositionInformation(self))

    def get_quality(self):
        lib.StepPositionInformation_getQuality.restype = c_uint8
        value = lib.StepPositionInformation_getQuality(pStepPositionInformation(self))
        return QualityDescriptor(value)

pStepPositionInformation = ctypes.POINTER(StepPositionInformation)


class StepPositionWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('vti', c_uint8),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, isTransient, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.stepPositionWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ST_TA_1.c_value
        self.create(ioa, value, isTransient, quality, timestamp)

    def create(self, ioa, value, isTransient, quality, timestamp):
        lib.StepPositionWithCP24Time2a_create.restype = pStepPositionWithCP24Time2a
        return lib.StepPositionWithCP24Time2a_create(
            pStepPositionWithCP24Time2a(self),
            c_int(ioa),
            c_int(value),
            c_bool(isTransient),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.StepPositionWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.StepPositionWithCP24Time2a_getTimestamp(pStepPositionWithCP24Time2a(self)).contents

    def get_object_address(self):
        lib.StepPositionInformation_getObjectAddress.restype = c_int
        return lib.StepPositionInformation_getObjectAddress(pStepPositionInformation(self))

    def get_value(self):
        lib.StepPositionInformation_getValue.restype = c_int
        return lib.StepPositionInformation_getValue(pStepPositionInformation(self))

    def is_transient(self):
        lib.StepPositionInformation_isTransient.restype = c_bool
        return lib.StepPositionInformation_isTransient(pStepPositionInformation(self))

    def get_quality(self):
        lib.StepPositionInformation_getQuality.restype = c_uint8
        value = lib.StepPositionInformation_getQuality(pStepPositionInformation(self))
        return QualityDescriptor(value)

pStepPositionWithCP24Time2a = ctypes.POINTER(StepPositionWithCP24Time2a)


class BitString32(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_uint32),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value):
        self.virtualFunctionTable = ctypes.cast(lib.bitString32VFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_BO_NA_1.c_value
        self.create(ioa, value)

    def create(self, ioa, value):
        lib.BitString32_create.restype = pBitString32
        return lib.BitString32_create(
            pBitString32(self),
            c_int(ioa),
            c_uint32(value)).contents

    def get_value(self):
        lib.BitString32_getValue.restype = c_uint32
        return lib.BitString32_getValue(pBitString32(self))

    def get_quality(self):
        lib.BitString32_getQuality.restype = c_uint8
        value = lib.BitString32_getQuality(pBitString32(self))
        return QualityDescriptor(value)

pBitString32 = ctypes.POINTER(BitString32)


class Bitstring32WithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_uint32),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.bitstring32WithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_BO_TA_1.c_value
        self.create(ioa, value, timestamp)

    def create(self, ioa, value, timestamp):
        lib.Bitstring32WithCP24Time2a_create.restype = pBitstring32WithCP24Time2a
        return lib.Bitstring32WithCP24Time2a_create(
            pBitstring32WithCP24Time2a(self),
            c_int(ioa),
            c_uint32(value),
            timestamp.pointer).contents

    def get_value(self):
        lib.BitString32_getValue.restype = c_uint32
        return lib.BitString32_getValue(pBitString32(self))

    def get_quality(self):
        lib.BitString32_getQuality.restype = c_uint8
        value = lib.BitString32_getQuality(pBitString32(self))
        return QualityDescriptor(value)

    def get_timestamp(self):
        lib.Bitstring32WithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.Bitstring32WithCP24Time2a_getTimestamp(pBitstring32WithCP24Time2a(self)).contents

pBitstring32WithCP24Time2a = ctypes.POINTER(Bitstring32WithCP24Time2a)


class MeasuredValueNormalized(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, quality):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueNormalizedVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_NA_1.c_value
        self.create(ioa, value, quality)

    def create(self, ioa, value, quality):
        lib.MeasuredValueNormalized_create.restype = pMeasuredValueNormalized
        return lib.MeasuredValueNormalized_create(
            pMeasuredValueNormalized(self),
            c_int(ioa),
            c_float(value),
            c_int(quality)).contents

    def get_value(self):
        lib.MeasuredValueNormalized_getValue.restype = c_float
        return lib.MeasuredValueNormalized_getValue(pMeasuredValueNormalized(self))

    def set_value(self, value):
        lib.MeasuredValueNormalized_setValue(
            pMeasuredValueNormalized(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueNormalized_getQuality.restype = c_uint8
        value = lib.MeasuredValueNormalized_getQuality(pMeasuredValueNormalized(self))
        return QualityDescriptor(value)

pMeasuredValueNormalized = ctypes.POINTER(MeasuredValueNormalized)


class MeasuredValueNormalizedWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueNormalizedWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TA_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueNormalizedWithCP24Time2a_create.restype = pMeasuredValueNormalizedWithCP24Time2a
        return lib.MeasuredValueNormalizedWithCP24Time2a_create(
            pMeasuredValueNormalizedWithCP24Time2a(self),
            c_int(ioa),
            c_float(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueNormalizedWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.MeasuredValueNormalizedWithCP24Time2a_getTimestamp(pMeasuredValueNormalizedWithCP24Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueNormalizedWithCP24Time2a_setTimestamp(
            pMeasuredValueNormalizedWithCP24Time2a(self),
            pCP24Time2a(value))

    def get_value(self):
        lib.MeasuredValueNormalized_getValue.restype = c_float
        return lib.MeasuredValueNormalized_getValue(pMeasuredValueNormalized(self))

    def set_value(self, value):
        lib.MeasuredValueNormalized_setValue(
            pMeasuredValueNormalized(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueNormalized_getQuality.restype = c_uint8
        value = lib.MeasuredValueNormalized_getQuality(pMeasuredValueNormalized(self))
        return QualityDescriptor(value)

pMeasuredValueNormalizedWithCP24Time2a = ctypes.POINTER(MeasuredValueNormalizedWithCP24Time2a)


class MeasuredValueScaled(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, quality):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueScaledVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_NB_1.c_value
        self.create(ioa, value, quality)

    def create(self, ioa, value, quality):
        lib.MeasuredValueScaled_create.restype = pMeasuredValueScaled
        return lib.MeasuredValueScaled_create(
            pMeasuredValueScaled(self),
            c_int(ioa),
            c_int(value),
            c_int(quality)).contents

    def get_value(self):
        lib.MeasuredValueScaled_getValue.restype = c_int
        return lib.MeasuredValueScaled_getValue(pMeasuredValueScaled(self))

    def set_value(self, value):
        lib.MeasuredValueScaled_setValue(
            pMeasuredValueScaled(self),
            c_int(value))

    def get_quality(self):
        lib.MeasuredValueScaled_getQuality.restype = c_uint8
        value = lib.MeasuredValueScaled_getQuality(pMeasuredValueScaled(self))
        return QualityDescriptor(value)

    def set_quality(self, quality):
        lib.MeasuredValueScaled_setQuality(
            pMeasuredValueScaled(self),
            c_int(quality))

pMeasuredValueScaled = ctypes.POINTER(MeasuredValueScaled)


class MeasuredValueScaledWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueScaledWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TB_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueScaledWithCP24Time2a_create.restype = pMeasuredValueScaledWithCP24Time2a
        return lib.MeasuredValueScaledWithCP24Time2a_create(
            pMeasuredValueScaledWithCP24Time2a(self),
            c_int(ioa),
            c_int(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueScaledWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.MeasuredValueScaledWithCP24Time2a_getTimestamp(pMeasuredValueScaledWithCP24Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueScaledWithCP24Time2a_setTimestamp(
            pMeasuredValueScaledWithCP24Time2a(self),
            pCP24Time2a(value))

    def get_value(self):
        lib.MeasuredValueScaled_getValue.restype = c_int
        return lib.MeasuredValueScaled_getValue(pMeasuredValueScaled(self))

    def set_value(self, value):
        lib.MeasuredValueScaled_setValue(
            pMeasuredValueScaled(self),
            c_int(value))

    def get_quality(self):
        lib.MeasuredValueScaled_getQuality.restype = c_uint8
        value = lib.MeasuredValueScaled_getQuality(pMeasuredValueScaled(self))
        return QualityDescriptor(value)

    def set_quality(self, quality):
        lib.MeasuredValueScaled_setQuality(
            pMeasuredValueScaled(self),
            c_int(quality))

pMeasuredValueScaledWithCP24Time2a = ctypes.POINTER(MeasuredValueScaledWithCP24Time2a)


class MeasuredValueShort(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_float),
        ('quality', c_uint8)
        ]

    def __init__(self, ioa, value, quality):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueShortVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_NC_1.c_value
        self.create(ioa, value, quality)

    def create(self, ioa, value, quality):
        lib.MeasuredValueShort_create.restype = pMeasuredValueShort
        return lib.MeasuredValueShort_create(
            pMeasuredValueShort(self),
            c_int(ioa),
            c_float(value),
            c_int(quality)).contents

    def get_value(self):
        lib.MeasuredValueShort_getValue.restype = c_float
        return lib.MeasuredValueShort_getValue(pMeasuredValueShort(self))

    def set_value(self, value):
        lib.MeasuredValueShort_setValue(
            pMeasuredValueShort(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueShort_getQuality.restype = c_uint8
        value = lib.MeasuredValueShort_getQuality(pMeasuredValueShort(self))
        return QualityDescriptor(value)

pMeasuredValueShort = ctypes.POINTER(MeasuredValueShort)


class MeasuredValueShortWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_float),
        ('quality', c_uint8),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueShortWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TC_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueShortWithCP24Time2a_create.restype = pMeasuredValueShortWithCP24Time2a
        return lib.MeasuredValueShortWithCP24Time2a_create(
            pMeasuredValueShortWithCP24Time2a(self),
            c_int(ioa),
            c_float(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueShortWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.MeasuredValueShortWithCP24Time2a_getTimestamp(pMeasuredValueShortWithCP24Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueShortWithCP24Time2a_setTimestamp(
            pMeasuredValueShortWithCP24Time2a(self),
            pCP24Time2a(value))

    def get_value(self):
        lib.MeasuredValueShort_getValue.restype = c_float
        return lib.MeasuredValueShort_getValue(pMeasuredValueShort(self))

    def set_value(self, value):
        lib.MeasuredValueShort_setValue(
            pMeasuredValueShort(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueShort_getQuality.restype = c_uint8
        value = lib.MeasuredValueShort_getQuality(pMeasuredValueShort(self))
        return QualityDescriptor(value)

pMeasuredValueShortWithCP24Time2a = ctypes.POINTER(MeasuredValueShortWithCP24Time2a)


class IntegratedTotals(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('totals', BinaryCounterReading)
        ]

    def __init__(self, ioa, value):
        self.virtualFunctionTable = ctypes.cast(lib.integratedTotalsVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_IT_NA_1.c_value
        self.create(ioa, value)

    def create(self, ioa, value):
        lib.IntegratedTotals_create.restype = pIntegratedTotals
        return lib.IntegratedTotals_create(
            pIntegratedTotals(self),
            c_int(ioa),
            pBinaryCounterReading(value)).contents

    def get_bcr(self):
        lib.IntegratedTotals_getBCR.restype = pBinaryCounterReading
        return lib.IntegratedTotals_getBCR(pIntegratedTotals(self)).contents

    def set_bcr(self, value):
        lib.IntegratedTotals_setBCR(
            pIntegratedTotals(self),
            pBinaryCounterReading(value))

pIntegratedTotals = ctypes.POINTER(IntegratedTotals)


class IntegratedTotalsWithCP24Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('totals', BinaryCounterReading),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, value, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.integratedTotalsWithCP24Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_IT_TA_1.c_value
        self.create(ioa, value, timestamp)

    def create(self, ioa, value, timestamp):
        lib.IntegratedTotalsWithCP24Time2a_create.restype = pIntegratedTotalsWithCP24Time2a
        return lib.IntegratedTotalsWithCP24Time2a_create(
            pIntegratedTotalsWithCP24Time2a(self),
            c_int(ioa),
            pBinaryCounterReading(value),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.IntegratedTotalsWithCP24Time2a_getTimestamp.restype = pCP24Time2a
        return lib.IntegratedTotalsWithCP24Time2a_getTimestamp(pIntegratedTotalsWithCP24Time2a(self)).contents

    def set_timestamp(self, value):
        lib.IntegratedTotalsWithCP24Time2a_setTimestamp(
            pIntegratedTotalsWithCP24Time2a(self),
            pCP24Time2a(value))

    def get_bcr(self):
        lib.IntegratedTotals_getBCR.restype = pBinaryCounterReading
        return lib.IntegratedTotals_getBCR(pIntegratedTotals(self)).contents

    def set_bcr(self, value):
        lib.IntegratedTotals_setBCR(
            pIntegratedTotals(self),
            pBinaryCounterReading(value))

pIntegratedTotalsWithCP24Time2a = ctypes.POINTER(IntegratedTotalsWithCP24Time2a)


class EventOfProtectionEquipment(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('event', SingleEvent),
        ('elapsedTime', CP16Time2a),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, event, elapsedTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.eventOfProtectionEquipmentVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TA_1.c_value
        self.create(ioa, event, elapsedTime, timestamp)

    def create(self, ioa, event, elapsedTime, timestamp):
        lib.EventOfProtectionEquipment_create.restype = pEventOfProtectionEquipment
        return lib.EventOfProtectionEquipment_create(
            pEventOfProtectionEquipment(self),
            c_int(ioa),
            pSingleEvent(event),
            pCP16Time2a(elapsedTime),
            timestamp.pointer).contents

    def get_event(self):
        lib.EventOfProtectionEquipment_getEvent.restype = pSingleEvent
        return lib.EventOfProtectionEquipment_getEvent(pEventOfProtectionEquipment(self)).contents

    def get_elapsed_time(self):
        lib.EventOfProtectionEquipment_getElapsedTime.restype = pCP16Time2a
        return lib.EventOfProtectionEquipment_getElapsedTime(pEventOfProtectionEquipment(self)).contents

    def get_timestamp(self):
        lib.EventOfProtectionEquipment_getTimestamp.restype = pCP24Time2a
        return lib.EventOfProtectionEquipment_getTimestamp(pEventOfProtectionEquipment(self)).contents

pEventOfProtectionEquipment = ctypes.POINTER(EventOfProtectionEquipment)


class PackedStartEventsOfProtectionEquipment(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('event', StartEvent),
        ('qdp', c_uint8),
        ('elapsedTime', CP16Time2a),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, event, qdp, elapsedTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.packedStartEventsOfProtectionEquipmentVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TB_1.c_value
        self.create(ioa, event, qdp, elapsedTime, timestamp)

    def create(self, ioa, event, qdp, elapsedTime, timestamp):
        lib.PackedStartEventsOfProtectionEquipment_create.restype = pPackedStartEventsOfProtectionEquipment
        return lib.PackedStartEventsOfProtectionEquipment_create(
            pPackedStartEventsOfProtectionEquipment(self),
            c_int(ioa),
            StartEvent(event),
            c_int(qdp),
            pCP16Time2a(elapsedTime),
            timestamp.pointer).contents

    def get_event(self):
        lib.PackedStartEventsOfProtectionEquipment_getEvent.restype = StartEvent
        return lib.PackedStartEventsOfProtectionEquipment_getEvent(pPackedStartEventsOfProtectionEquipment(self))

    def get_quality(self):
        lib.PackedStartEventsOfProtectionEquipment_getQuality.restype = c_uint8
        value = lib.PackedStartEventsOfProtectionEquipment_getQuality(pPackedStartEventsOfProtectionEquipment(self))
        return QualityDescriptor(value)

    def get_elapsed_time(self):
        lib.PackedStartEventsOfProtectionEquipment_getElapsedTime.restype = pCP16Time2a
        return lib.PackedStartEventsOfProtectionEquipment_getElapsedTime(pPackedStartEventsOfProtectionEquipment(self)).contents

    def get_timestamp(self):
        lib.PackedStartEventsOfProtectionEquipment_getTimestamp.restype = pCP24Time2a
        return lib.PackedStartEventsOfProtectionEquipment_getTimestamp(pPackedStartEventsOfProtectionEquipment(self)).contents

pPackedStartEventsOfProtectionEquipment = ctypes.POINTER(PackedStartEventsOfProtectionEquipment)


class PackedOutputCircuitInfo(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('oci', OutputCircuitInfo),
        ('qdp', c_uint8),
        ('operatingTime', CP16Time2a),
        ('timestamp', CP24Time2a)
        ]

    def __init__(self, ioa, oci, qdp, operatingTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.packedOutputCircuitInfoVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TC_1.c_value
        self.create(ioa, oci, qdp, operatingTime, timestamp)

    def create(self, ioa, oci, qdp, operatingTime, timestamp):
        lib.PackedOutputCircuitInfo_create.restype = pPackedOutputCircuitInfo
        return lib.PackedOutputCircuitInfo_create(
            pPackedOutputCircuitInfo(self),
            c_int(ioa),
            OutputCircuitInfo(oci),
            c_int(qdp),
            pCP16Time2a(operatingTime),
            timestamp.pointer).contents

    def get_oci(self):
        lib.PackedOutputCircuitInfo_getOCI.restype = OutputCircuitInfo
        return lib.PackedOutputCircuitInfo_getOCI(pPackedOutputCircuitInfo(self))

    def get_quality(self):
        lib.PackedOutputCircuitInfo_getQuality.restype = c_uint8
        value = lib.PackedOutputCircuitInfo_getQuality(pPackedOutputCircuitInfo(self))
        return QualityDescriptor(value)

    def get_operating_time(self):
        lib.PackedOutputCircuitInfo_getOperatingTime.restype = pCP16Time2a
        return lib.PackedOutputCircuitInfo_getOperatingTime(pPackedOutputCircuitInfo(self)).contents

    def get_timestamp(self):
        lib.PackedOutputCircuitInfo_getTimestamp.restype = pCP24Time2a
        return lib.PackedOutputCircuitInfo_getTimestamp(pPackedOutputCircuitInfo(self)).contents

pPackedOutputCircuitInfo = ctypes.POINTER(PackedOutputCircuitInfo)


class PackedSinglePointWithSCD(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('scd', StatusAndStatusChangeDetection),
        ('qds', c_uint8)
        ]

    def __init__(self, ioa, scd, qds):
        self.virtualFunctionTable = ctypes.cast(lib.packedSinglePointWithSCDVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_PS_NA_1.c_value
        self.create(ioa, scd, qds)

    def create(self, ioa, scd, qds):
        lib.PackedSinglePointWithSCD_create.restype = pPackedSinglePointWithSCD
        return lib.PackedSinglePointWithSCD_create(
            pPackedSinglePointWithSCD(self),
            c_int(ioa),
            pStatusAndStatusChangeDetection(scd),
            QualityDescriptor(qds)).contents

    def get_quality(self):
        lib.PackedSinglePointWithSCD_getQuality.restype = c_uint8
        value = lib.PackedSinglePointWithSCD_getQuality(pPackedSinglePointWithSCD(self))
        return QualityDescriptor(value)

    def get_scd(self):
        lib.PackedSinglePointWithSCD_getSCD.restype = pStatusAndStatusChangeDetection
        return lib.PackedSinglePointWithSCD_getSCD(pPackedSinglePointWithSCD(self)).contents

pPackedSinglePointWithSCD = ctypes.POINTER(PackedSinglePointWithSCD)


class MeasuredValueNormalizedWithoutQuality(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2)
        ]

    def __init__(self, ioa, value):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueNormalizedWithoutQualityVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_ND_1.c_value
        self.create(ioa, value)

    def create(self, ioa, value):
        lib.MeasuredValueNormalizedWithoutQuality_create.restype = pMeasuredValueNormalizedWithoutQuality
        return lib.MeasuredValueNormalizedWithoutQuality_create(
            pMeasuredValueNormalizedWithoutQuality(self),
            c_int(ioa),
            c_float(value)).contents

    def get_value(self):
        lib.MeasuredValueNormalizedWithoutQuality_getValue.restype = c_float
        return lib.MeasuredValueNormalizedWithoutQuality_getValue(pMeasuredValueNormalizedWithoutQuality(self))

    def set_value(self, value):
        lib.MeasuredValueNormalizedWithoutQuality_setValue(
            pMeasuredValueNormalizedWithoutQuality(self),
            c_float(value))

pMeasuredValueNormalizedWithoutQuality = ctypes.POINTER(MeasuredValueNormalizedWithoutQuality)


class SinglePointWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_bool),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.singlePointWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_SP_TB_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.SinglePointWithCP56Time2a_create.restype = pSinglePointWithCP56Time2a
        return lib.SinglePointWithCP56Time2a_create(
            pSinglePointWithCP56Time2a(self),
            c_int(ioa),
            c_bool(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.SinglePointWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.SinglePointWithCP56Time2a_getTimestamp(pSinglePointWithCP56Time2a(self)).contents

    def get_value(self):
        lib.SinglePointInformation_getValue.restype = c_bool
        return lib.SinglePointInformation_getValue(pSinglePointInformation(self))

    def get_quality(self):
        lib.SinglePointInformation_getQuality.restype = c_uint8
        value = lib.SinglePointInformation_getQuality(pSinglePointInformation(self))
        return QualityDescriptor(value)

pSinglePointWithCP56Time2a = ctypes.POINTER(SinglePointWithCP56Time2a)


class DoublePointWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', DoublePointValue),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.doublePointWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_DP_TB_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.DoublePointWithCP56Time2a_create.restype = pDoublePointWithCP56Time2a
        return lib.DoublePointWithCP56Time2a_create(
            pDoublePointWithCP56Time2a(self),
            c_int(ioa),
            DoublePointValue(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.DoublePointWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.DoublePointWithCP56Time2a_getTimestamp(pDoublePointWithCP56Time2a(self)).contents

    def get_value(self):
        lib.DoublePointInformation_getValue.restype = DoublePointValue
        return lib.DoublePointInformation_getValue(pDoublePointInformation(self))

    def get_quality(self):
        lib.DoublePointInformation_getQuality.restype = c_uint8
        value = lib.DoublePointInformation_getQuality(pDoublePointInformation(self))
        return QualityDescriptor(value)

pDoublePointWithCP56Time2a = ctypes.POINTER(DoublePointWithCP56Time2a)


class StepPositionWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('vti', c_uint8),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, isTransient, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.stepPositionWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ST_TB_1.c_value
        self.create(ioa, value, isTransient, quality, timestamp)

    def create(self, ioa, value, isTransient, quality, timestamp):
        lib.StepPositionWithCP56Time2a_create.restype = pStepPositionWithCP56Time2a
        return lib.StepPositionWithCP56Time2a_create(
            pStepPositionWithCP56Time2a(self),
            c_int(ioa),
            c_int(value),
            c_bool(isTransient),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.StepPositionWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.StepPositionWithCP56Time2a_getTimestamp(pStepPositionWithCP56Time2a(self)).contents

    def get_object_address(self):
        lib.StepPositionInformation_getObjectAddress.restype = c_int
        return lib.StepPositionInformation_getObjectAddress(pStepPositionInformation(self))

    def get_value(self):
        lib.StepPositionInformation_getValue.restype = c_int
        return lib.StepPositionInformation_getValue(pStepPositionInformation(self))

    def is_transient(self):
        lib.StepPositionInformation_isTransient.restype = c_bool
        return lib.StepPositionInformation_isTransient(pStepPositionInformation(self))

    def get_quality(self):
        lib.StepPositionInformation_getQuality.restype = c_uint8
        value = lib.StepPositionInformation_getQuality(pStepPositionInformation(self))
        return QualityDescriptor(value)

pStepPositionWithCP56Time2a = ctypes.POINTER(StepPositionWithCP56Time2a)


class Bitstring32WithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_uint32),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.bitstring32WithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_BO_TB_1.c_value
        self.create(ioa, value, timestamp)

    def create(self, ioa, value, timestamp):
        lib.Bitstring32WithCP56Time2a_create.restype = pBitstring32WithCP56Time2a
        return lib.Bitstring32WithCP56Time2a_create(
            pBitstring32WithCP56Time2a(self),
            c_int(ioa),
            c_uint32(value),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.Bitstring32WithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.Bitstring32WithCP56Time2a_getTimestamp(pBitstring32WithCP56Time2a(self)).contents

pBitstring32WithCP56Time2a = ctypes.POINTER(Bitstring32WithCP56Time2a)


class MeasuredValueNormalizedWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueNormalizedWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TD_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueNormalizedWithCP56Time2a_create.restype = pMeasuredValueNormalizedWithCP56Time2a
        return lib.MeasuredValueNormalizedWithCP56Time2a_create(
            pMeasuredValueNormalizedWithCP56Time2a(self),
            c_int(ioa),
            c_float(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueNormalizedWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.MeasuredValueNormalizedWithCP56Time2a_getTimestamp(pMeasuredValueNormalizedWithCP56Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueNormalizedWithCP56Time2a_setTimestamp(
            pMeasuredValueNormalizedWithCP56Time2a(self),
            pCP56Time2a(value))

    def get_value(self):
        lib.MeasuredValueNormalized_getValue.restype = c_float
        return lib.MeasuredValueNormalized_getValue(pMeasuredValueNormalized(self))

    def set_value(self, value):
        lib.MeasuredValueNormalized_setValue(
            pMeasuredValueNormalized(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueNormalized_getQuality.restype = c_uint8
        value = lib.MeasuredValueNormalized_getQuality(pMeasuredValueNormalized(self))
        return QualityDescriptor(value)

pMeasuredValueNormalizedWithCP56Time2a = ctypes.POINTER(MeasuredValueNormalizedWithCP56Time2a)


class MeasuredValueScaledWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueScaledWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TE_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueScaledWithCP56Time2a_create.restype = pMeasuredValueScaledWithCP56Time2a
        return lib.MeasuredValueScaledWithCP56Time2a_create(
            pMeasuredValueScaledWithCP56Time2a(self),
            c_int(ioa),
            c_int(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueScaledWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.MeasuredValueScaledWithCP56Time2a_getTimestamp(pMeasuredValueScaledWithCP56Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueScaledWithCP56Time2a_setTimestamp(
            pMeasuredValueScaledWithCP56Time2a(self),
            pCP56Time2a(value))

    def get_value(self):
        lib.MeasuredValueScaled_getValue.restype = c_int
        return lib.MeasuredValueScaled_getValue(pMeasuredValueScaled(self))

    def set_value(self, value):
        lib.MeasuredValueScaled_setValue(
            pMeasuredValueScaled(self),
            c_int(value))

    def get_quality(self):
        lib.MeasuredValueScaled_getQuality.restype = c_uint8
        value = lib.MeasuredValueScaled_getQuality(pMeasuredValueScaled(self))
        return QualityDescriptor(value)

    def set_quality(self, quality):
        lib.MeasuredValueScaled_setQuality(
            pMeasuredValueScaled(self),
            c_int(quality))

pMeasuredValueScaledWithCP56Time2a = ctypes.POINTER(MeasuredValueScaledWithCP56Time2a)


class MeasuredValueShortWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_float),
        ('quality', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, quality, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.measuredValueShortWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_ME_TF_1.c_value
        self.create(ioa, value, quality, timestamp)

    def create(self, ioa, value, quality, timestamp):
        lib.MeasuredValueShortWithCP56Time2a_create.restype = pMeasuredValueShortWithCP56Time2a
        return lib.MeasuredValueShortWithCP56Time2a_create(
            pMeasuredValueShortWithCP56Time2a(self),
            c_int(ioa),
            c_float(value),
            c_int(quality),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.MeasuredValueShortWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.MeasuredValueShortWithCP56Time2a_getTimestamp(pMeasuredValueShortWithCP56Time2a(self)).contents

    def set_timestamp(self, value):
        lib.MeasuredValueShortWithCP56Time2a_setTimestamp(
            pMeasuredValueShortWithCP56Time2a(self),
            pCP56Time2a(value))

    def get_value(self):
        lib.MeasuredValueShort_getValue.restype = c_float
        return lib.MeasuredValueShort_getValue(pMeasuredValueShort(self))

    def set_value(self, value):
        lib.MeasuredValueShort_setValue(
            pMeasuredValueShort(self),
            c_float(value))

    def get_quality(self):
        lib.MeasuredValueShort_getQuality.restype = c_uint8
        value = lib.MeasuredValueShort_getQuality(pMeasuredValueShort(self))
        return QualityDescriptor(value)

pMeasuredValueShortWithCP56Time2a = ctypes.POINTER(MeasuredValueShortWithCP56Time2a)


class IntegratedTotalsWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('totals', BinaryCounterReading),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.integratedTotalsWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_IT_TB_1.c_value
        self.create(ioa, value, timestamp)

    def create(self, ioa, value, timestamp):
        lib.IntegratedTotalsWithCP56Time2a_create.restype = pIntegratedTotalsWithCP56Time2a
        return lib.IntegratedTotalsWithCP56Time2a_create(
            pIntegratedTotalsWithCP56Time2a(self),
            c_int(ioa),
            pBinaryCounterReading(value),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.IntegratedTotalsWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.IntegratedTotalsWithCP56Time2a_getTimestamp(pIntegratedTotalsWithCP56Time2a(self)).contents

    def set_timestamp(self, value):
        lib.IntegratedTotalsWithCP56Time2a_setTimestamp(
            pIntegratedTotalsWithCP56Time2a(self),
            pCP56Time2a(value))

    def get_bcr(self):
        lib.IntegratedTotals_getBCR.restype = pBinaryCounterReading
        return lib.IntegratedTotals_getBCR(pIntegratedTotals(self)).contents

    def set_bcr(self, value):
        lib.IntegratedTotals_setBCR(
            pIntegratedTotals(self),
            pBinaryCounterReading(value))

pIntegratedTotalsWithCP56Time2a = ctypes.POINTER(IntegratedTotalsWithCP56Time2a)


class EventOfProtectionEquipmentWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('event', SingleEvent),
        ('elapsedTime', CP16Time2a),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, event, elapsedTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.eventOfProtectionEquipmentWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TD_1.c_value
        self.create(ioa, event, elapsedTime, timestamp)

    def create(self, ioa, event, elapsedTime, timestamp):
        lib.EventOfProtectionEquipmentWithCP56Time2a_create.restype = pEventOfProtectionEquipmentWithCP56Time2a
        return lib.EventOfProtectionEquipmentWithCP56Time2a_create(
            pEventOfProtectionEquipmentWithCP56Time2a(self),
            c_int(ioa),
            pSingleEvent(event),
            pCP16Time2a(elapsedTime),
            timestamp.pointer).contents

    def get_event(self):
        lib.EventOfProtectionEquipmentWithCP56Time2a_getEvent.restype = pSingleEvent
        return lib.EventOfProtectionEquipmentWithCP56Time2a_getEvent(pEventOfProtectionEquipmentWithCP56Time2a(self)).contents

    def get_elapsed_time(self):
        lib.EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime.restype = pCP16Time2a
        return lib.EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(pEventOfProtectionEquipmentWithCP56Time2a(self)).contents

    def get_timestamp(self):
        lib.EventOfProtectionEquipmentWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(pEventOfProtectionEquipmentWithCP56Time2a(self)).contents

pEventOfProtectionEquipmentWithCP56Time2a = ctypes.POINTER(EventOfProtectionEquipmentWithCP56Time2a)


class PackedStartEventsOfProtectionEquipmentWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('event', StartEvent),
        ('qdp', c_uint8),
        ('elapsedTime', CP16Time2a),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, event, qdp, elapsedTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.packedStartEventsOfProtectionEquipmentWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TE_1.c_value
        self.create(ioa, event, qdp, elapsedTime, timestamp)

    def create(self, ioa, event, qdp, elapsedTime, timestamp):
        lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create.restype = pPackedStartEventsOfProtectionEquipmentWithCP56Time2a
        return lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
            pPackedStartEventsOfProtectionEquipmentWithCP56Time2a(self),
            c_int(ioa),
            StartEvent(event),
            c_int(qdp),
            pCP16Time2a(elapsedTime),
            timestamp.pointer).contents

    def get_event(self):
        lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent.restype = StartEvent
        return lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(pPackedStartEventsOfProtectionEquipmentWithCP56Time2a(self))

    def get_quality(self):
        lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality.restype = c_uint8
        value = lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(pPackedStartEventsOfProtectionEquipmentWithCP56Time2a(self))
        return QualityDescriptor(value)

    def get_elapsed_time(self):
        lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime.restype = pCP16Time2a
        return lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(pPackedStartEventsOfProtectionEquipmentWithCP56Time2a(self)).contents

    def get_timestamp(self):
        lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(pPackedStartEventsOfProtectionEquipmentWithCP56Time2a(self)).contents

pPackedStartEventsOfProtectionEquipmentWithCP56Time2a = ctypes.POINTER(PackedStartEventsOfProtectionEquipmentWithCP56Time2a)


class PackedOutputCircuitInfoWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('oci', OutputCircuitInfo),
        ('qdp', c_uint8),
        ('operatingTime', CP16Time2a),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, oci, qdp, operatingTime, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.packedOutputCircuitInfoWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EP_TF_1.c_value
        self.create(ioa, oci, qdp, operatingTime, timestamp)

    def create(self, ioa, oci, qdp, operatingTime, timestamp):
        lib.PackedOutputCircuitInfoWithCP56Time2a_create.restype = pPackedOutputCircuitInfoWithCP56Time2a
        return lib.PackedOutputCircuitInfoWithCP56Time2a_create(
            pPackedOutputCircuitInfoWithCP56Time2a(self),
            c_int(ioa),
            OutputCircuitInfo(oci),
            c_int(qdp),
            pCP16Time2a(operatingTime),
            timestamp.pointer).contents

    def get_oci(self):
        lib.PackedOutputCircuitInfoWithCP56Time2a_getOCI.restype = OutputCircuitInfo
        return lib.PackedOutputCircuitInfoWithCP56Time2a_getOCI(pPackedOutputCircuitInfoWithCP56Time2a(self))

    def get_quality(self):
        lib.PackedOutputCircuitInfoWithCP56Time2a_getQuality.restype = c_uint8
        value = lib.PackedOutputCircuitInfoWithCP56Time2a_getQuality(pPackedOutputCircuitInfoWithCP56Time2a(self))
        return QualityDescriptor(value)

    def get_operating_time(self):
        lib.PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime.restype = pCP16Time2a
        return lib.PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(pPackedOutputCircuitInfoWithCP56Time2a(self)).contents

    def get_timestamp(self):
        lib.PackedOutputCircuitInfoWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(pPackedOutputCircuitInfoWithCP56Time2a(self)).contents

pPackedOutputCircuitInfoWithCP56Time2a = ctypes.POINTER(PackedOutputCircuitInfoWithCP56Time2a)


class SingleCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('sco', c_uint8)
        ]

    def __init__(self, ioa, command, selectCommand, qu):
        self.virtualFunctionTable = ctypes.cast(lib.singleCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SC_NA_1.c_value
        self.create(ioa, command, selectCommand, qu)

    def create(self, ioa, command, selectCommand, qu):
        lib.SingleCommand_create.restype = pSingleCommand
        return lib.SingleCommand_create(
            pSingleCommand(self),
            c_int(ioa),
            c_bool(command),
            c_bool(selectCommand),
            c_int(qu)).contents

    def get_qu(self):
        lib.SingleCommand_getQU.restype = c_int
        return lib.SingleCommand_getQU(pSingleCommand(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.SingleCommand_getState.restype = c_bool
        return lib.SingleCommand_getState(pSingleCommand(self))

    def is_select(self):
        lib.SingleCommand_isSelect.restype = c_bool
        return lib.SingleCommand_isSelect(pSingleCommand(self))

pSingleCommand = ctypes.POINTER(SingleCommand)


class DoubleCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('dcq', c_uint8)
        ]

    def __init__(self, ioa, command, selectCommand, qu):
        self.virtualFunctionTable = ctypes.cast(lib.doubleCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_DC_NA_1.c_value
        self.create(ioa, command, selectCommand, qu)

    def create(self, ioa, command, selectCommand, qu):
        lib.DoubleCommand_create.restype = pDoubleCommand
        return lib.DoubleCommand_create(
            pDoubleCommand(self),
            c_int(ioa),
            c_int(command),
            c_bool(selectCommand),
            c_int(qu)).contents

    def get_qu(self):
        lib.DoubleCommand_getQU.restype = c_int
        return lib.DoubleCommand_getQU(pDoubleCommand(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.DoubleCommand_getState.restype = c_int
        return lib.DoubleCommand_getState(pDoubleCommand(self))

    def is_select(self):
        lib.DoubleCommand_isSelect.restype = c_bool
        return lib.DoubleCommand_isSelect(pDoubleCommand(self))

pDoubleCommand = ctypes.POINTER(DoubleCommand)


class StepCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('dcq', c_uint8)
        ]

    def __init__(self, ioa, command, selectCommand, qu):
        self.virtualFunctionTable = ctypes.cast(lib.stepCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_RC_NA_1.c_value
        self.create(ioa, command, selectCommand, qu)

    def create(self, ioa, command, selectCommand, qu):
        lib.StepCommand_create.restype = pStepCommand
        return lib.StepCommand_create(
            pStepCommand(self),
            c_int(ioa),
            StepCommandValue(command),
            c_bool(selectCommand),
            c_int(qu)).contents

    def get_qu(self):
        lib.StepCommand_getQU.restype = c_int
        return lib.StepCommand_getQU(pStepCommand(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.StepCommand_getState.restype = StepCommandValue
        return lib.StepCommand_getState(pStepCommand(self)).contents

    def is_select(self):
        lib.StepCommand_isSelect.restype = c_bool
        return lib.StepCommand_isSelect(pStepCommand(self))

pStepCommand = ctypes.POINTER(StepCommand)


class SetpointCommandNormalized(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('qos', c_uint8)
        ]

    def __init__(self, ioa, value, selectCommand, ql):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandNormalizedVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_NA_1.c_value
        self.create(ioa, value, selectCommand, ql)

    def create(self, ioa, value, selectCommand, ql):
        lib.SetpointCommandNormalized_create.restype = pSetpointCommandNormalized
        return lib.SetpointCommandNormalized_create(
            pSetpointCommandNormalized(self),
            c_int(ioa),
            c_float(value),
            c_bool(selectCommand),
            c_int(ql)).contents

    def get_value(self):
        lib.SetpointCommandNormalized_getValue.restype = c_float
        return lib.SetpointCommandNormalized_getValue(pSetpointCommandNormalized(self))

    def get_ql(self):
        lib.SetpointCommandNormalized_getQL.restype = c_int
        return lib.SetpointCommandNormalized_getQL(pSetpointCommandNormalized(self))

    def is_select(self):
        lib.SetpointCommandNormalized_isSelect.restype = c_bool
        return lib.SetpointCommandNormalized_isSelect(pSetpointCommandNormalized(self))

pSetpointCommandNormalized = ctypes.POINTER(SetpointCommandNormalized)


class SetpointCommandScaled(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('qos', c_uint8)
        ]

    def __init__(self, ioa, value, selectCommand, ql):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandScaledVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_NB_1.c_value
        self.create(ioa, value, selectCommand, ql)

    def create(self, ioa, value, selectCommand, ql):
        lib.SetpointCommandScaled_create.restype = pSetpointCommandScaled
        return lib.SetpointCommandScaled_create(
            pSetpointCommandScaled(self),
            c_int(ioa),
            c_int(value),
            c_bool(selectCommand),
            c_int(ql)).contents

    def get_value(self):
        lib.SetpointCommandScaled_getValue.restype = c_int
        return lib.SetpointCommandScaled_getValue(pSetpointCommandScaled(self))

    def get_ql(self):
        lib.SetpointCommandScaled_getQL.restype = c_int
        return lib.SetpointCommandScaled_getQL(pSetpointCommandScaled(self))

    def is_select(self):
        lib.SetpointCommandScaled_isSelect.restype = c_bool
        return lib.SetpointCommandScaled_isSelect(pSetpointCommandScaled(self))

pSetpointCommandScaled = ctypes.POINTER(SetpointCommandScaled)


class SetpointCommandShort(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_float),
        ('qos', c_uint8)
        ]

    def __init__(self, ioa, value, selectCommand, ql):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandShortVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_NC_1.c_value
        self.create(ioa, value, selectCommand, ql)

    def create(self, ioa, value, selectCommand, ql):
        lib.SetpointCommandShort_create.restype = pSetpointCommandShort
        return lib.SetpointCommandShort_create(
            pSetpointCommandShort(self),
            c_int(ioa),
            c_float(value),
            c_bool(selectCommand),
            c_int(ql)).contents

    def get_value(self):
        lib.SetpointCommandShort_getValue.restype = c_float
        return lib.SetpointCommandShort_getValue(pSetpointCommandShort(self))

    def get_ql(self):
        lib.SetpointCommandShort_getQL.restype = c_int
        return lib.SetpointCommandShort_getQL(pSetpointCommandShort(self))

    def is_select(self):
        lib.SetpointCommandShort_isSelect.restype = c_bool
        return lib.SetpointCommandShort_isSelect(pSetpointCommandShort(self))

pSetpointCommandShort = ctypes.POINTER(SetpointCommandShort)


class Bitstring32Command(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_uint32)
        ]

    def __init__(self, ioa, value):
        self.virtualFunctionTable = ctypes.cast(lib.bitstring32CommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_BO_NA_1.c_value
        self.create(ioa, value)

    def create(self, ioa, value):
        lib.Bitstring32Command_create.restype = pBitstring32Command
        return lib.Bitstring32Command_create(
            pBitstring32Command(self),
            c_int(ioa),
            c_uint32(value)).contents

    def get_value(self):
        lib.Bitstring32Command_getValue.restype = c_uint32
        return lib.Bitstring32Command_getValue(pBitstring32Command(self))

pBitstring32Command = ctypes.POINTER(Bitstring32Command)


class SingleCommandWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('sco', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, command, selectCommand, qu, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.singleCommandWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SC_TA_1.c_value
        self.create(ioa, command, selectCommand, qu, timestamp)

    def create(self, ioa, command, selectCommand, qu, timestamp):
        lib.SingleCommandWithCP56Time2a_create.restype = pSingleCommandWithCP56Time2a
        return lib.SingleCommandWithCP56Time2a_create(
            pSingleCommandWithCP56Time2a(self),
            c_int(ioa),
            c_bool(command),
            c_bool(selectCommand),
            c_int(qu),
            timestamp.pointer).contents

    def get_timestamp(self):
        lib.SingleCommandWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.SingleCommandWithCP56Time2a_getTimestamp(pSingleCommandWithCP56Time2a(self)).contents

    def get_qu(self):
        lib.SingleCommand_getQU.restype = c_int
        return lib.SingleCommand_getQU(pSingleCommand(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.SingleCommand_getState.restype = c_bool
        return lib.SingleCommand_getState(pSingleCommand(self))

    def is_select(self):
        lib.SingleCommand_isSelect.restype = c_bool
        return lib.SingleCommand_isSelect(pSingleCommand(self))

pSingleCommandWithCP56Time2a = ctypes.POINTER(SingleCommandWithCP56Time2a)


class DoubleCommandWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('dcq', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, command, selectCommand, qu, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.doubleCommandWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_DC_TA_1.c_value
        self.create(ioa, command, selectCommand, qu, timestamp)

    def create(self, ioa, command, selectCommand, qu, timestamp):
        lib.DoubleCommandWithCP56Time2a_create.restype = pDoubleCommandWithCP56Time2a
        return lib.DoubleCommandWithCP56Time2a_create(
            pDoubleCommandWithCP56Time2a(self),
            c_int(ioa),
            c_int(command),
            c_bool(selectCommand),
            c_int(qu),
            timestamp.pointer).contents

    def get_qu(self):
        lib.DoubleCommandWithCP56Time2a_getQU.restype = c_int
        return lib.DoubleCommandWithCP56Time2a_getQU(pDoubleCommandWithCP56Time2a(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.DoubleCommandWithCP56Time2a_getState.restype = c_int
        return lib.DoubleCommandWithCP56Time2a_getState(pDoubleCommandWithCP56Time2a(self))

    def is_select(self):
        lib.DoubleCommandWithCP56Time2a_isSelect.restype = c_bool
        return lib.DoubleCommandWithCP56Time2a_isSelect(pDoubleCommandWithCP56Time2a(self))

pDoubleCommandWithCP56Time2a = ctypes.POINTER(DoubleCommandWithCP56Time2a)


class StepCommandWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('dcq', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, command, selectCommand, qu, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.stepCommandWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_RC_TA_1.c_value
        self.create(ioa, command, selectCommand, qu, timestamp)

    def create(self, ioa, command, selectCommand, qu, timestamp):
        lib.StepCommandWithCP56Time2a_create.restype = pStepCommandWithCP56Time2a
        return lib.StepCommandWithCP56Time2a_create(
            pStepCommandWithCP56Time2a(self),
            c_int(ioa),
            StepCommandValue(command),
            c_bool(selectCommand),
            c_int(qu),
            timestamp.pointer).contents

    def get_qu(self):
        lib.StepCommandWithCP56Time2a_getQU.restype = c_int
        return lib.StepCommandWithCP56Time2a_getQU(pStepCommandWithCP56Time2a(self))

    def get_value(self):
        return self.get_state()

    def get_state(self):
        lib.StepCommandWithCP56Time2a_getState.restype = StepCommandValue
        return lib.StepCommandWithCP56Time2a_getState(pStepCommandWithCP56Time2a(self)).contents

    def is_select(self):
        lib.StepCommandWithCP56Time2a_isSelect.restype = c_bool
        return lib.StepCommandWithCP56Time2a_isSelect(pStepCommandWithCP56Time2a(self))

pStepCommandWithCP56Time2a = ctypes.POINTER(StepCommandWithCP56Time2a)


class SetpointCommandNormalizedWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('qos', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, selectCommand, ql, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandNormalizedWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_TA_1.c_value
        self.create(ioa, value, selectCommand, ql, timestamp)

    def create(self, ioa, value, selectCommand, ql, timestamp):
        lib.SetpointCommandNormalizedWithCP56Time2a_create.restype = pSetpointCommandNormalizedWithCP56Time2a
        return lib.SetpointCommandNormalizedWithCP56Time2a_create(
            pSetpointCommandNormalizedWithCP56Time2a(self),
            c_int(ioa),
            c_float(value),
            c_bool(selectCommand),
            c_int(ql),
            timestamp.pointer).contents

    def get_value(self):
        lib.SetpointCommandNormalizedWithCP56Time2a_getValue.restype = c_float
        return lib.SetpointCommandNormalizedWithCP56Time2a_getValue(pSetpointCommandNormalizedWithCP56Time2a(self))

    def get_ql(self):
        lib.SetpointCommandNormalizedWithCP56Time2a_getQL.restype = c_int
        return lib.SetpointCommandNormalizedWithCP56Time2a_getQL(pSetpointCommandNormalizedWithCP56Time2a(self))

    def is_select(self):
        lib.SetpointCommandNormalizedWithCP56Time2a_isSelect.restype = c_bool
        return lib.SetpointCommandNormalizedWithCP56Time2a_isSelect(pSetpointCommandNormalizedWithCP56Time2a(self))

pSetpointCommandNormalizedWithCP56Time2a = ctypes.POINTER(SetpointCommandNormalizedWithCP56Time2a)


class SetpointCommandScaledWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('encodedValue', ctypes.c_uint8 * 2),
        ('qos', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, selectCommand, ql, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandScaledWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_TB_1.c_value
        self.create(ioa, value, selectCommand, ql, timestamp)

    def create(self, ioa, value, selectCommand, ql, timestamp):
        lib.SetpointCommandScaledWithCP56Time2a_create.restype = pSetpointCommandScaledWithCP56Time2a
        return lib.SetpointCommandScaledWithCP56Time2a_create(
            pSetpointCommandScaledWithCP56Time2a(self),
            c_int(ioa),
            c_int(value),
            c_bool(selectCommand),
            c_int(ql),
            timestamp.pointer).contents

    def get_value(self):
        lib.SetpointCommandScaledWithCP56Time2a_getValue.restype = c_int
        return lib.SetpointCommandScaledWithCP56Time2a_getValue(pSetpointCommandScaledWithCP56Time2a(self))

    def get_ql(self):
        lib.SetpointCommandScaledWithCP56Time2a_getQL.restype = c_int
        return lib.SetpointCommandScaledWithCP56Time2a_getQL(pSetpointCommandScaledWithCP56Time2a(self))

    def is_select(self):
        lib.SetpointCommandScaledWithCP56Time2a_isSelect.restype = c_bool
        return lib.SetpointCommandScaledWithCP56Time2a_isSelect(pSetpointCommandScaledWithCP56Time2a(self))

pSetpointCommandScaledWithCP56Time2a = ctypes.POINTER(SetpointCommandScaledWithCP56Time2a)


class SetpointCommandShortWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_float),
        ('qos', c_uint8),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, selectCommand, ql, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.setpointCommandShortWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_SE_TC_1.c_value
        self.create(ioa, value, selectCommand, ql, timestamp)

    def create(self, ioa, value, selectCommand, ql, timestamp):
        lib.SetpointCommandShortWithCP56Time2a_create.restype = pSetpointCommandShortWithCP56Time2a
        return lib.SetpointCommandShortWithCP56Time2a_create(
            pSetpointCommandShortWithCP56Time2a(self),
            c_int(ioa),
            c_float(value),
            c_bool(selectCommand),
            c_int(ql),
            timestamp.pointer).contents

    def get_value(self):
        lib.SetpointCommandShortWithCP56Time2a_getValue.restype = c_float
        return lib.SetpointCommandShortWithCP56Time2a_getValue(pSetpointCommandShortWithCP56Time2a(self))

    def get_ql(self):
        lib.SetpointCommandShortWithCP56Time2a_getQL.restype = c_int
        return lib.SetpointCommandShortWithCP56Time2a_getQL(pSetpointCommandShortWithCP56Time2a(self))

    def is_select(self):
        lib.SetpointCommandShortWithCP56Time2a_isSelect.restype = c_bool
        return lib.SetpointCommandShortWithCP56Time2a_isSelect(pSetpointCommandShortWithCP56Time2a(self))

pSetpointCommandShortWithCP56Time2a = ctypes.POINTER(SetpointCommandShortWithCP56Time2a)


class Bitstring32CommandWithCP56Time2a(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('value', c_uint32),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, value, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.bitstring32CommandWithCP56Time2aVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_BO_TA_1.c_value
        self.create(ioa, value, timestamp)

    def create(self, ioa, value, timestamp):
        lib.Bitstring32CommandWithCP56Time2a_create.restype = pBitstring32CommandWithCP56Time2a
        return lib.Bitstring32CommandWithCP56Time2a_create(
            pBitstring32CommandWithCP56Time2a(self),
            c_int(ioa),
            c_uint32(value),
            timestamp.pointer).contents

    def get_value(self):
        lib.Bitstring32CommandWithCP56Time2a_getValue.restype = c_uint32
        return lib.Bitstring32CommandWithCP56Time2a_getValue(pBitstring32CommandWithCP56Time2a(self))

    def get_timestamp(self):
        lib.Bitstring32CommandWithCP56Time2a_getTimestamp.restype = pCP56Time2a
        return lib.Bitstring32CommandWithCP56Time2a_getTimestamp(pBitstring32CommandWithCP56Time2a(self)).contents

pBitstring32CommandWithCP56Time2a = ctypes.POINTER(Bitstring32CommandWithCP56Time2a)


class EndOfInitialization(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('coi', c_uint8)
        ]

    def __init__(self, coi):
        self.virtualFunctionTable = ctypes.cast(lib.endOfInitializationVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.M_EI_NA_1.c_value
        self.create(coi)

    def create(self, coi):
        lib.EndOfInitialization_create.restype = pEndOfInitialization
        return lib.EndOfInitialization_create(
            pEndOfInitialization(self),
            c_uint8(coi)).contents

    def get_coi(self):
        lib.EndOfInitialization_getCOI.restype = c_uint8
        return lib.EndOfInitialization_getCOI(pEndOfInitialization(self))

pEndOfInitialization = ctypes.POINTER(EndOfInitialization)


class InterrogationCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('qoi', c_uint8)
        ]

    def __init__(self, ioa, qoi):
        self.virtualFunctionTable = ctypes.cast(lib.interrogationCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_IC_NA_1.c_value
        self.create(ioa, qoi)

    def create(self, ioa, qoi):
        lib.InterrogationCommand_create.restype = pInterrogationCommand
        return lib.InterrogationCommand_create(
            pInterrogationCommand(self),
            c_int(ioa),
            c_uint8(qoi)).contents

    def get_qoi(self):
        lib.InterrogationCommand_getQOI.restype = c_uint8
        return lib.InterrogationCommand_getQOI(pInterrogationCommand(self))

pInterrogationCommand = ctypes.POINTER(InterrogationCommand)


class CounterInterrogationCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('qcc', c_uint8)
        ]

    def __init__(self, ioa, qcc):
        self.virtualFunctionTable = ctypes.cast(lib.counterInterrogationCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_CI_NA_1.c_value
        self.create(ioa, qcc)

    def create(self, ioa, qcc):
        lib.CounterInterrogationCommand_create.restype = pCounterInterrogationCommand
        return lib.CounterInterrogationCommand_create(
            pCounterInterrogationCommand(self),
            c_int(ioa),
            pQualifierOfCIC(qcc)).contents

    def get_qcc(self):
        lib.CounterInterrogationCommand_getQCC.restype = pQualifierOfCIC
        return lib.CounterInterrogationCommand_getQCC(pCounterInterrogationCommand(self)).contents

pCounterInterrogationCommand = ctypes.POINTER(CounterInterrogationCommand)


class ReadCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT)
        ]

    def __init__(self, ioa):
        self.virtualFunctionTable = ctypes.cast(lib.readCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_RD_NA_1.c_value
        self.create(ioa)

    def create(self, ioa):
        lib.ReadCommand_create.restype = pReadCommand
        return lib.ReadCommand_create(
            pReadCommand(self),
            c_int(ioa)).contents

pReadCommand = ctypes.POINTER(ReadCommand)


class ClockSynchronizationCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('timestamp', CP56Time2a)
        ]

    def __init__(self, ioa, timestamp):
        self.virtualFunctionTable = ctypes.cast(lib.clockSynchronizationCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_CS_NA_1.c_value
        self.create(ioa, timestamp)

    def create(self, ioa, timestamp):
        lib.ClockSynchronizationCommand_create.restype = pClockSynchronizationCommand
        return lib.ClockSynchronizationCommand_create(
            pClockSynchronizationCommand(self),
            c_int(ioa),
            timestamp.pointer).contents

    def get_time(self):
        lib.ClockSynchronizationCommand_getTime.restype = pCP56Time2a
        return lib.ClockSynchronizationCommand_getTime(pClockSynchronizationCommand(self)).contents

pClockSynchronizationCommand = ctypes.POINTER(ClockSynchronizationCommand)


class ResetProcessCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('qrp', QualifierOfRPC)
        ]

    def __init__(self, ioa, qrp):
        self.virtualFunctionTable = ctypes.cast(lib.resetProcessCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_RP_NA_1.c_value
        self.create(ioa, qrp)

    def create(self, ioa, qrp):
        lib.ResetProcessCommand_create.restype = pResetProcessCommand
        return lib.ResetProcessCommand_create(
            pResetProcessCommand(self),
            c_int(ioa),
            QualifierOfRPC(qrp)).contents

    def get_qrp(self):
        lib.ResetProcessCommand_getQRP.restype = QualifierOfRPC
        return lib.ResetProcessCommand_getQRP(pResetProcessCommand(self))

pResetProcessCommand = ctypes.POINTER(ResetProcessCommand)


class DelayAcquisitionCommand(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('delay', CP16Time2a)
        ]

    def __init__(self, ioa, delay):
        self.virtualFunctionTable = ctypes.cast(lib.delayAcquisitionCommandVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.C_CD_NA_1.c_value
        self.create(ioa, delay)

    def create(self, ioa, delay):
        lib.DelayAcquisitionCommand_create.restype = pDelayAcquisitionCommand
        return lib.DelayAcquisitionCommand_create(
            pDelayAcquisitionCommand(self),
            c_int(ioa),
            pCP16Time2a(delay)).contents

    def get_delay(self):
        lib.DelayAcquisitionCommand_getDelay.restype = pCP16Time2a
        return lib.DelayAcquisitionCommand_getDelay(pDelayAcquisitionCommand(self)).contents

pDelayAcquisitionCommand = ctypes.POINTER(DelayAcquisitionCommand)


class ParameterNormalizedValue(MeasuredValueNormalized):
    def __init__(self, ioa, value, qpm):
        self.virtualFunctionTable = ctypes.cast(lib.parameterNormalizedValueVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.P_ME_NA_1.c_value
        self.create(ioa, value, qpm)

    def create(self, ioa, value, qpm):
        lib.ParameterNormalizedValue_create.restype = pParameterNormalizedValue
        return lib.ParameterNormalizedValue_create(
            pParameterNormalizedValue(self),
            c_int(ioa),
            c_float(value),
            pQualifierOfParameterMV(qpm)).contents

    def get_value(self):
        lib.ParameterNormalizedValue_getValue.restype = c_float
        return lib.ParameterNormalizedValue_getValue(pParameterNormalizedValue(self))

    def set_value(self, value):
        lib.ParameterNormalizedValue_setValue(
            pParameterNormalizedValue(self),
            c_float(value))

    def get_qpm(self):
        lib.ParameterNormalizedValue_getQPM.restype = pQualifierOfParameterMV
        return lib.ParameterNormalizedValue_getQPM(pParameterNormalizedValue(self)).contents

pParameterNormalizedValue = ctypes.POINTER(ParameterNormalizedValue)


class ParameterScaledValue(MeasuredValueScaled):
    def __init__(self, ioa, value, qpm):
        self.virtualFunctionTable = ctypes.cast(lib.parameterScaledValueVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.P_ME_NB_1.c_value
        self.create(ioa, value, qpm)

    def create(self, ioa, value, qpm):
        lib.ParameterScaledValue_create.restype = pParameterScaledValue
        return lib.ParameterScaledValue_create(
            pParameterScaledValue(self),
            c_int(ioa),
            c_int(value),
            pQualifierOfParameterMV(qpm)).contents

    def get_value(self):
        lib.ParameterScaledValue_getValue.restype = c_int
        return lib.ParameterScaledValue_getValue(pParameterScaledValue(self))

    def set_value(self, value):
        lib.ParameterScaledValue_setValue(
            pParameterScaledValue(self),
            c_int(value))

    def get_qpm(self):
        lib.ParameterScaledValue_getQPM.restype = pQualifierOfParameterMV
        return lib.ParameterScaledValue_getQPM(pParameterScaledValue(self)).contents

pParameterScaledValue = ctypes.POINTER(ParameterScaledValue)


class ParameterFloatValue(MeasuredValueShort):
    def __init__(self, ioa, value, qpm):
        self.virtualFunctionTable = ctypes.cast(lib.parameterFloatValueVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.P_ME_NC_1.c_value
        self.create(ioa, value, qpm)

    def create(self, ioa, value, qpm):
        lib.ParameterFloatValue_create.restype = pParameterFloatValue
        return lib.ParameterFloatValue_create(
            pParameterFloatValue(self),
            c_int(ioa),
            c_float(value),
            pQualifierOfParameterMV(qpm)).contents

    def get_value(self):
        lib.ParameterFloatValue_getValue.restype = c_float
        return lib.ParameterFloatValue_getValue(pParameterFloatValue(self))

    def set_value(self, value):
        lib.ParameterFloatValue_setValue(
            pParameterFloatValue(self),
            c_float(value))

    def get_qpm(self):
        lib.ParameterFloatValue_getQPM.restype = pQualifierOfParameterMV
        return lib.ParameterFloatValue_getQPM(pParameterFloatValue(self)).contents

pParameterFloatValue = ctypes.POINTER(ParameterFloatValue)


class ParameterActivation(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('qpa', QualifierOfParameterActivation)
        ]

    def __init__(self, ioa, qpa):
        self.virtualFunctionTable = ctypes.cast(lib.parameterActivationVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.P_AC_NA_1.c_value
        self.create(ioa, qpa)

    def create(self, ioa, qpa):
        lib.ParameterActivation_create.restype = pParameterActivation
        return lib.ParameterActivation_create(
            pParameterActivation(self),
            c_int(ioa),
            QualifierOfParameterActivation(qpa)).contents

    def get_quality(self):
        lib.ParameterActivation_getQuality.restype = QualifierOfParameterActivation
        value = lib.ParameterActivation_getQuality(pParameterActivation(self))
        return QualityDescriptor(value)

pParameterActivation = ctypes.POINTER(ParameterActivation)


class FileReady(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('lengthOfFile', c_uint32),
        ('frq', c_uint8)
        ]

    def __init__(self, ioa, nof, lengthOfFile, positive):
        self.virtualFunctionTable = ctypes.cast(lib.fileReadyVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_FR_NA_1.c_value
        self.create(ioa, nof, lengthOfFile, positive)

    def create(self, ioa, nof, lengthOfFile, positive):
        lib.FileReady_create.restype = pFileReady
        return lib.FileReady_create(
            pFileReady(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint32(lengthOfFile),
            c_bool(positive)).contents

    def get_frq(self):
        lib.FileReady_getFRQ.restype = c_uint8
        return lib.FileReady_getFRQ(pFileReady(self))

    def set_frq(self, frq):
        lib.FileReady_setFRQ(
            pFileReady(self),
            c_uint8(frq))

    def is_positive(self):
        lib.FileReady_isPositive.restype = c_bool
        return lib.FileReady_isPositive(pFileReady(self))

    def get_nof(self):
        lib.FileReady_getNOF.restype = c_uint16
        return lib.FileReady_getNOF(pFileReady(self))

    def get_length_of_file(self):
        lib.FileReady_getLengthOfFile.restype = c_uint32
        return lib.FileReady_getLengthOfFile(pFileReady(self))

pFileReady = ctypes.POINTER(FileReady)


class SectionReady(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('nameOfSection', c_uint8),
        ('lengthOfSection', c_uint32),
        ('srq', c_uint8)
        ]

    def __init__(self, ioa, nof, nos, lengthOfSection, notReady):
        self.virtualFunctionTable = ctypes.cast(lib.sectionReadyVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_SR_NA_1.c_value
        self.create(ioa, nof, nos, lengthOfSection, notReady)

    def create(self, ioa, nof, nos, lengthOfSection, notReady):
        lib.SectionReady_create.restype = pSectionReady
        return lib.SectionReady_create(
            pSectionReady(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint8(nos),
            c_uint32(lengthOfSection),
            c_bool(notReady)).contents

    def is_not_ready(self):
        lib.SectionReady_isNotReady.restype = c_bool
        return lib.SectionReady_isNotReady(pSectionReady(self))

    def get_srq(self):
        lib.SectionReady_getSRQ.restype = c_uint8
        return lib.SectionReady_getSRQ(pSectionReady(self))

    def set_srq(self, srq):
        lib.SectionReady_setSRQ(
            pSectionReady(self),
            c_uint8(srq))

    def get_nof(self):
        lib.SectionReady_getNOF.restype = c_uint16
        return lib.SectionReady_getNOF(pSectionReady(self))

    def get_name_of_section(self):
        lib.SectionReady_getNameOfSection.restype = c_uint8
        return lib.SectionReady_getNameOfSection(pSectionReady(self))

    def get_length_of_section(self):
        lib.SectionReady_getLengthOfSection.restype = c_uint32
        return lib.SectionReady_getLengthOfSection(pSectionReady(self))

pSectionReady = ctypes.POINTER(SectionReady)


class FileCallOrSelect(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('nameOfSection', c_uint8),
        ('scq', c_uint8)
        ]

    def __init__(self, ioa, nof, nos, scq):
        self.virtualFunctionTable = ctypes.cast(lib.fileCallOrSelectVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_SC_NA_1.c_value
        self.create(ioa, nof, nos, scq)

    def create(self, ioa, nof, nos, scq):
        lib.FileCallOrSelect_create.restype = pFileCallOrSelect
        return lib.FileCallOrSelect_create(
            pFileCallOrSelect(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint8(nos),
            c_uint8(scq)).contents

    def get_nof(self):
        lib.FileCallOrSelect_getNOF.restype = c_uint16
        return lib.FileCallOrSelect_getNOF(pFileCallOrSelect(self))

    def get_name_of_section(self):
        lib.FileCallOrSelect_getNameOfSection.restype = c_uint8
        return lib.FileCallOrSelect_getNameOfSection(pFileCallOrSelect(self))

    def get_scq(self):
        lib.FileCallOrSelect_getSCQ.restype = c_uint8
        return lib.FileCallOrSelect_getSCQ(pFileCallOrSelect(self))

pFileCallOrSelect = ctypes.POINTER(FileCallOrSelect)


class FileLastSegmentOrSection(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('nameOfSection', c_uint8),
        ('lsq', c_uint8),
        ('chs', c_uint8)
        ]

    def __init__(self, ioa, nof, nos, lsq, chs):
        self.virtualFunctionTable = ctypes.cast(lib.fileLastSegmentOrSectionVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_LS_NA_1.c_value
        self.create(ioa, nof, nos, lsq, chs)

    def create(self, ioa, nof, nos, lsq, chs):
        lib.FileLastSegmentOrSection_create.restype = pFileLastSegmentOrSection
        return lib.FileLastSegmentOrSection_create(
            pFileLastSegmentOrSection(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint8(nos),
            c_uint8(lsq),
            c_uint8(chs)).contents

    def get_nof(self):
        lib.FileLastSegmentOrSection_getNOF.restype = c_uint16
        return lib.FileLastSegmentOrSection_getNOF(pFileLastSegmentOrSection(self))

    def get_name_of_section(self):
        lib.FileLastSegmentOrSection_getNameOfSection.restype = c_uint8
        return lib.FileLastSegmentOrSection_getNameOfSection(pFileLastSegmentOrSection(self))

    def get_lsq(self):
        lib.FileLastSegmentOrSection_getLSQ.restype = c_uint8
        return lib.FileLastSegmentOrSection_getLSQ(pFileLastSegmentOrSection(self))

    def get_chs(self):
        lib.FileLastSegmentOrSection_getCHS.restype = c_uint8
        return lib.FileLastSegmentOrSection_getCHS(pFileLastSegmentOrSection(self))

pFileLastSegmentOrSection = ctypes.POINTER(FileLastSegmentOrSection)


class FileACK(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('nameOfSection', c_uint8),
        ('afq', c_uint8)
        ]

    def __init__(self, ioa, nof, nos, afq):
        self.virtualFunctionTable = ctypes.cast(lib.fileACKVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_AF_NA_1.c_value
        self.create(ioa, nof, nos, afq)

    def create(self, ioa, nof, nos, afq):
        lib.FileACK_create.restype = pFileACK
        return lib.FileACK_create(
            pFileACK(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint8(nos),
            c_uint8(afq)).contents

    def get_nof(self):
        lib.FileACK_getNOF.restype = c_uint16
        return lib.FileACK_getNOF(pFileACK(self))

    def get_name_of_section(self):
        lib.FileACK_getNameOfSection.restype = c_uint8
        return lib.FileACK_getNameOfSection(pFileACK(self))

    def get_afq(self):
        lib.FileACK_getAFQ.restype = c_uint8
        return lib.FileACK_getAFQ(pFileACK(self))

pFileACK = ctypes.POINTER(FileACK)


class FileSegment(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('nameOfSection', c_uint8),
        ('los', c_uint8),
        ('data', ctypes.POINTER(c_uint8))
        ]

    def __init__(self, ioa, nof, nos, data, los):
        self.virtualFunctionTable = ctypes.cast(lib.fileSegmentVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_SG_NA_1.c_value
        self.create(ioa, nof, nos, data, los)

    def create(self, ioa, nof, nos, data, los):
        lib.FileSegment_create.restype = pFileSegment
        return lib.FileSegment_create(
            pFileSegment(self),
            c_int(ioa),
            c_uint16(nof),
            c_uint8(nos),
            ctypes.POINTER(c_uint8)(data),
            c_uint8(los)).contents

    def get_nof(self):
        lib.FileSegment_getNOF.restype = c_uint16
        return lib.FileSegment_getNOF(pFileSegment(self))

    def get_name_of_section(self):
        lib.FileSegment_getNameOfSection.restype = c_uint8
        return lib.FileSegment_getNameOfSection(pFileSegment(self))

    def get_length_of_segment(self):
        lib.FileSegment_getLengthOfSegment.restype = c_uint8
        return lib.FileSegment_getLengthOfSegment(pFileSegment(self))

    def get_segment_data(self):
        lib.FileSegment_getSegmentData.restype = ctypes.POINTER(c_uint8)
        return lib.FileSegment_getSegmentData(pFileSegment(self))

    def get_max_data_size(parameters):
        lib.FileSegment_GetMaxDataSize.restype = c_int
        return lib.FileSegment_GetMaxDataSize(parameters.pointer)

pFileSegment = ctypes.POINTER(FileSegment)


class FileDirectory(ctypes.Structure, IOBase):
    _fields_ = [
        ('objectAddress', c_int),
        ('type', cTypeId),
        ('virtualFunctionTable', pInformationObjectVFT),
        ('nof', c_uint16),
        ('lengthOfFile', c_int),
        ('sof', c_uint8),
        ('creationTime', CP56Time2a)
        ]

    def __init__(self, ioa, nof, lengthOfFile, sof, creationTime):
        self.virtualFunctionTable = ctypes.cast(lib.fileDirectoryVFT, pInformationObjectVFT)
        self.type = lib60870.TypeID.F_DR_TA_1.c_value
        self.create(ioa, nof, lengthOfFile, sof, creationTime)

    def create(self, ioa, nof, lengthOfFile, sof, creationTime):
        lib.FileDirectory_create.restype = pFileDirectory
        return lib.FileDirectory_create(
            pFileDirectory(self),
            c_int(ioa),
            c_uint16(nof),
            c_int(lengthOfFile),
            c_uint8(sof),
            pCP56Time2a(creationTime)).contents

    def get_nof(self):
        lib.FileDirectory_getNOF.restype = c_uint16
        return lib.FileDirectory_getNOF(pFileDirectory(self))

    def get_sof(self):
        lib.FileDirectory_getSOF.restype = c_uint8
        return lib.FileDirectory_getSOF(pFileDirectory(self))

    def get_status(self):
        lib.FileDirectory_getSTATUS.restype = c_int
        return lib.FileDirectory_getSTATUS(pFileDirectory(self))

    def get_lfd(self):
        lib.FileDirectory_getLFD.restype = c_bool
        return lib.FileDirectory_getLFD(pFileDirectory(self))

    def get_for(self):
        lib.FileDirectory_getFOR.restype = c_bool
        return lib.FileDirectory_getFOR(pFileDirectory(self))

    def get_fa(self):
        lib.FileDirectory_getFA.restype = c_bool
        return lib.FileDirectory_getFA(pFileDirectory(self))

    def get_length_of_file(self):
        lib.FileDirectory_getLengthOfFile.restype = c_uint8
        return lib.FileDirectory_getLengthOfFile(pFileDirectory(self))

    def get_creation_time(self):
        lib.FileDirectory_getCreationTime.restype = pCP56Time2a
        return lib.FileDirectory_getCreationTime(pFileDirectory(self)).contents

pFileDirectory = ctypes.POINTER(FileDirectory)


_type_id_type_lookup = [
    ((lib60870.TypeID.INVALID), (InformationObject)),
    ((lib60870.TypeID.M_SP_NA_1), (SinglePointInformation)),  # 1
    ((lib60870.TypeID.M_SP_TA_1), (SinglePointWithCP24Time2a)),  # 2
    ((lib60870.TypeID.M_DP_NA_1), (DoublePointInformation)),  # 3
    ((lib60870.TypeID.M_DP_TA_1), (DoublePointWithCP24Time2a)),  # 4
    ((lib60870.TypeID.M_ST_NA_1), (StepPositionInformation)),  # 5
    ((lib60870.TypeID.M_ST_TA_1), (StepPositionWithCP24Time2a)),  # 6
    ((lib60870.TypeID.M_BO_NA_1), (BitString32)),  # 7
    ((lib60870.TypeID.M_BO_TA_1), (Bitstring32WithCP24Time2a)),  # 8
    ((lib60870.TypeID.M_ME_NA_1), (MeasuredValueNormalized)),  # 9
    ((lib60870.TypeID.M_ME_TA_1), (MeasuredValueNormalizedWithCP24Time2a)),  # 10
    ((lib60870.TypeID.M_ME_NB_1), (MeasuredValueScaled)),  # 11
    ((lib60870.TypeID.M_ME_TB_1), (MeasuredValueScaledWithCP24Time2a)),  # 12
    ((lib60870.TypeID.M_ME_NC_1), (MeasuredValueShort)),  # 13
    ((lib60870.TypeID.M_ME_TC_1), (MeasuredValueShortWithCP24Time2a)),  # 14
    ((lib60870.TypeID.M_IT_NA_1), (IntegratedTotals)),  # 15
    ((lib60870.TypeID.M_IT_TA_1), (IntegratedTotalsWithCP24Time2a)),  # 16
    ((lib60870.TypeID.M_EP_TA_1), (EventOfProtectionEquipment)),  # 17
    ((lib60870.TypeID.M_EP_TB_1), (PackedStartEventsOfProtectionEquipment)),  # 18
    ((lib60870.TypeID.M_EP_TC_1), (PackedOutputCircuitInfo)),  # 19
    ((lib60870.TypeID.M_PS_NA_1), (PackedSinglePointWithSCD)),  # 20
    ((lib60870.TypeID.M_ME_ND_1), (MeasuredValueNormalizedWithoutQuality)),  # 21
    ((lib60870.TypeID.M_SP_TB_1), (SinglePointWithCP56Time2a)),  # 30
    ((lib60870.TypeID.M_DP_TB_1), (DoublePointWithCP56Time2a)),  # 31
    ((lib60870.TypeID.M_ST_TB_1), (StepPositionWithCP56Time2a)),  # 32
    ((lib60870.TypeID.M_BO_TB_1), (Bitstring32WithCP56Time2a)),  # 33
    ((lib60870.TypeID.M_ME_TD_1), (MeasuredValueNormalizedWithCP56Time2a)),  # 34
    ((lib60870.TypeID.M_ME_TE_1), (MeasuredValueScaledWithCP56Time2a)),  # 35
    ((lib60870.TypeID.M_ME_TF_1), (MeasuredValueShortWithCP56Time2a)),  # 36
    ((lib60870.TypeID.M_IT_TB_1), (IntegratedTotalsWithCP56Time2a)),  # 37
    ((lib60870.TypeID.M_EP_TD_1), (EventOfProtectionEquipmentWithCP56Time2a)),  # 38
    ((lib60870.TypeID.M_EP_TE_1), (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)),  # 39
    ((lib60870.TypeID.M_EP_TF_1), (PackedOutputCircuitInfoWithCP56Time2a)),  # 40
    # 41 - 44 reserved
    ((lib60870.TypeID.C_SC_NA_1), (SingleCommand)),  # 45
    ((lib60870.TypeID.C_DC_NA_1), (DoubleCommand)),  # 46
    ((lib60870.TypeID.C_RC_NA_1), (StepCommand)),  # 47
    ((lib60870.TypeID.C_SE_NA_1), (SetpointCommandNormalized)),  # 48 - Set-point command, normalized value
    ((lib60870.TypeID.C_SE_NB_1), (SetpointCommandScaled)),  # 49 - Set-point command, scaled value
    ((lib60870.TypeID.C_SE_NC_1), (SetpointCommandShort)),  # 50 - Set-point command, short floating point number
    ((lib60870.TypeID.C_BO_NA_1), (Bitstring32Command)),  # 51 - Bitstring command
    # 52 - 57 reserved
    ((lib60870.TypeID.C_SC_TA_1), (SingleCommandWithCP56Time2a)),  # 58 - Single command with CP56Time2a
    ((lib60870.TypeID.C_DC_TA_1), (DoubleCommandWithCP56Time2a)),  # 59 - Double command with CP56Time2a
    ((lib60870.TypeID.C_RC_TA_1), (StepCommandWithCP56Time2a)),  # 60 - Step command with CP56Time2a
    ((lib60870.TypeID.C_SE_TA_1), (SetpointCommandNormalizedWithCP56Time2a)),
    # 61 - Setpoint command, normalized value with CP56Time2a
    ((lib60870.TypeID.C_SE_TB_1), (SetpointCommandScaledWithCP56Time2a)),
    # 62 - Setpoint command, scaled value with CP56Time2a
    ((lib60870.TypeID.C_SE_TC_1), (SetpointCommandShortWithCP56Time2a)),
    # 63 - Setpoint command, short value with CP56Time2a
    ((lib60870.TypeID.C_BO_TA_1), (Bitstring32CommandWithCP56Time2a)),  # 64 - Bitstring command with CP56Time2a
    ((lib60870.TypeID.M_EI_NA_1), (EndOfInitialization)),  # 70 - End of Initialization
    ((lib60870.TypeID.C_IC_NA_1), (InterrogationCommand)),  # 100 - Interrogation command
    ((lib60870.TypeID.C_CI_NA_1), (CounterInterrogationCommand)),  # 101 - Counter interrogation command
    ((lib60870.TypeID.C_RD_NA_1), (ReadCommand)),  # 102 - Read command
    ((lib60870.TypeID.C_CS_NA_1), (ClockSynchronizationCommand)),  # 103 - Clock synchronization command
    # ((lib60870.TypeID.C_TS_NA_1), (TestCommand)), #104
    ((lib60870.TypeID.C_RP_NA_1), (ResetProcessCommand)),  # 105 - Reset process command
    ((lib60870.TypeID.C_CD_NA_1), (DelayAcquisitionCommand)),  # 106 - Delay acquisition command
    ((lib60870.TypeID.P_ME_NA_1), (ParameterNormalizedValue)),  # 110 - Parameter of measured values, normalized value
    ((lib60870.TypeID.P_ME_NB_1), (ParameterScaledValue)),  # 111 - Parameter of measured values, scaled value
    ((lib60870.TypeID.P_ME_NC_1), (ParameterFloatValue)),
    # 112 - Parameter of measured values, short floating point number
    ((lib60870.TypeID.P_AC_NA_1), (ParameterActivation)),  # 113 - Parameter for activation
    ((lib60870.TypeID.F_FR_NA_1), (FileReady)),  # 120 - File ready
    ((lib60870.TypeID.F_SR_NA_1), (SectionReady)),  # 121 - Section ready
    ((lib60870.TypeID.F_SC_NA_1), (FileCallOrSelect)),  # 122 - Call/Select directory/file/section
    ((lib60870.TypeID.F_LS_NA_1), (FileLastSegmentOrSection)),  # 123 - Last segment/section
    ((lib60870.TypeID.F_AF_NA_1), (FileACK)),  # 124 -  ACK file/section
    ((lib60870.TypeID.F_SG_NA_1), (FileSegment)),  # 125 - File segment
    ((lib60870.TypeID.F_DR_TA_1), (FileDirectory)),  # 126 - File directory
]


def get_io_type_from_type_id(type_id):
    """
    Look-up function to get the (python)type for a given TypeID
    """
    return dict(_type_id_type_lookup).get(type_id, InformationObject)


def get_type_id_from_name(type_name):
    """
    Look-up function to get a TypeID for a given type name
    """
    lookup = dict([(item[1].__name__, item[0]) for item in _type_id_type_lookup])
    return lookup.get(type_name)
