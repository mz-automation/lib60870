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

#include <stdbool.h>
#include <stdint.h>

#include "iec60870_common.h"
#include "information_objects_internal.h"
#include "apl_types_internal.h"
#include "lib_memory.h"

struct sASDU {
    bool stackCreated;
    ConnectionParameters parameters;
    uint8_t* asdu;
    int asduHeaderLength;
    uint8_t* payload;
    int payloadSize;
};

typedef struct sStaticASDU* StaticASDU;

struct sStaticASDU {
    bool stackCreated;
    ConnectionParameters parameters;
    uint8_t* asdu;
    int asduHeaderLength;
    uint8_t* payload;
    int payloadSize;
    uint8_t encodedData[256];
};

typedef struct sASDUFrame* ASDUFrame;

struct sASDUFrame {
    FrameVFT virtualFunctionTable;
    ASDU asdu;
};

static void
asduFrame_destroy(Frame self)
{
}

static void
asduFrame_setNextByte(Frame self, uint8_t byte)
{
    ASDUFrame frame = (ASDUFrame) self;

    frame->asdu->payload[frame->asdu->payloadSize++] = byte;
}

static void
asduFrame_appendBytes(Frame self, uint8_t* bytes, int numberOfBytes)
{
    ASDUFrame frame = (ASDUFrame) self;

    uint8_t* target = frame->asdu->payload + frame->asdu->payloadSize;

    int i;
    for (i = 0; i < numberOfBytes; i++)
        target[i] = bytes[i];

    frame->asdu->payloadSize += numberOfBytes;
}

struct sFrameVFT asduFrameVFT = {
        asduFrame_destroy,
        NULL,
        asduFrame_setNextByte,
        asduFrame_appendBytes,
        NULL,
        NULL
};

ASDU
ASDU_create(ConnectionParameters parameters, TypeID typeId, CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative)
{
    StaticASDU self = (StaticASDU) GLOBAL_MALLOC(sizeof(struct sStaticASDU));
    //TODO support allocation from static pool

    if (self != NULL) {
        self->stackCreated = false;

        int asduHeaderLength = 2 + parameters->sizeOfCOT + parameters->sizeOfCA;

        self->encodedData[0] = (uint8_t) typeId;
        self->encodedData[1] = 0;
        self->encodedData[2] = (uint8_t) (cot & 0x3f);

        if (isTest)
            self->encodedData[2] |= 0x80;

        if (isNegative)
            self->encodedData[2] |= 0x40;

        int caIndex;

        if (parameters->sizeOfCOT > 1) {
            self->encodedData[3] = (uint8_t) oa;
            caIndex = 4;
        }
        else
            caIndex = 3;

        self->encodedData[caIndex] = ca % 0x100;

        if (parameters->sizeOfCA > 1)
            self->encodedData[caIndex + 1] = ca / 0x100;

        self->asdu = self->encodedData;
        self->asduHeaderLength = asduHeaderLength;
        self->payload = self->encodedData + asduHeaderLength;
        self->payloadSize = 0;
        self->parameters = parameters;
    }

    return (ASDU) self;
}

void
ASDU_destroy(ASDU self)
{
    GLOBAL_FREEMEM(self);
}

bool
ASDU_isStackCreated(ASDU self)
{
    return self->stackCreated;
}

void
ASDU_encode(ASDU self, Frame frame)
{
    Frame_appendBytes(frame, self->asdu, self->asduHeaderLength + self->payloadSize);
}

ASDU
ASDU_createFromBuffer(ConnectionParameters parameters, uint8_t* msg, int msgLength)
{
    int asduHeaderLength = 2 + parameters->sizeOfCOT + parameters->sizeOfCA;

    if (msgLength < asduHeaderLength)
        return NULL;

    ASDU self = (ASDU) GLOBAL_MALLOC(sizeof(struct sASDU));
    //TODO support allocation from static pool

    if (self != NULL) {

        self->stackCreated = true;
        self->parameters = parameters;

        self->asdu = msg;
        self->asduHeaderLength = asduHeaderLength;

        self->payload = msg + asduHeaderLength;
        self->payloadSize = msgLength - asduHeaderLength;
    }

    return self;
}

