/*
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870-C
 *
 *  lib60870-C is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870-C is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870-C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#ifndef SRC_INC_IEC60870_COMMON_H_
#define SRC_INC_IEC60870_COMMON_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IEC_60870_5_104_DEFAULT_PORT 2404

#define LIB60870_VERSION_MAJOR 0
#define LIB60870_VERSION_MINOR 9
#define LIB60870_VERSION_PATCH 3

typedef struct {
    int major;
    int minor;
    int patch;
} Lib60870VersionInfo;

typedef struct sASDU* ASDU;

typedef struct sCP16Time2a* CP16Time2a;

struct sCP16Time2a {
    uint8_t encodedValue[2];
};

typedef struct sCP24Time2a* CP24Time2a;

struct sCP24Time2a {
    uint8_t encodedValue[3];
};

typedef struct sCP56Time2a* CP56Time2a;

struct sCP56Time2a {
    uint8_t encodedValue[7];
};

typedef struct sBinaryCounterReading* BinaryCounterReading;

struct sBinaryCounterReading {
    uint8_t encodedValue[5];
};

typedef struct sConnectionParameters* ConnectionParameters;

typedef struct sT104ConnectionParameters* T104ConnectionParameters;

#include "information_objects.h"

struct sT104ConnectionParameters {
    /* Application layer parameters */
    int sizeOfTypeId;
    int sizeOfVSQ;
    int sizeOfCOT;
    int originatorAddress;
    int sizeOfCA;
    int sizeOfIOA;

    /* data link layer/protocol specific parameters */
    int k;
    int w;
    int t0;
    int t1;
    int t2;
    int t3;
};

struct sConnectionParameters {
    /* Application layer parameters */
    int sizeOfTypeId;
    int sizeOfVSQ;
    int sizeOfCOT;
    int originatorAddress;
    int sizeOfCA;
    int sizeOfIOA;

    /* data link layer/protocol specific parameters */
};

typedef enum {
    M_SP_NA_1 = 1,
    M_SP_TA_1 = 2,
    M_DP_NA_1 = 3,
    M_DP_TA_1 = 4,
    M_ST_NA_1 = 5,
    M_ST_TA_1 = 6,
    M_BO_NA_1 = 7,
    M_BO_TA_1 = 8,
    M_ME_NA_1 = 9,
    M_ME_TA_1 = 10,
    M_ME_NB_1 = 11,
    M_ME_TB_1 = 12,
    M_ME_NC_1 = 13,
    M_ME_TC_1 = 14,
    M_IT_NA_1 = 15,
    M_IT_TA_1 = 16,
    M_EP_TA_1 = 17,
    M_EP_TB_1 = 18,
    M_EP_TC_1 = 19,
    M_PS_NA_1 = 20,
    M_ME_ND_1 = 21,
    M_SP_TB_1 = 30,
    M_DP_TB_1 = 31,
    M_ST_TB_1 = 32,
    M_BO_TB_1 = 33,
    M_ME_TD_1 = 34,
    M_ME_TE_1 = 35,
    M_ME_TF_1 = 36,
    M_IT_TB_1 = 37,
    M_EP_TD_1 = 38,
    M_EP_TE_1 = 39,
    M_EP_TF_1 = 40,
    C_SC_NA_1 = 45,
    C_DC_NA_1 = 46,
    C_RC_NA_1 = 47,
    C_SE_NA_1 = 48,
    C_SE_NB_1 = 49,
    C_SE_NC_1 = 50,
    C_BO_NA_1 = 51,
    C_SC_TA_1 = 58,
    C_DC_TA_1 = 59,
    C_RC_TA_1 = 60,
    C_SE_TA_1 = 61,
    C_SE_TB_1 = 62,
    C_SE_TC_1 = 63,
    C_BO_TA_1 = 64,
    M_EI_NA_1 = 70,
    C_IC_NA_1 = 100,
    C_CI_NA_1 = 101,
    C_RD_NA_1 = 102,
    C_CS_NA_1 = 103,
    C_TS_NA_1 = 104,
    C_RP_NA_1 = 105,
    C_CD_NA_1 = 106,
    C_TS_TA_1 = 107,
    P_ME_NA_1 = 110,
    P_ME_NB_1 = 111,
    P_ME_NC_1 = 112,
    P_AC_NA_1 = 113,
    F_FR_NA_1 = 120,
    F_SR_NA_1 = 121,
    F_SC_NA_1 = 122,
    F_LS_NA_1 = 123,
    F_AF_NA_1 = 124,
    F_SG_NA_1 = 125,
    F_DR_TA_1 = 126,
    F_SC_NB_1 = 127
} IEC60870_5_TypeID;

