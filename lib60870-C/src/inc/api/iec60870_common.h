/*
 *  Copyright 2016-2022 Michael Zillgith
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

/**
 * \file iec60870_common.h
 * \brief Common definitions for IEC 60870-5-101/104
 * These types are used by CS101/CS104 master and slaves
 */

/**
 * @addtogroup COMMON Common API functions
 *
 * @{
 */


#define IEC_60870_5_104_DEFAULT_PORT 2404
#define IEC_60870_5_104_DEFAULT_TLS_PORT 19998

#define LIB60870_VERSION_MAJOR 2
#define LIB60870_VERSION_MINOR 3
#define LIB60870_VERSION_PATCH 6

/**
 * \brief lib60870 version information
 */
typedef struct {
    int major;
    int minor;
    int patch;
} Lib60870VersionInfo;

/**
 * \brief link layer mode for serial link layers
 */
typedef enum {
    IEC60870_LINK_LAYER_BALANCED = 0,
    IEC60870_LINK_LAYER_UNBALANCED = 1
} IEC60870_LinkLayerMode;

/** \brief State of the link layer */
typedef enum {
    /** The link layer is idle, there is no communication */
    LL_STATE_IDLE,

    /** An error has occurred at the link layer, the link may not be usable */
    LL_STATE_ERROR,

    /** The link layer is busy and therefore no usable */
    LL_STATE_BUSY,

    /** The link is available for user data transmission and reception */
    LL_STATE_AVAILABLE
} LinkLayerState;

/**
 * \brief Callback handler for link layer state changes
 *
 * \param parameter user provided parameter that is passed to the handler
 * \param address slave address used by the link layer state machine (only relevant for unbalanced master)
 * \param newState the new link layer state
 */
typedef void (*IEC60870_LinkLayerStateChangedHandler) (void* parameter, int address, LinkLayerState newState);

/**
 * \brief Callback handler for sent and received messages
 *
 * This callback handler provides access to the raw message buffer of received or sent
 * messages. It can be used for debugging purposes. Usually it is not used nor required
 * for applications.
 *
 * \param parameter user provided parameter
 * \param msg the message buffer
 * \param msgSize size of the message
 * \param sent indicates if the message was sent or received
 */
typedef void (*IEC60870_RawMessageHandler) (void* parameter, uint8_t* msg, int msgSize, bool sent);

/**
 * \brief Parameters for the CS101/CS104 application layer
 */
typedef struct sCS101_AppLayerParameters* CS101_AppLayerParameters;

struct sCS101_AppLayerParameters {
    int sizeOfTypeId;      /* size of the type id (default = 1 - don't change) */
    int sizeOfVSQ;         /* don't change */
    int sizeOfCOT;         /* size of COT (1/2 - default = 2 -> COT includes OA) */
    int originatorAddress; /* originator address (OA) to use (0-255) */
    int sizeOfCA;          /* size of common address (CA) of ASDU (1/2 - default = 2) */
    int sizeOfIOA;         /* size of information object address (IOA) (1/2/3 - default = 3) */
    int maxSizeOfASDU;     /* maximum size of the ASDU that is generated - the maximum maximum value is 249 for IEC 104 and 254 for IEC 101 */
};

/**
 * \brief Message type IDs
 */
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
    S_IT_TC_1 = 41,
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
    S_CH_NA_1 = 81,
    S_RP_NA_1 = 82,
    S_AR_NA_1 = 83,
    S_KR_NA_1 = 84,
    S_KS_NA_1 = 85,
    S_KC_NA_1 = 86,
    S_ER_NA_1 = 87,
    S_US_NA_1 = 90,
    S_UQ_NA_1 = 91,
    S_UR_NA_1 = 92,
    S_UK_NA_1 = 93,
    S_UA_NA_1 = 94,
    S_UC_NA_1 = 95,
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

typedef struct sInformationObject* InformationObject;

/**
 * \brief Application Service Data Unit (ASDU) for the CS101/CS104 application layer
 */
typedef struct sCS101_ASDU* CS101_ASDU;

typedef struct {
    CS101_AppLayerParameters parameters;
    uint8_t* asdu;
    int asduHeaderLength;
    uint8_t* payload;
    int payloadSize;
    uint8_t encodedData[256];
} sCS101_StaticASDU;

typedef sCS101_StaticASDU* CS101_StaticASDU;

typedef struct sCP16Time2a* CP16Time2a;

struct sCP16Time2a {
    uint8_t encodedValue[2];
};

typedef struct sCP24Time2a* CP24Time2a;

struct sCP24Time2a {
    uint8_t encodedValue[3];
};