void
ASDU_addInformationObject(ASDU self, InformationObject io)
{
    self->asdu[1]++; // increase number of elements in VSQ

    struct sASDUFrame asduFrame = {
            &asduFrameVFT,
            self
    };

    InformationObject_encode(io, (Frame) &asduFrame, self->parameters);
}

bool
ASDU_isTest(ASDU self)
{
    if ((self->asdu[2] & 0x80) == 0x80)
        return true;
    else
        return false;
}

void
ASDU_setTest(ASDU self, bool value)
{
    if (value)
        self->asdu[2] |= 0x80;
    else
        self->asdu[2] &= ~(0x80);
}

bool
ASDU_isNegative(ASDU self)
{
    if ((self->asdu[2] & 0x40) == 0x40)
        return true;
    else
        return false;
}

void
ASDU_setNegative(ASDU self, bool value)
{
    if (value)
        self->asdu[2] |= 0x40;
    else
        self->asdu[2] &= ~(0x40);
}

int
ASDU_getOA(ASDU self)
{
    if (self->parameters->sizeOfCOT < 2)
        return -1;
    else
        return (int) self->asdu[4];
}

CauseOfTransmission
ASDU_getCOT(ASDU self)
{
    return (CauseOfTransmission) (self->asdu[2] & 0x3f);
}

void
ASDU_setCOT(ASDU self, CauseOfTransmission value)
{
    uint8_t cot = self->asdu[2] & 0xc0;
    cot += ((int) value) & 0x3f;

    self->asdu[2] = cot;
}

int
ASDU_getCA(ASDU self)
{
    int caIndex = 2 + self->parameters->sizeOfCOT;

    int ca = self->asdu[caIndex];

    if (self->parameters->sizeOfCA > 1)
        ca += (self->asdu[caIndex + 1] * 0x100);

    return ca;
}

TypeID
ASDU_getTypeID(ASDU self)
{
    return (TypeID) (self->asdu[0]);
}

bool
ASDU_isSequence(ASDU self)
{
    if ((self->asdu[1] & 0x80) != 0)
        return true;
    else
        return false;
}

int
ASDU_getNumberOfElements(ASDU self)
{
    return (self->asdu[1] & 0x7f);
}