typedef IEC60870_5_TypeID TypeID;

const char*
TypeID_toString(TypeID self);

typedef enum {
    PERIODIC = 1,
    BACKGROUND_SCAN = 2,
    SPONTANEOUS = 3,
    INITIALIZED = 4,
    REQUEST = 5,
    ACTIVATION = 6,
    ACTIVATION_CON = 7,
    DEACTIVATION = 8,
    DEACTIVATION_CON = 9,
    ACTIVATION_TERMINATION = 10,
    RETURN_INFO_REMOTE = 11,
    RETURN_INFO_LOCAL = 12,
    FILE_TRANSFER = 13,
    AUTHENTICATION = 14,
    MAINTENANCE_OF_AUTH_SESSION_KEY = 15,
    MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY = 16,
    INTERROGATED_BY_STATION = 20,
    INTERROGATED_BY_GROUP_1 = 21,
    INTERROGATED_BY_GROUP_2 = 22,
    INTERROGATED_BY_GROUP_3 = 23,
    INTERROGATED_BY_GROUP_4 = 24,
    INTERROGATED_BY_GROUP_5 = 25,
    INTERROGATED_BY_GROUP_6 = 26,
    INTERROGATED_BY_GROUP_7 = 27,
    INTERROGATED_BY_GROUP_8 = 28,
    INTERROGATED_BY_GROUP_9 = 29,
    INTERROGATED_BY_GROUP_10 = 30,
    INTERROGATED_BY_GROUP_11 = 31,
    INTERROGATED_BY_GROUP_12 = 32,
    INTERROGATED_BY_GROUP_13 = 33,
    INTERROGATED_BY_GROUP_14 = 34,
    INTERROGATED_BY_GROUP_15 = 35,
    INTERROGATED_BY_GROUP_16 = 36,
    REQUESTED_BY_GENERAL_COUNTER = 37,
    REQUESTED_BY_GROUP_1_COUNTER = 38,
    REQUESTED_BY_GROUP_2_COUNTER = 39,
    REQUESTED_BY_GROUP_3_COUNTER = 40,
    REQUESTED_BY_GROUP_4_COUNTER = 41,
    UNKNOWN_TYPE_ID = 44,
    UNKNOWN_CAUSE_OF_TRANSMISSION = 45,
    UNKNOWN_COMMON_ADDRESS_OF_ASDU = 46,
    UNKNOWN_INFORMATION_OBJECT_ADDRESS = 47
} CauseOfTransmission;

const char*
CauseOfTransmission_toString(CauseOfTransmission self);

void
Lib60870_enableDebugOutput(bool value);

Lib60870VersionInfo
Lib60870_getLibraryVersionInfo(void);

bool
ASDU_isTest(ASDU self);

void
ASDU_setTest(ASDU self, bool value);

bool
ASDU_isNegative(ASDU self);

void
ASDU_setNegative(ASDU self, bool value);

int
ASDU_getOA(ASDU self);

CauseOfTransmission
ASDU_getCOT(ASDU self);

void
ASDU_setCOT(ASDU self, CauseOfTransmission value);

int
ASDU_getCA(ASDU self);

IEC60870_5_TypeID
ASDU_getTypeID(ASDU self);

bool
ASDU_isSequence(ASDU self);

int
ASDU_getNumberOfElements(ASDU self);

InformationObject
ASDU_getElement(ASDU self, int index);

ASDU
ASDU_create(ConnectionParameters parameters, TypeID typeId, bool isSequence, CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative);

void
ASDU_destroy(ASDU self);

/**
 * \brief add an information object to the ASDU
 *
 * \param self ASDU object instance
 * \param io information object to be added
 *
 * \return true when added, false when there not enought space left in the ASDU
 */