typedef struct sCP32Time2a* CP32Time2a;

/**
 * \brief 4 byte binary time
 */
struct sCP32Time2a {
    uint8_t encodedValue[4];
};

/**
 * \brief 7 byte binary time
 */
typedef struct sCP56Time2a* CP56Time2a;

struct sCP56Time2a {
    uint8_t encodedValue[7];
};

/**
 * \brief Base type for counter readings
 */
typedef struct sBinaryCounterReading* BinaryCounterReading;

struct sBinaryCounterReading {
    uint8_t encodedValue[5];
};

/**
 * \brief Parameters for CS104 connections - APCI (application protocol control information)
 */
typedef struct sCS104_APCIParameters* CS104_APCIParameters;

struct sCS104_APCIParameters {
    int k;
    int w;
    int t0;
    int t1;
    int t2;
    int t3;
};

#include "cs101_information_objects.h"

typedef enum {
    CS101_COT_PERIODIC = 1,
    CS101_COT_BACKGROUND_SCAN = 2,
    CS101_COT_SPONTANEOUS = 3,
    CS101_COT_INITIALIZED = 4,
    CS101_COT_REQUEST = 5,
    CS101_COT_ACTIVATION = 6,
    CS101_COT_ACTIVATION_CON = 7,
    CS101_COT_DEACTIVATION = 8,
    CS101_COT_DEACTIVATION_CON = 9,
    CS101_COT_ACTIVATION_TERMINATION = 10,
    CS101_COT_RETURN_INFO_REMOTE = 11,
    CS101_COT_RETURN_INFO_LOCAL = 12,
    CS101_COT_FILE_TRANSFER = 13,
    CS101_COT_AUTHENTICATION = 14,
    CS101_COT_MAINTENANCE_OF_AUTH_SESSION_KEY = 15,
    CS101_COT_MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY = 16,
    CS101_COT_INTERROGATED_BY_STATION = 20,
    CS101_COT_INTERROGATED_BY_GROUP_1 = 21,
    CS101_COT_INTERROGATED_BY_GROUP_2 = 22,
    CS101_COT_INTERROGATED_BY_GROUP_3 = 23,
    CS101_COT_INTERROGATED_BY_GROUP_4 = 24,
    CS101_COT_INTERROGATED_BY_GROUP_5 = 25,
    CS101_COT_INTERROGATED_BY_GROUP_6 = 26,
    CS101_COT_INTERROGATED_BY_GROUP_7 = 27,
    CS101_COT_INTERROGATED_BY_GROUP_8 = 28,
    CS101_COT_INTERROGATED_BY_GROUP_9 = 29,
    CS101_COT_INTERROGATED_BY_GROUP_10 = 30,
    CS101_COT_INTERROGATED_BY_GROUP_11 = 31,
    CS101_COT_INTERROGATED_BY_GROUP_12 = 32,
    CS101_COT_INTERROGATED_BY_GROUP_13 = 33,
    CS101_COT_INTERROGATED_BY_GROUP_14 = 34,
    CS101_COT_INTERROGATED_BY_GROUP_15 = 35,
    CS101_COT_INTERROGATED_BY_GROUP_16 = 36,
    CS101_COT_REQUESTED_BY_GENERAL_COUNTER = 37,
    CS101_COT_REQUESTED_BY_GROUP_1_COUNTER = 38,
    CS101_COT_REQUESTED_BY_GROUP_2_COUNTER = 39,
    CS101_COT_REQUESTED_BY_GROUP_3_COUNTER = 40,
    CS101_COT_REQUESTED_BY_GROUP_4_COUNTER = 41,
    CS101_COT_UNKNOWN_TYPE_ID = 44,
    CS101_COT_UNKNOWN_COT = 45,
    CS101_COT_UNKNOWN_CA = 46,
    CS101_COT_UNKNOWN_IOA = 47
} CS101_CauseOfTransmission;

const char*
CS101_CauseOfTransmission_toString(CS101_CauseOfTransmission self);

void
Lib60870_enableDebugOutput(bool value);

Lib60870VersionInfo
Lib60870_getLibraryVersionInfo(void);

/**
 * \brief Check if the test flag of the ASDU is set
 */
bool
CS101_ASDU_isTest(CS101_ASDU self);

/**
 * \brief Set the test flag of the ASDU
 */
void
CS101_ASDU_setTest(CS101_ASDU self, bool value);

/**
 * \brief Check if the negative flag of the ASDU is set
 */
bool
CS101_ASDU_isNegative(CS101_ASDU self);

/**
 * \brief Set the negative flag of the ASDU
 */