InformationObject
ASDU_getElement(ASDU self, int index)
{
    InformationObject retVal = NULL;

    int elementSize;

    switch (ASDU_getTypeID(self)) {

    case M_SP_NA_1: /* 1 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) SinglePointInformation_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_SP_TA_1: /* 2 */

        elementSize = self->parameters->sizeOfIOA + 4;

        retVal = (InformationObject) SinglePointWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_DP_NA_1: /* 3 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) DoublePointInformation_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_DP_TA_1: /* 4 */

        elementSize = self->parameters->sizeOfIOA + 4;

        retVal = (InformationObject) DoublePointWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ST_NA_1: /* 5 */

        elementSize = self->parameters->sizeOfIOA + 2;

        retVal = (InformationObject) StepPositionInformation_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_ST_TA_1: /* 6 */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) StepPositionWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_BO_NA_1: /* 7 */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) BitString32_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_BO_TA_1: /* 8 */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) Bitstring32WithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_NA_1: /* 9 */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) MeasuredValueNormalized_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_ME_TA_1: /* 10 */

        elementSize = self->parameters->sizeOfIOA + 6;

        retVal = (InformationObject) MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_NB_1: /* 11 */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) MeasuredValueScaled_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        //TODO add support for Sequence of elements in a single information object (sq = 1)

        break;

    case M_ME_TB_1: /* 12 */

        elementSize = self->parameters->sizeOfIOA + 6;

        retVal = (InformationObject) MeasuredValueScaledWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;


    case M_ME_NC_1: /* 13 */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) MeasuredValueShort_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_TC_1: /* 14 */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) MeasuredValueShortWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_IT_NA_1: /* 15 */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) IntegratedTotals_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_IT_TA_1: /* 16 */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) IntegratedTotalsWithCP24Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TA_1: /* 17 */

        elementSize = self->parameters->sizeOfIOA + 6;

        retVal = (InformationObject) EventOfProtectionEquipment_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TB_1: /* 18 */

        elementSize = self->parameters->sizeOfIOA + 7;

        retVal = (InformationObject) PackedStartEventsOfProtectionEquipment_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TC_1: /* 19 */

        elementSize = self->parameters->sizeOfIOA + 7;

        retVal = (InformationObject) PackedOutputCircuitInfo_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_PS_NA_1: /* 20 */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) PackedSinglePointWithSCD_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_ND_1: /* 21 */

        elementSize = self->parameters->sizeOfIOA + 2;

        retVal = (InformationObject) MeasuredValueNormalizedWithoutQuality_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_SP_TB_1: /* 30 */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) SinglePointWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_DP_TB_1: /* 31 */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) DoublePointWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ST_TB_1: /* 32 */

        elementSize = self->parameters->sizeOfIOA + 9;

        retVal = (InformationObject) StepPositionWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_BO_TB_1: /* 33 */

        elementSize = self->parameters->sizeOfIOA + 12;

        retVal = (InformationObject) Bitstring32WithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_TD_1: /* 34 */

        elementSize = self->parameters->sizeOfIOA + 10;

        retVal = (InformationObject) MeasuredValueNormalizedWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_TE_1: /* 35 */

        elementSize = self->parameters->sizeOfIOA + 10;

        retVal = (InformationObject) MeasuredValueScaledWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_ME_TF_1: /* 36 */

        elementSize = self->parameters->sizeOfIOA + 12;

        retVal = (InformationObject) MeasuredValueShortWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_IT_TB_1: /* 37 */

        elementSize = self->parameters->sizeOfIOA + 12;

        retVal = (InformationObject) IntegratedTotalsWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TD_1: /* 38 */

        elementSize = self->parameters->sizeOfIOA + 10;

        retVal = (InformationObject) EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TE_1: /* 39 */

        elementSize = self->parameters->sizeOfIOA + 11;

        retVal = (InformationObject) PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EP_TF_1: /* 40 */

        elementSize = self->parameters->sizeOfIOA + 11;

        retVal = (InformationObject) PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    /* 41 - 44 reserved */

    case C_SC_NA_1: /* 45 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) SingleCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;


    case C_DC_NA_1: /* 46 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) DoubleCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_RC_NA_1: /* 47 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) StepCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_NA_1: /* 48 - Set-point command, normalized value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) SetpointCommandNormalized_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;


    case C_SE_NB_1: /* 49 - Set-point command, scaled value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) SetpointCommandScaled_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_NC_1: /* 50 - Set-point command, short floating point number */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) SetpointCommandShort_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_BO_NA_1: /* 51 - Bitstring command */

        elementSize = self->parameters->sizeOfIOA + 4;

        retVal = (InformationObject) Bitstring32Command_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    /* 52 - 57 reserved */

    case C_SC_TA_1: /* 58 - Single command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) SingleCommandWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_DC_TA_1: /* 59 - Double command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) DoubleCommandWithCP56Time2a_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_IC_NA_1: /* 100 - Interrogation command */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) InterrogationCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_RD_NA_1: /* 102 - Read command */

        elementSize = self->parameters->sizeOfIOA;

        retVal = (InformationObject) ReadCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        elementSize = self->parameters->sizeOfIOA;

        retVal = (InformationObject) ClockSynchronizationCommand_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_ME_NA_1: /* 110 - Parameter of measured values, normalized value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) ParameterNormalizedValue_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_ME_NB_1: /* 111 - Parameter of measured values, scaled value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) ParameterScaledValue_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_ME_NC_1: /* 112 - Parameter of measured values, short floating point number */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) ParameterFloatValue_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_AC_NA_1: /* 113 - Parameter for activation */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) ParameterActivation_getFromBuffer(NULL, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;
    }




    return retVal;
}

