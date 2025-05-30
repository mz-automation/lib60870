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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "iec60870_common.h"
#include "information_objects_internal.h"
#include "apl_types_internal.h"
#include "lib_memory.h"
#include "lib60870_internal.h"
#include "cs101_asdu_internal.h"

typedef struct sASDUFrame* ASDUFrame;

struct sASDUFrame {
    FrameVFT virtualFunctionTable;
    CS101_ASDU asdu;
};

static void
asduFrame_destroy(Frame self)
{
    UNUSED_PARAMETER(self);
}

static void
asduFrame_setNextByte(Frame self, uint8_t byte)
{
    ASDUFrame frame = (ASDUFrame) self;

    frame->asdu->payload[frame->asdu->payloadSize++] = byte;
}

static void
asduFrame_appendBytes(Frame self, const uint8_t* bytes, int numberOfBytes)
{
    ASDUFrame frame = (ASDUFrame) self;

    uint8_t* target = frame->asdu->payload + frame->asdu->payloadSize;

    int i;
    for (i = 0; i < numberOfBytes; i++)
        target[i] = bytes[i];

    frame->asdu->payloadSize += numberOfBytes;
}

static int
asduFrame_getSpaceLeft(Frame self)
{
    ASDUFrame frame = (ASDUFrame) self;

    return (frame->asdu->parameters->maxSizeOfASDU - frame->asdu->payloadSize - frame->asdu->asduHeaderLength);
}

struct sFrameVFT asduFrameVFT = {
        asduFrame_destroy,
        NULL,
        asduFrame_setNextByte,
        asduFrame_appendBytes,
        NULL,
        NULL,
        asduFrame_getSpaceLeft
};

CS101_ASDU
CS101_ASDU_create(CS101_AppLayerParameters parameters, bool isSequence, CS101_CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative)
{
    CS101_StaticASDU self = (CS101_StaticASDU) GLOBAL_MALLOC(sizeof(sCS101_StaticASDU));

    if (self != NULL)
        CS101_ASDU_initializeStatic(self, parameters, isSequence, cot, oa, ca, isTest, isNegative);

    return (CS101_ASDU) self;
}

CS101_ASDU
CS101_ASDU_clone(CS101_ASDU self, CS101_StaticASDU clone)
{
    if (clone == NULL) {
        clone = (CS101_StaticASDU) GLOBAL_MALLOC(sizeof(sCS101_StaticASDU));
    }

    if (clone) {
        CS101_ASDU_initializeStatic(clone, self->parameters, CS101_ASDU_isSequence(self), 
            CS101_ASDU_getCOT(self), CS101_ASDU_getOA(self), CS101_ASDU_getCA(self), CS101_ASDU_isTest(self), CS101_ASDU_isNegative(self));

        uint8_t* payload = CS101_ASDU_getPayload(self);
        int payloadSize = CS101_ASDU_getPayloadSize(self);

        CS101_ASDU_setTypeID((CS101_ASDU)clone, CS101_ASDU_getTypeID(self));
        CS101_ASDU_setNumberOfElements((CS101_ASDU)clone, CS101_ASDU_getNumberOfElements(self));

        CS101_ASDU_addPayload((CS101_ASDU)clone, payload, payloadSize);
    }

    return (CS101_ASDU) clone;
}

CS101_ASDU
CS101_ASDU_initializeStatic(CS101_StaticASDU self, CS101_AppLayerParameters parameters, bool isSequence, CS101_CauseOfTransmission cot, int oa, int ca,
        bool isTest, bool isNegative)
{
    int asduHeaderLength = 2 + parameters->sizeOfCOT + parameters->sizeOfCA;

    self->encodedData[0] = (uint8_t) 0;

    if (isSequence)
        self->encodedData[1] = 0x80;
    else
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

    return (CS101_ASDU) self;
}

void
CS101_ASDU_destroy(CS101_ASDU self)
{
    GLOBAL_FREEMEM(self);
}

void
CS101_ASDU_encode(CS101_ASDU self, Frame frame)
{
    Frame_appendBytes(frame, self->asdu, self->asduHeaderLength + self->payloadSize);
}