void
CS101_ASDU_setNegative(CS101_ASDU self, bool value);

/**
 * \brief get the OA (originator address) of the ASDU.
 */
int
CS101_ASDU_getOA(CS101_ASDU self);

/**
 * \brief Get the cause of transmission (COT) of the ASDU
 */
CS101_CauseOfTransmission
CS101_ASDU_getCOT(CS101_ASDU self);

/**
 * \brief Set the cause of transmission (COT) of the ASDU
 */
void
CS101_ASDU_setCOT(CS101_ASDU self, CS101_CauseOfTransmission value);

/**
 * \brief Get the common address (CA) of the ASDU
 */
int
CS101_ASDU_getCA(CS101_ASDU self);

/**
 * \brief Set the common address (CA) of the ASDU
 *
 * \param ca the ca in unstructured form
 */
void
CS101_ASDU_setCA(CS101_ASDU self, int ca);


/**
 * \brief Get the type ID of the ASDU
 *
 * \return the type ID to identify the ASDU type
 */
IEC60870_5_TypeID
CS101_ASDU_getTypeID(CS101_ASDU self);

/**
 * \brief Set the type ID of the ASDU
 *
 * NOTE: Usually it is not required to call this function as the type is determined when the
 * ASDU is parsed or when a new information object is added with \ref CS101_ASDU_addInformationObject
 * The function is intended to be used by library extensions of for creating private ASDU types.
 *
 * \param typeId the type ID to identify the ASDU type
 */
void
CS101_ASDU_setTypeID(CS101_ASDU self, IEC60870_5_TypeID typeId);

/**
 * \brief Check if the ASDU contains a sequence of consecutive information objects
 *
 * NOTE: in a sequence of consecutive information objects only the first information object address
 * is encoded. The following information objects ahve consecutive information object addresses.
 *
 * \return true when the ASDU represents a sequence of consecutive information objects, false otherwise
 */
bool
CS101_ASDU_isSequence(CS101_ASDU self);

/**
 * \brief Set the ASDU to represent a sequence of consecutive information objects
 *
 * NOTE: It is not required to use this function when constructing ASDUs as this information is
 * already provided when calling one of the constructors \ref CS101_ASDU_create or \ref CS101_ASDU_initializeStatic
 *
 * \param isSequence specify if the ASDU represents a sequence of consecutive information objects
 */
void
CS101_ASDU_setSequence(CS101_ASDU self, bool isSequence);

/**
 * \brief Get the number of information objects (elements) in the ASDU
 *
 * \return the number of information objects/element (valid range 0 - 127)
 */
int
CS101_ASDU_getNumberOfElements(CS101_ASDU self);

/**
 * \brief Set the number of information objects (elements) in the ASDU
 *
 * NOTE: Usually it is not required to call this function as the number of elements is set when the
 * ASDU is parsed or when a new information object is added with \ref CS101_ASDU_addInformationObject
 * The function is intended to be used by library extensions of for creating private ASDU types.
 *
 * \param numberOfElements the number of information objects/element (valid range 0 - 127)
 */
void
CS101_ASDU_setNumberOfElements(CS101_ASDU self, int numberOfElements);

/**
 * \brief Get the information object with the given index
 *
 * \param index the index of the information object (starting with 0)
 *
 * \return the information object, or NULL if there is no information object with the given index
 */
InformationObject
CS101_ASDU_getElement(CS101_ASDU self, int index);

/**
 * \brief Get the information object with the given index and store it in the provided information object instance
 *
 * \param io if not NULL use the provided information object instance to store the information, has to be of correct type.
 * \param index the index of the information object (starting with 0)
 *
 * \return the information object, or NULL if there is no information object with the given index
 */
InformationObject
CS101_ASDU_getElementEx(CS101_ASDU self, InformationObject io, int index);

/**
 * \brief Create a new ASDU. The type ID will be derived from the first InformationObject that will be added
 *
 * \param parameters the application layer parameters used to encode the ASDU
 * \param isSequence if the information objects will be encoded as a compact sequence of information objects with subsequent IOA values
 * \param cot cause of transmission (COT)
 * \param oa originator address (OA) to be used
 * \param ca the common address (CA) of the ASDU
 * \param isTest if the test flag will be set or not
 * \param isNegative if the negative falg will be set or not
 *
 * \return the new CS101_ASDU instance
 */
CS101_ASDU
CS101_ASDU_create(CS101_AppLayerParameters parameters, bool isSequence, CS101_CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative);

