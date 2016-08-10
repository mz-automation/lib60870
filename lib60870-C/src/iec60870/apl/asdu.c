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
#include "lib_memory.h"

struct sASDU {
    TypeID typeId;
    uint8_t vsq;
    CauseOfTransmission cot;
    uint8_t oa; /* originator address */
    bool isTest; /* is message a test message */
    bool isNegative; /* is message a negative confirmation */
    int ca; /* Common address */

    uint8_t* payload;
    int payloadSize;
    ConnectionParameters parameters;
};


ASDU
ASDU_createFromBuffer(ConnectionParameters parameters, uint8_t* msg, int msgLength)
{
    //TODO check if header fits in length

    ASDU self = (ASDU) GLOBAL_MALLOC(sizeof(struct sASDU));
    //TODO support allocation from static pool

    self->parameters = parameters;

    //int bufPos = 6; /* ignore header */

    int bufPos = 0;

    self->typeId = (TypeID) msg[bufPos++];
    self->vsq = msg[bufPos++];

    uint8_t cotByte = msg[bufPos++];

    if ((cotByte / 0x80) != 0)
        self->isTest = true;
    else
        self->isTest = false;

    if ((cotByte & 0x40) != 0)
        self->isNegative = true;
    else
        self->isNegative = false;

    self->cot = (CauseOfTransmission) (cotByte & 0x3f);

    if (parameters->sizeOfCOT == 2)
        self->oa = msg[bufPos++];
    else
        self->oa = 0;

    self->ca = msg[bufPos++];

    if (parameters->sizeOfCA > 1)
        self->ca += (msg[bufPos++] * 0x100);

    self->payloadSize = msgLength - bufPos;

    self->payload = msg + bufPos;

    return self;
}

CauseOfTransmission
ASDU_getCOT(ASDU self)
{
    return self->cot;
}

int
ASDU_getCA(ASDU self)
{
    return self->ca;
}

TypeID
ASDU_getTypeID(ASDU self)
{
    return self->typeId;
}

bool
ASDU_isSequence(ASDU self)
{
    if ((self->vsq & 0x80) != 0)
        return true;
    else
        return false;
}

int
ASDU_getNumberOfElements(ASDU self)
{
    return (self->vsq & 0x7f);
}

InformationObject
ASDU_getElement(ASDU self, int index)
{
    InformationObject retVal = NULL;

    int elementSize;

    switch (self->typeId) {

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