CS101_ASDU
CS101_ASDU_createFromBufferEx(CS101_ASDU asdu, CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength)
{
    int asduHeaderLength = 2 + parameters->sizeOfCOT + parameters->sizeOfCA;

    if (msgLength < asduHeaderLength)
        return NULL;

    CS101_ASDU self = (CS101_ASDU)asdu;

    if (self == NULL)
        self = (CS101_ASDU) GLOBAL_MALLOC(sizeof(struct sCS101_ASDU));

    if (self)
    {
        self->parameters = parameters;

        self->asdu = msg;
        self->asduHeaderLength = asduHeaderLength;

        self->payload = msg + asduHeaderLength;
        self->payloadSize = msgLength - asduHeaderLength;
    }

    return self;
}

CS101_ASDU
CS101_ASDU_createFromBuffer(CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength)
{
    return CS101_ASDU_createFromBufferEx(NULL, parameters, msg, msgLength);
}

uint8_t*
CS101_ASDU_getPayload(CS101_ASDU self)
{
    return self->payload;
}

int
CS101_ASDU_getPayloadSize(CS101_ASDU self)
{
    return self->payloadSize;
}

bool
CS101_ASDU_addPayload(CS101_ASDU self, uint8_t* buffer, int size)
{
    if (buffer == NULL)
        return false;

    if (self->payloadSize + self->asduHeaderLength + size <= 256) {
        memcpy(self->payload + self->payloadSize, buffer, size);
        self->payloadSize += size;
        return true;
    }
    else
        return false;
}

static int
getFirstIOA(CS101_ASDU self)
{
    int startIndex = self->asduHeaderLength;

    int ioa = self->asdu[startIndex];

    if (self->parameters->sizeOfIOA > 1)
        ioa += (self->asdu [startIndex + 1] * 0x100);

    if (self->parameters->sizeOfIOA > 2)
        ioa += (self->asdu [startIndex + 2] * 0x10000);

    return ioa;
}

bool
CS101_ASDU_addInformationObject(CS101_ASDU self, InformationObject io)
{
    struct sASDUFrame asduFrame = {
            &asduFrameVFT,
            self
    };

    bool encoded = false;

    int numberOfElements = CS101_ASDU_getNumberOfElements(self);

    if (numberOfElements == 0)
    {
        self->asdu[0] = (uint8_t) InformationObject_getType(io);

        encoded = InformationObject_encode(io, (Frame) &asduFrame, self->parameters, false);
    }
    else if (numberOfElements < 0x7f)
    {
        /* Check if type of information object is matching ASDU type */

        if (self->asdu[0] == (uint8_t) InformationObject_getType(io))
        {
            if (CS101_ASDU_isSequence(self))
            {
                /* check that new information object has correct IOA */
                if (InformationObject_getObjectAddress(io) == (getFirstIOA(self) + CS101_ASDU_getNumberOfElements(self)))
                    encoded = InformationObject_encode(io, (Frame) &asduFrame, self->parameters, true);
                else
                    encoded = false;
            }
            else
            {
                encoded = InformationObject_encode(io, (Frame) &asduFrame, self->parameters, false);;
            }
        }
    }

    if (encoded)
        self->asdu[1]++; /* increase number of elements in VSQ */

    return encoded;
}

void
CS101_ASDU_removeAllElements(CS101_ASDU self)
{
    self->asdu[1] = (self->asdu[1] & 0x80);
    self->payloadSize = 0;
}

bool
CS101_ASDU_isTest(CS101_ASDU self)
{
    if ((self->asdu[2] & 0x80) == 0x80)
        return true;
    else
        return false;
}

void
CS101_ASDU_setTest(CS101_ASDU self, bool value)
{
    if (value)
        self->asdu[2] |= 0x80;
    else
        self->asdu[2] &= ~(0x80);
}

bool
CS101_ASDU_isNegative(CS101_ASDU self)
{
    if ((self->asdu[2] & 0x40) == 0x40)
        return true;
    else
        return false;
}

void
CS101_ASDU_setNegative(CS101_ASDU self, bool value)
{
    if (value)
        self->asdu[2] |= 0x40;
    else
        self->asdu[2] &= ~(0x40);
}

int
CS101_ASDU_getOA(CS101_ASDU self)
{
    if (self->parameters->sizeOfCOT < 2)
        return -1;
    else
        return (int) self->asdu[3];
}

CS101_CauseOfTransmission
CS101_ASDU_getCOT(CS101_ASDU self)
{
    return (CS101_CauseOfTransmission) (self->asdu[2] & 0x3f);
}