/**
 * \brief Create a new ASDU instance from a buffer containing the raw ASDU message bytes
 *
 * NOTE: Do not try to append information objects to the instance!
 *
 * \param parameters the application layer parameters used to encode the ASDU
 * \param msg the buffer containing the raw ASDU message bytes
 * \param msgLength the length of the message
 *
 * \return the new CS101_ASDU instance
 */
CS101_ASDU
CS101_ASDU_createFromBuffer(CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength);

/**
 * \brief Create a new ASDU and store it in the provided static ASDU structure.
 *
 * NOTE: The type ID will be derived from the first InformationObject that will be added.
 *
 * \param self pointer to the statically allocated data structure
 * \param parameters the application layer parameters used to encode the ASDU
 * \param isSequence if the information objects will be encoded as a compact sequence of information objects with subsequent IOA values
 * \param cot cause of transmission (COT)
 * \param oa originator address (OA) to be used
 * \param ca the common address (CA) of the ASDU
 * \param isTest if the test flag will be set or not
 * \param isNegative if the negative falg will be set or not
 *
 * \return the new CS101_ASDU instance
 */
CS101_ASDU
CS101_ASDU_initializeStatic(CS101_StaticASDU self, CS101_AppLayerParameters parameters, bool isSequence, CS101_CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative);

/**
 * \brief Create a new ASDU that is an exact copy of the ASDU
 * 
 * \param self ASDU instance to be copied
 * \param clone static ASDU instance where to store the cloned ASDU or NULL. When this parameter is NULL the function will allocate the memory for the clone
 * 
 * \return the cloned ASDU instance
 */
CS101_ASDU
CS101_ASDU_clone(CS101_ASDU self, CS101_StaticASDU clone);

/**
 * Get the ASDU payload
 *
 * The payload is the ASDU message part after the ASDU header (type ID, VSQ, COT, CASDU)
 *
 * \return the ASDU payload buffer
 */
uint8_t*
CS101_ASDU_getPayload(CS101_ASDU self);

/**
 * \brief Append the provided data to the ASDU payload
 *
 * NOTE: Usually it is not required to call this function as the payload is set when the
 * ASDU is parsed or when a new information object is added with \ref CS101_ASDU_addInformationObject
 * The function is intended to be only used by library extensions of for creating private ASDU types.
 *
 * \param buffer pointer to the payload data to add
 * \param size number of bytes to append to the payload
 *
 * \return true when data has been added, false otherwise
 */
bool
CS101_ASDU_addPayload(CS101_ASDU self, uint8_t* buffer, int size);

/**
 * Get the ASDU payload buffer size
 *
 * The payload is the ASDU message part after the ASDU header (type ID, VSQ, COT, CASDU)
 *
 * \return the ASDU payload buffer size
 */
int
CS101_ASDU_getPayloadSize(CS101_ASDU self);

/**
 * \brief Destroy the ASDU object (release all resources)
 */
void
CS101_ASDU_destroy(CS101_ASDU self);

/**
 * \brief add an information object to the ASDU
 *
 * NOTE: Only information objects of the exact same type can be added to a single ASDU!
 *
 * \param self ASDU object instance
 * \param io information object to be added
 *
 * \return true when added, false when there not enough space left in the ASDU or IO cannot be added to the sequence because of wrong IOA, or wrong type.
 */
bool
CS101_ASDU_addInformationObject(CS101_ASDU self, InformationObject io);

/**
 * \brief remove all information elements from the ASDU object
 *
 * \param self ASDU object instance
 */
void
CS101_ASDU_removeAllElements(CS101_ASDU self);

/**
 * \brief Get the elapsed time in ms
 */
int
CP16Time2a_getEplapsedTimeInMs(const CP16Time2a self);

/**
 * \brief set the elapsed time in ms
 */
void
CP16Time2a_setEplapsedTimeInMs(CP16Time2a self, int value);

/**
 * \brief Get the millisecond part of the time value
 */
int
CP24Time2a_getMillisecond(const CP24Time2a self);

/**
 * \brief Set the millisecond part of the time value
 */
void
CP24Time2a_setMillisecond(CP24Time2a self, int value);

/**
 * \brief Get the second part of the time value
 */
int
CP24Time2a_getSecond(const CP24Time2a self);

/**
 * \brief Set the second part of the time value
 */
void
CP24Time2a_setSecond(CP24Time2a self, int value);

/**
 * \brief Get the minute part of the time value
 */
int
CP24Time2a_getMinute(const CP24Time2a self);

/**
 * \brief Set the minute part of the time value
 */
void
CP24Time2a_setMinute(CP24Time2a self, int value);

/**
 * \brief Check if the invalid flag of the time value is set
 */
bool
CP24Time2a_isInvalid(const CP24Time2a self);

