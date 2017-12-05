import ctypes
from ctypes import c_char_p, c_uint16, c_void_p, c_int, c_bool
from ctypes import Structure

from ctypes.util import find_library
import logging
import time
import enum

import platform
if platform.system() == 'Windows':
    from ctypes import windll as cdll
else:
    from ctypes import cdll

logger = logging.getLogger(__name__)


lib_location = None
lib_location = lib_location or find_library('iec60870')
if not lib_location:
    msg = "can't find library. If installed, try running ldconfig"
    raise ValueError(msg)

lib = cdll.LoadLibrary(lib_location)


IEC_60870_5_104_DEFAULT_PORT = 2404
IEC_60870_5_104_DEFAULT_TLS_PORT = 19998


class IEC60870_5_TypeID(enum.Enum):
    INVALID = 0
    M_SP_NA_1 = 1
    M_SP_TA_1 = 2
    M_DP_NA_1 = 3
    M_DP_TA_1 = 4
    M_ST_NA_1 = 5
    M_ST_TA_1 = 6
    M_BO_NA_1 = 7
    M_BO_TA_1 = 8
    M_ME_NA_1 = 9
    M_ME_TA_1 = 10
    M_ME_NB_1 = 11
    M_ME_TB_1 = 12
    M_ME_NC_1 = 13
    M_ME_TC_1 = 14
    M_IT_NA_1 = 15
    M_IT_TA_1 = 16
    M_EP_TA_1 = 17
    M_EP_TB_1 = 18
    M_EP_TC_1 = 19
    M_PS_NA_1 = 20
    M_ME_ND_1 = 21
    M_SP_TB_1 = 30
    M_DP_TB_1 = 31
    M_ST_TB_1 = 32
    M_BO_TB_1 = 33
    M_ME_TD_1 = 34
    M_ME_TE_1 = 35
    M_ME_TF_1 = 36
    M_IT_TB_1 = 37
    M_EP_TD_1 = 38
    M_EP_TE_1 = 39
    M_EP_TF_1 = 40
    C_SC_NA_1 = 45
    C_DC_NA_1 = 46
    C_RC_NA_1 = 47
    C_SE_NA_1 = 48
    C_SE_NB_1 = 49
    C_SE_NC_1 = 50
    C_BO_NA_1 = 51
    C_SC_TA_1 = 58
    C_DC_TA_1 = 59
    C_RC_TA_1 = 60
    C_SE_TA_1 = 61
    C_SE_TB_1 = 62
    C_SE_TC_1 = 63
    C_BO_TA_1 = 64
    M_EI_NA_1 = 70
    C_IC_NA_1 = 100
    C_CI_NA_1 = 101
    C_RD_NA_1 = 102
    C_CS_NA_1 = 103
    C_TS_NA_1 = 104
    C_RP_NA_1 = 105
    C_CD_NA_1 = 106
    C_TS_TA_1 = 107
    P_ME_NA_1 = 110
    P_ME_NB_1 = 111
    P_ME_NC_1 = 112
    P_AC_NA_1 = 113
    F_FR_NA_1 = 120
    F_SR_NA_1 = 121
    F_SC_NA_1 = 122
    F_LS_NA_1 = 123
    F_AF_NA_1 = 124
    F_SG_NA_1 = 125
    F_DR_TA_1 = 126
    F_SC_NB_1 = 127

    @property
    def c_value(self):
        return ctypes.c_int(self.value)

TypeID = IEC60870_5_TypeID


class IEC60870ConnectionEvent(enum.Enum):
    IEC60870_CONNECTION_OPENED = 0
    IEC60870_CONNECTION_CLOSED = 1
    IEC60870_CONNECTION_STARTDT_CON_RECEIVED = 2
    IEC60870_CONNECTION_STOPDT_CON_RECEIVED = 3

    @property
    def c_value(self):
        return ctypes.c_int(self.value)