void
CS101_ASDU_setCOT(CS101_ASDU self, CS101_CauseOfTransmission value)
{
    uint8_t cot = self->asdu[2] & 0xc0;
    cot += ((int) value) & 0x3f;

    self->asdu[2] = cot;
}

int
CS101_ASDU_getCA(CS101_ASDU self)
{
    int caIndex = 2 + self->parameters->sizeOfCOT;

    int ca = self->asdu[caIndex];

    if (self->parameters->sizeOfCA > 1)
        ca += (self->asdu[caIndex + 1] * 0x100);

    return ca;
}

void
CS101_ASDU_setCA(CS101_ASDU self, int ca)
{
    int caIndex = 2 + self->parameters->sizeOfCOT;

    int setCa = ca;

    /* Check if CA is in range and adjust if not */
    if (ca < 0)
        setCa = 0;
    else {
        if (self->parameters->sizeOfCA == 1) {
            if (ca > 255)
                setCa = 255;
        }
        else if (self->parameters->sizeOfCA > 1) {
            if (ca > 65535)
                setCa = 65535;
        }
    }

    if (self->parameters->sizeOfCA == 1) {
        self->asdu[caIndex] = (uint8_t) setCa;
    }
    else {
        self->asdu[caIndex] = (uint8_t) (setCa % 0x100);
        self->asdu[caIndex + 1] = (uint8_t) (setCa / 0x100);
    }
}

IEC60870_5_TypeID
CS101_ASDU_getTypeID(CS101_ASDU self)
{
    return (TypeID) (self->asdu[0]);
}

void
CS101_ASDU_setTypeID(CS101_ASDU self, IEC60870_5_TypeID typeId)
{
    self->asdu[0] = (uint8_t) typeId;
}

bool
CS101_ASDU_isSequence(CS101_ASDU self)
{
    if ((self->asdu[1] & 0x80) != 0)
        return true;
    else
        return false;
}

void
CS101_ASDU_setSequence(CS101_ASDU self, bool isSequence)
{
    if (isSequence)
        self->asdu[1] |= 0x80;
    else
        self->asdu[1] &= 0x7f;
}

int
CS101_ASDU_getNumberOfElements(CS101_ASDU self)
{
    return (self->asdu[1] & 0x7f);
}

void
CS101_ASDU_setNumberOfElements(CS101_ASDU self, int numberOfElements)
{
    self->asdu[1] &= 0x80;
    self->asdu[1] |= ((uint8_t) numberOfElements) & 0x7f;
}

InformationObject
CS101_ASDU_getElement(CS101_ASDU self, int index)
{
    return CS101_ASDU_getElementEx(self, NULL, index);
}