/**
 * \brief Set the invalid flag of the time value
 */
void
CP24Time2a_setInvalid(CP24Time2a self, bool value);

/**
 * \brief Check if the substituted flag of the time value is set
 */
bool
CP24Time2a_isSubstituted(const CP24Time2a self);

/**
 * \brief Set the substituted flag of the time value
 */
void
CP24Time2a_setSubstituted(CP24Time2a self, bool value);

/**
 * \brief Create a 7 byte time from a UTC ms timestamp
 */
CP56Time2a
CP56Time2a_createFromMsTimestamp(CP56Time2a self, uint64_t timestamp);


CP32Time2a
CP32Time2a_create(CP32Time2a self);

void
CP32Time2a_setFromMsTimestamp(CP32Time2a self, uint64_t timestamp);

int
CP32Time2a_getMillisecond(const CP32Time2a self);

void
CP32Time2a_setMillisecond(CP32Time2a self, int value);

int
CP32Time2a_getSecond(const CP32Time2a self);

void
CP32Time2a_setSecond(CP32Time2a self, int value);

int
CP32Time2a_getMinute(const CP32Time2a self);


void
CP32Time2a_setMinute(CP32Time2a self, int value);

bool
CP32Time2a_isInvalid(const CP32Time2a self);

void
CP32Time2a_setInvalid(CP32Time2a self, bool value);

bool
CP32Time2a_isSubstituted(const CP32Time2a self);

void
CP32Time2a_setSubstituted(CP32Time2a self, bool value);

int
CP32Time2a_getHour(const CP32Time2a self);

void
CP32Time2a_setHour(CP32Time2a self, int value);

bool
CP32Time2a_isSummerTime(const CP32Time2a self);

void
CP32Time2a_setSummerTime(CP32Time2a self, bool value);

/**
 * \brief Set the time value of a 7 byte time from a UTC ms timestamp
 */
void
CP56Time2a_setFromMsTimestamp(CP56Time2a self, uint64_t timestamp);

/**
 * \brief Convert a 7 byte time to a ms timestamp
 */
uint64_t
CP56Time2a_toMsTimestamp(const CP56Time2a self);

/**
 * \brief Get the ms part of a time value
 */
int
CP56Time2a_getMillisecond(const CP56Time2a self);

/**
 * \brief Set the ms part of a time value
 */
void
CP56Time2a_setMillisecond(CP56Time2a self, int value);

int
CP56Time2a_getSecond(const CP56Time2a self);

void
CP56Time2a_setSecond(CP56Time2a self, int value);

int
CP56Time2a_getMinute(const CP56Time2a self);

void
CP56Time2a_setMinute(CP56Time2a self, int value);

int
CP56Time2a_getHour(const CP56Time2a self);

void
CP56Time2a_setHour(CP56Time2a self, int value);

int
CP56Time2a_getDayOfWeek(const CP56Time2a self);

void
CP56Time2a_setDayOfWeek(CP56Time2a self, int value);

int
CP56Time2a_getDayOfMonth(const CP56Time2a self);

void
CP56Time2a_setDayOfMonth(CP56Time2a self, int value);

/**
 * \brief Get the month field of the time
 *
 * \return value the month (1..12)
 */
int
CP56Time2a_getMonth(const CP56Time2a self);

/**
 * \brief Set the month field of the time
 *
 * \param value the month (1..12)
 */
void
CP56Time2a_setMonth(CP56Time2a self, int value);

/**
 * \brief Get the year (range 0..99)
 *
 * \param value the year (0..99)
 */
int
CP56Time2a_getYear(const CP56Time2a self);

/**
 * \brief Set the year
 *
 * \param value the year
 */
void
CP56Time2a_setYear(CP56Time2a self, int value);

bool
CP56Time2a_isSummerTime(const CP56Time2a self);

void
CP56Time2a_setSummerTime(CP56Time2a self, bool value);

bool
CP56Time2a_isInvalid(const CP56Time2a self);

void
CP56Time2a_setInvalid(CP56Time2a self, bool value);

bool
CP56Time2a_isSubstituted(const CP56Time2a self);

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

/**
 * @}
 */

typedef struct sFrame* Frame;

void
Frame_destroy(Frame self);

void
Frame_resetFrame(Frame self);

void
Frame_setNextByte(Frame self, uint8_t byte);

void
Frame_appendBytes(Frame self, uint8_t* bytes, int numberOfBytes);

int
Frame_getMsgSize(Frame self);

uint8_t*
Frame_getBuffer(Frame self);

int
Frame_getSpaceLeft(Frame self);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_IEC60870_COMMON_H_ */