const char*
TypeID_toString(TypeID self)
{
    switch (self) {

    case M_SP_NA_1:
        return "M_SP_NA_1";

    case M_SP_TA_1:
        return "M_SP_TA_1";

    case M_DP_NA_1:
        return "M_DP_NA_1";

    case M_DP_TA_1:
        return "M_DP_TA_1";

    case M_ST_NA_1:
        return "M_ST_NA_1";

    case M_ST_TA_1:
        return "M_ST_TA_1";

    case M_BO_NA_1:
        return "M_BO_NA_1";

    case M_BO_TA_1:
        return "M_BO_TA_1";

    case M_ME_NA_1:
        return "M_ME_NA_1";

    case M_ME_TA_1:
        return "M_ME_TA_1";

    case M_ME_NB_1:
        return "M_ME_NB_1";

    case M_ME_TB_1:
        return "M_ME_TB_1";

    case M_ME_NC_1:
        return "M_ME_NC_1";

    case M_ME_TC_1:
        return "M_ME_TC_1";

    case M_IT_NA_1:
        return "M_IT_NA_1";

    case M_IT_TA_1:
        return "M_IT_TA_1";

    case M_EP_TA_1:
        return "M_EP_TA_1";

    case M_EP_TB_1:
        return "M_EP_TB_1";

    case M_EP_TC_1:
        return "M_EP_TC_1";

    case M_PS_NA_1:
        return "M_PS_NA_1";

    case M_ME_ND_1:
        return "M_ME_ND_1";

    case M_SP_TB_1:
        return "M_SP_TB_1";

    case M_DP_TB_1:
        return "M_DP_TB_1";

    case M_ST_TB_1:
        return "M_ST_TB_1";

    case M_BO_TB_1:
        return "M_BO_TB_1";

    case M_ME_TD_1:
        return "M_ME_TD_1";

    case M_ME_TE_1:
        return "M_ME_TE_1";

    case M_ME_TF_1:
        return "M_ME_TF_1";

    case M_IT_TB_1:
        return "M_IT_TB_1";

    case M_EP_TD_1:
        return "M_EP_TD_1";

    case M_EP_TE_1:
        return "M_EP_TE_1";

    case M_EP_TF_1:
        return "M_EP_TF_1";

    case C_SC_NA_1:
        return "C_SC_NA_1";

    case C_DC_NA_1:
        return "C_DC_NA_1";

    case C_RC_NA_1:
        return "C_RC_NA_1";

    case C_SE_NA_1:
        return "C_SE_NA_1";

    case C_SE_NB_1:
        return "C_SE_NB_1";

    case C_SE_NC_1:
        return "C_SE_NC_1";

    case C_BO_NA_1:
        return "C_BO_NA_1";

    case C_SC_TA_1:
        return "C_SC_TA_1";

    case C_DC_TA_1:
        return "C_DC_TA_1";

    case C_RC_TA_1:
        return "C_RC_TA_1";

    case C_SE_TA_1:
        return "C_SE_TA_1";

    case C_SE_TB_1:
        return "C_SE_TB_1";

    case C_SE_TC_1:
        return "C_SE_TC_1";

    case C_BO_TA_1:
        return "C_BO_TA_1";

    case M_EI_NA_1:
        return "M_EI_NA_1";

    case C_IC_NA_1:
        return "C_IC_NA_1";

    case C_CI_NA_1:
        return "C_CI_NA_1";

    case C_RD_NA_1:
        return "C_RD_NA_1";

    case C_CS_NA_1:
        return "C_CS_NA_1";

    case C_TS_NA_1:
        return "C_TS_NA_1";

    case C_RP_NA_1:
        return "C_RP_NA_1";

    case C_CD_NA_1:
        return "C_CD_NA_1";

    case C_TS_TA_1:
        return "C_TS_TA_1";

    case P_ME_NA_1:
        return "P_ME_NA_1";

    case P_ME_NB_1:
        return "P_ME_NB_1";

    case P_ME_NC_1:
        return "P_ME_NC_1";

    case P_AC_NA_1:
        return "P_AC_NA_1";

    case F_FR_NA_1:
        return "F_FR_NA_1";

    case F_SR_NA_1:
        return "F_SR_NA_1";

    case F_SC_NA_1:
        return "F_SC_NA_1";

    case F_LS_NA_1:
        return "F_LS_NA_1";

    case F_AF_NA_1:
        return "F_AF_NA_1";

    case F_SG_NA_1:
        return "F_SG_NA_1";

    case F_DR_TA_1:
        return "F_DR_TA_1";

    case F_SC_NB_1:
        return "F_SC_NB_1";

    default:
        return "unknown";
    }
}