bool
ASDU_addInformationObject(ASDU self, InformationObject io);

/**
 * \brief create a new (read-only) instance
 *
 * NOTE: Do not try to append information objects to the instance!
 */
//TODO internal - remove from API
ASDU
ASDU_createFromBuffer(ConnectionParameters parameters, uint8_t* msg, int msgLength);

//TODO internal - remove from API
bool
ASDU_isStackCreated(ASDU self);

int
CP16Time2a_getEplapsedTimeInMs(CP16Time2a self);

void
CP16Time2a_setEplapsedTimeInMs(CP16Time2a self, int value);


int
CP24Time2a_getSecond(CP24Time2a self);

void
CP24Time2a_setSecond(CP24Time2a self, int value);

int
CP24Time2a_getMinute(CP24Time2a self);

void
CP24Time2a_setMinute(CP24Time2a self, int value);

bool
CP24Time2a_isInvalid(CP24Time2a self);

void
CP24Time2a_setInvalid(CP24Time2a self, bool value);

bool
CP24Time2a_isSubstituted(CP24Time2a self);

void
CP24Time2a_setSubstituted(CP24Time2a self, bool value);


CP56Time2a
CP56Time2a_createFromMsTimestamp(CP56Time2a self, uint64_t timestamp);

void
CP56Time2a_setFromMsTimestamp(CP56Time2a self, uint64_t timestamp);

uint64_t
CP56Time2a_toMsTimestamp(CP56Time2a self);

int
CP56Time2a_getMillisecond(CP56Time2a self);

void
CP56Time2a_setMillisecond(CP56Time2a self, int value);

int
CP56Time2a_getSecond(CP56Time2a self);

void
CP56Time2a_setSecond(CP56Time2a self, int value);

int
CP56Time2a_getMinute(CP56Time2a self);

void
CP56Time2a_setMinute(CP56Time2a self, int value);

int
CP56Time2a_getHour(CP56Time2a self);

void
CP56Time2a_setHour(CP56Time2a self, int value);

int
CP56Time2a_getDayOfWeek(CP56Time2a self);

void
CP56Time2a_setDayOfWeek(CP56Time2a self, int value);

int
CP56Time2a_getDayOfMonth(CP56Time2a self);

void
CP56Time2a_setDayOfMonth(CP56Time2a self, int value);

int
CP56Time2a_getMonth(CP56Time2a self);

void
CP56Time2a_setMonth(CP56Time2a self, int value);

int
CP56Time2a_getYear(CP56Time2a self);

void
CP56Time2a_setYear(CP56Time2a self, int value);

bool
CP56Time2a_isSummerTime(CP56Time2a self);

void
CP56Time2a_setSummerTime(CP56Time2a self, bool value);

bool
CP56Time2a_isInvalid(CP56Time2a self);

void
CP56Time2a_setInvalid(CP56Time2a self, bool value);

bool
CP56Time2a_isSubstituted(CP56Time2a self);

void
CP56Time2a_setSubstituted(CP56Time2a self, bool value);

BinaryCounterReading
BinaryCounterReading_create(BinaryCounterReading self, int32_t value, int seqNumber,
        bool hasCarry, bool isAdjusted, bool isInvalid);

void
BinaryCounterReading_destroy(BinaryCounterReading self);

int32_t
BinaryCounterReading_getValue(BinaryCounterReading self);

void
BinaryCounterReading_setValue(BinaryCounterReading self, int32_t value);

int
BinaryCounterReading_getSequenceNumber(BinaryCounterReading self);

bool
BinaryCounterReading_hasCarry(BinaryCounterReading self);

bool
BinaryCounterReading_isAdjusted(BinaryCounterReading self);

bool
BinaryCounterReading_isInvalid(BinaryCounterReading self);

void
BinaryCounterReading_setSequenceNumber(BinaryCounterReading self, int value);

void
BinaryCounterReading_setCarry(BinaryCounterReading self, bool value);

void
BinaryCounterReading_setAdjusted(BinaryCounterReading self, bool value);

void
BinaryCounterReading_setInvalid(BinaryCounterReading self, bool value);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_IEC60870_COMMON_H_ */