class CauseOfTransmission(enum.Enum):
    INVALID = 0
    PERIODIC = 1
    BACKGROUND_SCAN = 2
    SPONTANEOUS = 3
    INITIALIZED = 4
    REQUEST = 5
    ACTIVATION = 6
    ACTIVATION_CON = 7
    DEACTIVATION = 8
    DEACTIVATION_CON = 9
    ACTIVATION_TERMINATION = 10
    RETURN_INFO_REMOTE = 11
    RETURN_INFO_LOCAL = 12
    FILE_TRANSFER = 13
    AUTHENTICATION = 14
    MAINTENANCE_OF_AUTH_SESSION_KEY = 15
    MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY = 16
    INTERROGATED_BY_STATION = 20
    INTERROGATED_BY_GROUP_1 = 21
    INTERROGATED_BY_GROUP_2 = 22
    INTERROGATED_BY_GROUP_3 = 23
    INTERROGATED_BY_GROUP_4 = 24
    INTERROGATED_BY_GROUP_5 = 25
    INTERROGATED_BY_GROUP_6 = 26
    INTERROGATED_BY_GROUP_7 = 27
    INTERROGATED_BY_GROUP_8 = 28
    INTERROGATED_BY_GROUP_9 = 29
    INTERROGATED_BY_GROUP_10 = 30
    INTERROGATED_BY_GROUP_11 = 31
    INTERROGATED_BY_GROUP_12 = 32
    INTERROGATED_BY_GROUP_13 = 33
    INTERROGATED_BY_GROUP_14 = 34
    INTERROGATED_BY_GROUP_15 = 35
    INTERROGATED_BY_GROUP_16 = 36
    REQUESTED_BY_GENERAL_COUNTER = 37
    REQUESTED_BY_GROUP_1_COUNTER = 38
    REQUESTED_BY_GROUP_2_COUNTER = 39
    REQUESTED_BY_GROUP_3_COUNTER = 40
    REQUESTED_BY_GROUP_4_COUNTER = 41
    UNKNOWN_TYPE_ID = 44
    UNKNOWN_CAUSE_OF_TRANSMISSION = 45
    UNKNOWN_COMMON_ADDRESS_OF_ASDU = 46
    UNKNOWN_INFORMATION_OBJECT_ADDRESS = 47

    @property
    def c_value(self):
        return ctypes.c_int(self.value)


class QualifierOfInterrogation(enum.Enum):
    "brief Qualifier of interrogation (QUI) according to IEC 60870-5-101:2003 7.2.6.22"
    # typedef uint8_t QualifierOfInterrogation;
    IEC60870_QOI_STATION = 20
    IEC60870_QOI_GROUP_1 = 21
    IEC60870_QOI_GROUP_2 = 22
    IEC60870_QOI_GROUP_3 = 23
    IEC60870_QOI_GROUP_4 = 24
    IEC60870_QOI_GROUP_5 = 25
    IEC60870_QOI_GROUP_6 = 26
    IEC60870_QOI_GROUP_7 = 27
    IEC60870_QOI_GROUP_8 = 28
    IEC60870_QOI_GROUP_9 = 29
    IEC60870_QOI_GROUP_10 = 30
    IEC60870_QOI_GROUP_11 = 31
    IEC60870_QOI_GROUP_12 = 32
    IEC60870_QOI_GROUP_13 = 33
    IEC60870_QOI_GROUP_14 = 34
    IEC60870_QOI_GROUP_15 = 35
    IEC60870_QOI_GROUP_16 = 36

    @property
    def c_value(self):
        return ctypes.c_int(self.value)


class QualityDescriptor(int):
    IEC60870_QUALITY_GOOD = 0  #
    IEC60870_QUALITY_OVERFLOW = 0x01  # QualityDescriptor
    IEC60870_QUALITY_RESERVED = 0x04  # QualityDescriptorP
    IEC60870_QUALITY_ELAPSED_TIME_INVALID = 0x08  # QualityDescriptorP
    IEC60870_QUALITY_BLOCKED = 0x10  # QualityDescriptor QualityDescriptorP
    IEC60870_QUALITY_SUBSTITUTED = 0x20  # QualityDescriptor QualityDescriptorP
    IEC60870_QUALITY_NON_TOPICAL = 0x40  # QualityDescriptor QualityDescriptorP
    IEC60870_QUALITY_INVALID = 0x80  # QualityDescriptor, QualityDescriptorP

    @property
    def c_value(self):
        return ctypes.c_uint8(self)

    @classmethod
    def from_c(cls, value):
        return cls(int(value.value))

QualityDescriptorP = QualityDescriptor


def get_library():
    return lib