InformationObject
CS101_ASDU_getElementEx(CS101_ASDU self, InformationObject io, int index)
{
    InformationObject retVal = NULL;

    int elementSize;

    switch (CS101_ASDU_getTypeID(self)) {

    case M_SP_NA_1: /* 1 */

        elementSize = 1;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) SinglePointInformation_getFromBuffer((SinglePointInformation) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) SinglePointInformation_getFromBuffer((SinglePointInformation) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_SP_TA_1: /* 2 */

        elementSize = 4;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) SinglePointWithCP24Time2a_getFromBuffer((SinglePointWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) SinglePointWithCP24Time2a_getFromBuffer((SinglePointWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_DP_NA_1: /* 3 */

        elementSize = 1;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) DoublePointInformation_getFromBuffer((DoublePointInformation) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) DoublePointInformation_getFromBuffer((DoublePointInformation) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);


        break;

    case M_DP_TA_1: /* 4 */

        elementSize = 4;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) DoublePointWithCP24Time2a_getFromBuffer((DoublePointWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) DoublePointWithCP24Time2a_getFromBuffer((DoublePointWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ST_NA_1: /* 5 */

        elementSize = 2;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) StepPositionInformation_getFromBuffer((StepPositionInformation) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) StepPositionInformation_getFromBuffer((StepPositionInformation) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ST_TA_1: /* 6 */

        elementSize = 5;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) StepPositionWithCP24Time2a_getFromBuffer((StepPositionWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) StepPositionWithCP24Time2a_getFromBuffer((StepPositionWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_BO_NA_1: /* 7 */

        elementSize = 5;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) BitString32_getFromBuffer((BitString32) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) BitString32_getFromBuffer((BitString32) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_BO_TA_1: /* 8 */

        elementSize = 8;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) Bitstring32WithCP24Time2a_getFromBuffer((Bitstring32WithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) Bitstring32WithCP24Time2a_getFromBuffer((Bitstring32WithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_NA_1: /* 9 */

        elementSize = 3;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueNormalized_getFromBuffer((MeasuredValueNormalized) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueNormalized_getFromBuffer((MeasuredValueNormalized) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_TA_1: /* 10 */

        elementSize = 6;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueNormalizedWithCP24Time2a_getFromBuffer((MeasuredValueNormalizedWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueNormalizedWithCP24Time2a_getFromBuffer((MeasuredValueNormalizedWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_NB_1: /* 11 */

        elementSize = 3;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueScaled_getFromBuffer((MeasuredValueScaled) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueScaled_getFromBuffer((MeasuredValueScaled) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_TB_1: /* 12 */

        elementSize = 6;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueScaledWithCP24Time2a_getFromBuffer((MeasuredValueScaledWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueScaledWithCP24Time2a_getFromBuffer((MeasuredValueScaledWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;


    case M_ME_NC_1: /* 13 */

        elementSize = 5;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueShort_getFromBuffer((MeasuredValueShort) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueShort_getFromBuffer((MeasuredValueShort) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);


        break;

    case M_ME_TC_1: /* 14 */

        elementSize = 8;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueShortWithCP24Time2a_getFromBuffer((MeasuredValueShortWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueShortWithCP24Time2a_getFromBuffer((MeasuredValueShortWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_IT_NA_1: /* 15 */

        elementSize = 5;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) IntegratedTotals_getFromBuffer((IntegratedTotals) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) IntegratedTotals_getFromBuffer((IntegratedTotals) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_IT_TA_1: /* 16 */

        elementSize = 8;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) IntegratedTotalsWithCP24Time2a_getFromBuffer((IntegratedTotalsWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) IntegratedTotalsWithCP24Time2a_getFromBuffer((IntegratedTotalsWithCP24Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TA_1: /* 17 */

        elementSize = 6;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) EventOfProtectionEquipment_getFromBuffer((EventOfProtectionEquipment) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) EventOfProtectionEquipment_getFromBuffer((EventOfProtectionEquipment) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TB_1: /* 18 */

        elementSize = 7;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) PackedStartEventsOfProtectionEquipment_getFromBuffer((PackedStartEventsOfProtectionEquipment) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) PackedStartEventsOfProtectionEquipment_getFromBuffer((PackedStartEventsOfProtectionEquipment) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TC_1: /* 19 */

        elementSize = 7;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) PackedOutputCircuitInfo_getFromBuffer((PackedOutputCircuitInfo) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) PackedOutputCircuitInfo_getFromBuffer((PackedOutputCircuitInfo) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_PS_NA_1: /* 20 */

        elementSize = 5;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) PackedSinglePointWithSCD_getFromBuffer((PackedSinglePointWithSCD) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) PackedSinglePointWithSCD_getFromBuffer((PackedSinglePointWithSCD) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_ND_1: /* 21 */

        elementSize = 2;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueNormalizedWithoutQuality_getFromBuffer((MeasuredValueNormalizedWithoutQuality) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueNormalizedWithoutQuality_getFromBuffer((MeasuredValueNormalizedWithoutQuality) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_SP_TB_1: /* 30 */

        elementSize = 8;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) SinglePointWithCP56Time2a_getFromBuffer((SinglePointWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) SinglePointWithCP56Time2a_getFromBuffer((SinglePointWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_DP_TB_1: /* 31 */

        elementSize = 8;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) DoublePointWithCP56Time2a_getFromBuffer((DoublePointWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) DoublePointWithCP56Time2a_getFromBuffer((DoublePointWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ST_TB_1: /* 32 */

        elementSize = 9;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) StepPositionWithCP56Time2a_getFromBuffer((StepPositionWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) StepPositionWithCP56Time2a_getFromBuffer((StepPositionWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_BO_TB_1: /* 33 */

        elementSize = 12;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) Bitstring32WithCP56Time2a_getFromBuffer((Bitstring32WithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) Bitstring32WithCP56Time2a_getFromBuffer((Bitstring32WithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_TD_1: /* 34 */

        elementSize = 10;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueNormalizedWithCP56Time2a_getFromBuffer((MeasuredValueNormalizedWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueNormalizedWithCP56Time2a_getFromBuffer((MeasuredValueNormalizedWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_TE_1: /* 35 */

        elementSize = 10;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueScaledWithCP56Time2a_getFromBuffer((MeasuredValueScaledWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueScaledWithCP56Time2a_getFromBuffer((MeasuredValueScaledWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_ME_TF_1: /* 36 */

        elementSize = 12;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) MeasuredValueShortWithCP56Time2a_getFromBuffer((MeasuredValueShortWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) MeasuredValueShortWithCP56Time2a_getFromBuffer((MeasuredValueShortWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_IT_TB_1: /* 37 */

        elementSize = 12;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) IntegratedTotalsWithCP56Time2a_getFromBuffer((IntegratedTotalsWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) IntegratedTotalsWithCP56Time2a_getFromBuffer((IntegratedTotalsWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TD_1: /* 38 */

        elementSize = 10;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer((EventOfProtectionEquipmentWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer((EventOfProtectionEquipmentWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TE_1: /* 39 */

        elementSize = 11;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer((PackedStartEventsOfProtectionEquipmentWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer((PackedStartEventsOfProtectionEquipmentWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case M_EP_TF_1: /* 40 */

        elementSize = 11;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer((PackedOutputCircuitInfoWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer((PackedOutputCircuitInfoWithCP56Time2a) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    /* 41 - 44 reserved */

    case C_SC_NA_1: /* 45 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) SingleCommand_getFromBuffer((SingleCommand) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;


    case C_DC_NA_1: /* 46 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) DoubleCommand_getFromBuffer((DoubleCommand) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_RC_NA_1: /* 47 */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) StepCommand_getFromBuffer((StepCommand) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_NA_1: /* 48 - Set-point command, normalized value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) SetpointCommandNormalized_getFromBuffer((SetpointCommandNormalized) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;


    case C_SE_NB_1: /* 49 - Set-point command, scaled value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) SetpointCommandScaled_getFromBuffer((SetpointCommandScaled) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_NC_1: /* 50 - Set-point command, short floating point number */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) SetpointCommandShort_getFromBuffer((SetpointCommandShort) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_BO_NA_1: /* 51 - Bitstring command */

        elementSize = self->parameters->sizeOfIOA + 4;

        retVal = (InformationObject) Bitstring32Command_getFromBuffer((Bitstring32Command) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    /* 52 - 57 reserved */

    case C_SC_TA_1: /* 58 - Single command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) SingleCommandWithCP56Time2a_getFromBuffer((SingleCommandWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_DC_TA_1: /* 59 - Double command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) DoubleCommandWithCP56Time2a_getFromBuffer((DoubleCommandWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_RC_TA_1: /* 60 - Step command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 8;

        retVal = (InformationObject) StepCommandWithCP56Time2a_getFromBuffer((StepCommandWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_TA_1: /* 61 - Setpoint command, normalized value with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 10;

        retVal = (InformationObject) SetpointCommandNormalizedWithCP56Time2a_getFromBuffer((SetpointCommandNormalizedWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_TB_1: /* 62 - Setpoint command, scaled value with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 10;

        retVal = (InformationObject) SetpointCommandScaledWithCP56Time2a_getFromBuffer((SetpointCommandScaledWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_SE_TC_1: /* 63 - Setpoint command, short value with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 12;

        retVal = (InformationObject) SetpointCommandShortWithCP56Time2a_getFromBuffer((SetpointCommandShortWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case C_BO_TA_1: /* 64 - Bitstring command with CP56Time2a */

        elementSize = self->parameters->sizeOfIOA + 11;

        retVal = (InformationObject) Bitstring32CommandWithCP56Time2a_getFromBuffer((Bitstring32CommandWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case M_EI_NA_1: /* 70 - End of Initialization */

        retVal = (InformationObject) EndOfInitialization_getFromBuffer((EndOfInitialization) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_IC_NA_1: /* 100 - Interrogation command */

        retVal = (InformationObject) InterrogationCommand_getFromBuffer((InterrogationCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_CI_NA_1: /* 101 - Counter interrogation command */

        retVal = (InformationObject) CounterInterrogationCommand_getFromBuffer((CounterInterrogationCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_RD_NA_1: /* 102 - Read command */

        retVal = (InformationObject) ReadCommand_getFromBuffer((ReadCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        retVal = (InformationObject) ClockSynchronizationCommand_getFromBuffer((ClockSynchronizationCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_TS_NA_1: /* 104 - Test command */

        retVal = (InformationObject) TestCommand_getFromBuffer((TestCommand) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case C_RP_NA_1: /* 105 - Reset process command */

        retVal = (InformationObject) ResetProcessCommand_getFromBuffer((ResetProcessCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_CD_NA_1: /* 106 - Delay acquisition command */

        retVal = (InformationObject) DelayAcquisitionCommand_getFromBuffer((DelayAcquisitionCommand) io, self->parameters, self->payload, self->payloadSize,  0);

        break;

    case C_TS_TA_1: /* 107 - Test command with time */

        retVal = (InformationObject) TestCommandWithCP56Time2a_getFromBuffer((TestCommandWithCP56Time2a) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case P_ME_NA_1: /* 110 - Parameter of measured values, normalized value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) ParameterNormalizedValue_getFromBuffer((ParameterNormalizedValue) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_ME_NB_1: /* 111 - Parameter of measured values, scaled value */

        elementSize = self->parameters->sizeOfIOA + 3;

        retVal = (InformationObject) ParameterScaledValue_getFromBuffer((ParameterScaledValue) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_ME_NC_1: /* 112 - Parameter of measured values, short floating point number */

        elementSize = self->parameters->sizeOfIOA + 5;

        retVal = (InformationObject) ParameterFloatValue_getFromBuffer((ParameterFloatValue) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case P_AC_NA_1: /* 113 - Parameter for activation */

        elementSize = self->parameters->sizeOfIOA + 1;

        retVal = (InformationObject) ParameterActivation_getFromBuffer((ParameterActivation) io, self->parameters, self->payload, self->payloadSize,  index * elementSize);

        break;

    case F_FR_NA_1: /* 120 - File ready */

        retVal = (InformationObject) FileReady_getFromBuffer((FileReady) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_SR_NA_1: /* 121 - Section ready */

        retVal = (InformationObject) SectionReady_getFromBuffer((SectionReady) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_SC_NA_1: /* 122 - Call/Select directory/file/section */

        retVal = (InformationObject) FileCallOrSelect_getFromBuffer((FileCallOrSelect) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_LS_NA_1: /* 123 - Last segment/section */

        retVal = (InformationObject) FileLastSegmentOrSection_getFromBuffer((FileLastSegmentOrSection) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_AF_NA_1: /* 124 -  ACK file/section */

        retVal = (InformationObject) FileACK_getFromBuffer((FileACK) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_SG_NA_1: /* 125 - File segment */

        retVal = (InformationObject) FileSegment_getFromBuffer((FileSegment) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    case F_DR_TA_1: /* 126 - File directory */

        elementSize = 13;

        if (CS101_ASDU_isSequence(self)) {
            retVal  = (InformationObject) FileDirectory_getFromBuffer((FileDirectory) io, self->parameters,
                    self->payload, self->payloadSize, self->parameters->sizeOfIOA + (index * elementSize), true);

            InformationObject_setObjectAddress(retVal, InformationObject_ParseObjectAddress(self->parameters, self->payload, 0) + index);
        }
        else
            retVal  = (InformationObject) FileDirectory_getFromBuffer((FileDirectory) io, self->parameters,
                    self->payload, self->payloadSize, index * (self->parameters->sizeOfIOA + elementSize), false);

        break;

    case F_SC_NB_1: /* 127 - QueryLog */

        retVal = (InformationObject) QueryLog_getFromBuffer((QueryLog) io, self->parameters, self->payload, self->payloadSize, 0);

        break;

    default:
    	DEBUG_PRINT("type %d not supported\n", CS101_ASDU_getTypeID(self));
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

const char*
CS101_CauseOfTransmission_toString(CS101_CauseOfTransmission self)
{
    switch (self) {

    case CS101_COT_PERIODIC:
        return "PERIODIC";
    case CS101_COT_BACKGROUND_SCAN:
        return "BACKGROUND_SCAN";
    case CS101_COT_SPONTANEOUS:
        return "SPONTANEOUS";
    case CS101_COT_INITIALIZED:
        return "INITIALIZED";
    case CS101_COT_REQUEST:
        return "REQUEST";
    case CS101_COT_ACTIVATION:
        return "ACTIVATION";
    case CS101_COT_ACTIVATION_CON:
        return "ACTIVATION_CON";
    case CS101_COT_DEACTIVATION:
        return "DEACTIVATION";
    case CS101_COT_DEACTIVATION_CON:
        return "DEACTIVATION_CON";
    case CS101_COT_ACTIVATION_TERMINATION:
        return "ACTIVATION_TERMINATION";
    case CS101_COT_RETURN_INFO_REMOTE:
        return "RETURN_INFO_REMOTE";
    case CS101_COT_RETURN_INFO_LOCAL:
        return "RETURN_INFO_LOCAL";
    case CS101_COT_FILE_TRANSFER:
        return "FILE_TRANSFER";
    case CS101_COT_AUTHENTICATION:
        return "AUTHENTICATION";
    case CS101_COT_MAINTENANCE_OF_AUTH_SESSION_KEY:
        return "MAINTENANCE_OF_AUTH_SESSION_KEY";
    case CS101_COT_MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY:
        return "MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY";
    case CS101_COT_INTERROGATED_BY_STATION:
        return "INTERROGATED_BY_STATION";
    case CS101_COT_INTERROGATED_BY_GROUP_1:
        return "INTERROGATED_BY_GROUP_1";
    case CS101_COT_INTERROGATED_BY_GROUP_2:
        return "INTERROGATED_BY_GROUP_2";
    case CS101_COT_INTERROGATED_BY_GROUP_3:
        return "INTERROGATED_BY_GROUP_3";
    case CS101_COT_INTERROGATED_BY_GROUP_4:
        return "INTERROGATED_BY_GROUP_4";
    case CS101_COT_INTERROGATED_BY_GROUP_5:
        return "INTERROGATED_BY_GROUP_5";
    case CS101_COT_INTERROGATED_BY_GROUP_6:
        return "INTERROGATED_BY_GROUP_6";
    case CS101_COT_INTERROGATED_BY_GROUP_7:
        return "INTERROGATED_BY_GROUP_7";
    case CS101_COT_INTERROGATED_BY_GROUP_8:
        return "INTERROGATED_BY_GROUP_8";
    case CS101_COT_INTERROGATED_BY_GROUP_9:
        return "INTERROGATED_BY_GROUP_9";
    case CS101_COT_INTERROGATED_BY_GROUP_10:
        return "INTERROGATED_BY_GROUP_10";
    case CS101_COT_INTERROGATED_BY_GROUP_11:
        return "INTERROGATED_BY_GROUP_11";
    case CS101_COT_INTERROGATED_BY_GROUP_12:
        return "INTERROGATED_BY_GROUP_12";
    case CS101_COT_INTERROGATED_BY_GROUP_13:
        return "INTERROGATED_BY_GROUP_13";
    case CS101_COT_INTERROGATED_BY_GROUP_14:
        return "INTERROGATED_BY_GROUP_14";
    case CS101_COT_INTERROGATED_BY_GROUP_15:
        return "INTERROGATED_BY_GROUP_15";
    case CS101_COT_INTERROGATED_BY_GROUP_16:
        return "INTERROGATED_BY_GROUP_16";
    case CS101_COT_REQUESTED_BY_GENERAL_COUNTER:
        return "REQUESTED_BY_GENERAL_COUNTER";
    case CS101_COT_REQUESTED_BY_GROUP_1_COUNTER:
        return "REQUESTED_BY_GROUP_1_COUNTER";
    case CS101_COT_REQUESTED_BY_GROUP_2_COUNTER:
        return "REQUESTED_BY_GROUP_2_COUNTER";
    case CS101_COT_REQUESTED_BY_GROUP_3_COUNTER:
        return "REQUESTED_BY_GROUP_3_COUNTER";
    case CS101_COT_REQUESTED_BY_GROUP_4_COUNTER:
        return "REQUESTED_BY_GROUP_4_COUNTER";
    case CS101_COT_UNKNOWN_TYPE_ID:
        return "UNKNOWN_TYPE_ID";
    case CS101_COT_UNKNOWN_COT:
        return "UNKNOWN_CAUSE_OF_TRANSMISSION";
    case CS101_COT_UNKNOWN_CA:
        return "UNKNOWN_COMMON_ADDRESS_OF_ASDU";
    case CS101_COT_UNKNOWN_IOA:
        return "UNKNOWN_INFORMATION_OBJECT_ADDRESS";
    default:
        return "UNKNOWN_COT";
    }
}



