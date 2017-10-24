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
#include <stdlib.h>

#include "iec60870_common.h"
#include "apl_types_internal.h"
#include "information_objects.h"
#include "information_objects_internal.h"
#include "lib_memory.h"
#include "frame.h"
#include "platform_endian.h"

typedef struct sInformationObjectVFT* InformationObjectVFT;


typedef bool (*EncodeFunction)(InformationObject self, Frame frame, ConnectionParameters parameters, bool isSequence);
typedef void (*DestroyFunction)(InformationObject self);

struct sInformationObjectVFT {
    EncodeFunction encode;
    DestroyFunction destroy;
#if 0
    const char* (*toString)(InformationObject self);
#endif
};


/*****************************************
 * Basic data types
 ****************************************/

void
SingleEvent_setEventState(SingleEvent self, EventState eventState)
{
    uint8_t value = *self;

    value &= 0xfc;

    value += eventState;

    *self = value;
}

EventState
SingleEvent_getEventState(SingleEvent self)
{
    return (EventState) (*self & 0x3);
}

void
SingleEvent_setQDP(SingleEvent self, QualityDescriptorP qdp)
{
    uint8_t value = *self;

    value &= 0x03;

    value += qdp;

    *self = value;
}

QualityDescriptorP
SingleEvent_getQDP(SingleEvent self)
{
    return (QualityDescriptor) (*self & 0xfc);
}

uint16_t
StatusAndStatusChangeDetection_getSTn(StatusAndStatusChangeDetection self)
{
    return (uint16_t) (self->encodedValue[0] + 256 * self->encodedValue[1]);
}

uint16_t
StatusAndStatusChangeDetection_getCDn(StatusAndStatusChangeDetection self)
{
    return (uint16_t) (self->encodedValue[2] + 256 * self->encodedValue[3]);
}

void
StatusAndStatusChangeDetection_setSTn(StatusAndStatusChangeDetection self, uint16_t value)
{
    self->encodedValue[0] = (uint8_t) (value % 256);
    self->encodedValue[1] = (uint8_t) (value / 256);
}

bool
StatusAndStatusChangeDetection_getST(StatusAndStatusChangeDetection self, int index)
{
    if ((index >= 0) && (index < 16))
        return ((int) (StatusAndStatusChangeDetection_getSTn(self) & (2^index)) != 0);
    else
        return false;
}

bool
StatusAndStatusChangeDetection_getCD(StatusAndStatusChangeDetection self, int index)
{
    if ((index >= 0) && (index < 16))
        return ((int) (StatusAndStatusChangeDetection_getCDn(self) & (2^index)) != 0);
    else
        return false;
}


/*****************************************
 * Information object hierarchy
 *****************************************/

/*****************************************
 * InformationObject (base class)
 *****************************************/

struct sInformationObject {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;
};

bool
InformationObject_encode(InformationObject self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    return self->virtualFunctionTable->encode(self, frame, parameters, isSequence);
}

void
InformationObject_destroy(InformationObject self)
{
    self->virtualFunctionTable->destroy(self);
}

int
InformationObject_getObjectAddress(InformationObject self)
{
    return self->objectAddress;
}

void
InformationObject_setObjectAddress(InformationObject self, int ioa)
{
    self->objectAddress = ioa;
}


static void
InformationObject_encodeBase(InformationObject self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    if (!isSequence) {
        Frame_setNextByte(frame, (uint8_t)(self->objectAddress & 0xff));

        if (parameters->sizeOfIOA > 1)
            Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x100) & 0xff));

        if (parameters->sizeOfIOA > 2)
            Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x10000) & 0xff));
    }
}

int
InformationObject_ParseObjectAddress(ConnectionParameters parameters, uint8_t* msg, int startIndex)
{
    /* parse information object address */
    int ioa = msg [startIndex];

    if (parameters->sizeOfIOA > 1)
        ioa += (msg [startIndex + 1] * 0x100);

    if (parameters->sizeOfIOA > 2)
        ioa += (msg [startIndex + 2] * 0x10000);

    return ioa;
}

static void
InformationObject_getFromBuffer(InformationObject self, ConnectionParameters parameters,
        uint8_t* msg, int startIndex)
{
    /* parse information object address */
    self->objectAddress = InformationObject_ParseObjectAddress(parameters, msg, startIndex);
}


/**********************************************
 * SinglePointInformation
 **********************************************/

struct sSinglePointInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;
};

static bool
SinglePointInformation_encode(SinglePointInformation self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    return true;
}

struct sInformationObjectVFT singlePointInformationVFT = {
        (EncodeFunction) SinglePointInformation_encode,
        (DestroyFunction) SinglePointInformation_destroy
};

static void
SinglePointInformation_initialize(SinglePointInformation self)
{
    self->virtualFunctionTable = &(singlePointInformationVFT);
    self->type = M_SP_NA_1;
}

SinglePointInformation
SinglePointInformation_create(SinglePointInformation self, int ioa, bool value,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (SinglePointInformation) GLOBAL_CALLOC(1, sizeof(struct sSinglePointInformation));

    if (self != NULL)
        SinglePointInformation_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

void
SinglePointInformation_destroy(SinglePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointInformation) GLOBAL_MALLOC(sizeof(struct sSinglePointInformation));

        if (self != NULL)
            SinglePointInformation_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);
    }

    return self;
}

bool
SinglePointInformation_getValue(SinglePointInformation self)
{
    return self->value;
}

QualityDescriptor
SinglePointInformation_getQuality(SinglePointInformation self)
{
    return self->quality;
}


/**********************************************
 * StepPositionInformation
 **********************************************/

struct sStepPositionInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;
};

static bool
StepPositionInformation_encode(StepPositionInformation self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 2 : (parameters->sizeOfIOA + 2);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    return true;
}

struct sInformationObjectVFT stepPositionInformationVFT = {
        (EncodeFunction) StepPositionInformation_encode,
        (DestroyFunction) StepPositionInformation_destroy
};

static void
StepPositionInformation_initialize(StepPositionInformation self)
{
    self->virtualFunctionTable = &(stepPositionInformationVFT);
    self->type = M_ST_NA_1;
}

StepPositionInformation
StepPositionInformation_create(StepPositionInformation self, int ioa, int value, bool isTransient,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (StepPositionInformation) GLOBAL_CALLOC(1, sizeof(struct sStepPositionInformation));

    if (self != NULL)
        StepPositionInformation_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    return self;
}

void
StepPositionInformation_destroy(StepPositionInformation self)
{
    GLOBAL_FREEMEM(self);
}

int
StepPositionInformation_getObjectAddress(StepPositionInformation self)
{
    return self->objectAddress;
}

int
StepPositionInformation_getValue(StepPositionInformation self)
{
    int value = (self->vti & 0x7f);

    if (value > 63)
        value = value - 128;

    return value;
}

bool
StepPositionInformation_isTransient(StepPositionInformation self)
{
    return ((self->vti & 0x80) == 0x80);
}

QualityDescriptor
StepPositionInformation_getQuality(StepPositionInformation self)
{
    return self->quality;
}


StepPositionInformation
StepPositionInformation_getFromBuffer(StepPositionInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionInformation) GLOBAL_MALLOC(sizeof(struct sStepPositionInformation));

        if (self != NULL)
            StepPositionInformation_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];
    }

    return self;
}


/**********************************************
 * StepPositionWithCP56Time2a
 **********************************************/

struct sStepPositionWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
StepPositionWithCP56Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 9 : (parameters->sizeOfIOA + 9);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return false;
}

struct sInformationObjectVFT stepPositionWithCP56Time2aVFT = {
        (EncodeFunction) StepPositionWithCP56Time2a_encode,
        (DestroyFunction) StepPositionWithCP56Time2a_destroy
};

static void
StepPositionWithCP56Time2a_initialize(StepPositionWithCP56Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP56Time2aVFT);
    self->type = M_ST_TB_1;
}

void
StepPositionWithCP56Time2a_destroy(StepPositionWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_create(StepPositionWithCP56Time2a self, int ioa, int value, bool isTransient,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (StepPositionWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sStepPositionWithCP56Time2a));

    if (self != NULL)
        StepPositionWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
StepPositionWithCP56Time2a_getTimestamp(StepPositionWithCP56Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_getFromBuffer(StepPositionWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP56Time2a));

        if (self != NULL)
            StepPositionWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * StepPositionWithCP24Time2a
 **********************************************/

struct sStepPositionWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


static bool
StepPositionWithCP24Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT stepPositionWithCP24Time2aVFT = {
        (EncodeFunction) StepPositionWithCP24Time2a_encode,
        (DestroyFunction) StepPositionWithCP24Time2a_destroy
};

static void
StepPositionWithCP24Time2a_initialize(StepPositionWithCP24Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP24Time2aVFT);
    self->type = M_ST_TA_1;
}

void
StepPositionWithCP24Time2a_destroy(StepPositionWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_create(StepPositionWithCP24Time2a self, int ioa, int value, bool isTransient,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (StepPositionWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sStepPositionWithCP24Time2a));

    if (self != NULL)
        StepPositionWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;

    if (value > 63)
        value = 63;
    else if (value < -64)
        value = -64;

    if (value < 0)
        value = value + 128;

    self->vti = value;

    if (isTransient)
        self->vti |= 0x80;

    self->quality = quality;

    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
StepPositionWithCP24Time2a_getTimestamp(StepPositionWithCP24Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_getFromBuffer(StepPositionWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (StepPositionWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP24Time2a));

        if (self != NULL)
            StepPositionWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * DoublePointInformation
 **********************************************/

struct sDoublePointInformation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;
};

static bool
DoublePointInformation_encode(DoublePointInformation self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    return true;
}

struct sInformationObjectVFT doublePointInformationVFT = {
        (EncodeFunction) DoublePointInformation_encode,
        (DestroyFunction) DoublePointInformation_destroy
};

void
DoublePointInformation_destroy(DoublePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointInformation_initialize(DoublePointInformation self)
{
    self->virtualFunctionTable = &(doublePointInformationVFT);
    self->type = M_DP_NA_1;
}

DoublePointInformation
DoublePointInformation_create(DoublePointInformation self, int ioa, DoublePointValue value,
        QualityDescriptor quality)
{
    if (self == NULL)
        self = (DoublePointInformation) GLOBAL_CALLOC(1, sizeof(struct sDoublePointInformation));

    if (self != NULL)
        DoublePointInformation_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

DoublePointValue
DoublePointInformation_getValue(DoublePointInformation self)
{
    return self->value;
}

QualityDescriptor
DoublePointInformation_getQuality(DoublePointInformation self)
{
    return self->quality;
}

DoublePointInformation
DoublePointInformation_getFromBuffer(DoublePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointInformation) GLOBAL_MALLOC(sizeof(struct sDoublePointInformation));

        if (self != NULL)
            DoublePointInformation_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse DIQ (double point information with quality) */
        uint8_t diq = msg [startIndex++];

        self->value = (DoublePointValue) (diq & 0x03);

        self->quality = (QualityDescriptor) (diq & 0xf0);
    }

    return self;
}


/*******************************************
 * DoublePointWithCP24Time2a
 *******************************************/

struct sDoublePointWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static bool
DoublePointWithCP24Time2a_encode(DoublePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 4 : (parameters->sizeOfIOA + 4);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}


struct sInformationObjectVFT doublePointWithCP24Time2aVFT = {
        (EncodeFunction) DoublePointWithCP24Time2a_encode,
        (DestroyFunction) DoublePointWithCP24Time2a_destroy
};

void
DoublePointWithCP24Time2a_destroy(DoublePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointWithCP24Time2a_initialize(DoublePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP24Time2aVFT);
    self->type = M_DP_TA_1;
}

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_create(DoublePointWithCP24Time2a self, int ioa, DoublePointValue value,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (DoublePointWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sDoublePointWithCP24Time2a));

    if (self != NULL)
        DoublePointWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
DoublePointWithCP24Time2a_getTimestamp(DoublePointWithCP24Time2a self)
{
    return &(self->timestamp);
}

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_getFromBuffer(DoublePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP24Time2a));

        if (self != NULL)
            DoublePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse DIQ (double point information with quality) */
        uint8_t diq = msg [startIndex++];

        self->value = (DoublePointValue) (diq & 0x03);

        self->quality = (QualityDescriptor) (diq & 0xf0);

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * DoublePointWithCP56Time2a
 *******************************************/

struct sDoublePointWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
DoublePointWithCP56Time2a_encode(DoublePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}



struct sInformationObjectVFT doublePointWithCP56Time2aVFT = {
        (EncodeFunction) DoublePointWithCP56Time2a_encode,
        (DestroyFunction) DoublePointWithCP56Time2a_destroy
};

void
DoublePointWithCP56Time2a_destroy(DoublePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
DoublePointWithCP56Time2a_initialize(DoublePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP56Time2aVFT);
    self->type = M_DP_TB_1;
}

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_create(DoublePointWithCP56Time2a self, int ioa, DoublePointValue value,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (DoublePointWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sDoublePointWithCP56Time2a));

    if (self != NULL)
        DoublePointWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
DoublePointWithCP56Time2a_getTimestamp(DoublePointWithCP56Time2a self)
{
    return &(self->timestamp);
}

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_getFromBuffer(DoublePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (DoublePointWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP56Time2a));

        if (self != NULL)
            DoublePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse DIQ (double point information with quality) */
        uint8_t diq = msg [startIndex++];

        self->value = (DoublePointValue) (diq & 0x03);

        self->quality = (QualityDescriptor) (diq & 0xf0);

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * SinglePointWithCP24Time2a
 *******************************************/

struct sSinglePointWithCP24Time2a {
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


static bool
SinglePointWithCP24Time2a_encode(SinglePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 4 : (parameters->sizeOfIOA + 4);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT singlePointWithCP24Time2aVFT = {
        (EncodeFunction) SinglePointWithCP24Time2a_encode,
        (DestroyFunction) SinglePointWithCP24Time2a_destroy
};

void
SinglePointWithCP24Time2a_destroy(SinglePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
SinglePointWithCP24Time2a_initialize(SinglePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP24Time2aVFT);
    self->type = M_SP_TA_1;
}

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_create(SinglePointWithCP24Time2a self, int ioa, bool value,
        QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (SinglePointWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sSinglePointWithCP24Time2a));

    if (self != NULL)
        SinglePointWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
SinglePointWithCP24Time2a_getTimestamp(SinglePointWithCP24Time2a self)
{
    return &(self->timestamp);
}

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_getFromBuffer(SinglePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP24Time2a));

        if (self != NULL)
            SinglePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex++];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/*******************************************
 * SinglePointWithCP56Time2a
 *******************************************/

struct sSinglePointWithCP56Time2a {
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};


static bool
SinglePointWithCP56Time2a_encode(SinglePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}


struct sInformationObjectVFT singlePointWithCP56Time2aVFT = {
        (EncodeFunction) SinglePointWithCP56Time2a_encode,
        (DestroyFunction) SinglePointWithCP56Time2a_destroy
};

static void
SinglePointWithCP56Time2a_initialize(SinglePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP56Time2aVFT);
    self->type = M_SP_TB_1;
}

SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_create(SinglePointWithCP56Time2a self, int ioa, bool value,
        QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (SinglePointWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sSinglePointWithCP56Time2a));

    if (self != NULL)
        SinglePointWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

void
SinglePointWithCP56Time2a_destroy(SinglePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP56Time2a
SinglePointWithCP56Time2a_getTimestamp(SinglePointWithCP56Time2a self)
{
    return &(self->timestamp);
}


SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_getFromBuffer(SinglePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (SinglePointWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP56Time2a));

        if (self != NULL)
            SinglePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex++];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/**********************************************
 * BitString32
 **********************************************/

struct sBitString32 {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;
};

static bool
BitString32_encode(BitString32 self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    return true;
}

struct sInformationObjectVFT bitString32VFT = {
        (EncodeFunction) BitString32_encode,
        (DestroyFunction) BitString32_destroy
};

static void
BitString32_initialize(BitString32 self)
{
    self->virtualFunctionTable = &(bitString32VFT);
    self->type = M_BO_NA_1;
}

void
BitString32_destroy(BitString32 self)
{
    GLOBAL_FREEMEM(self);
}

BitString32
BitString32_create(BitString32 self, int ioa, uint32_t value)
{
    if (self == NULL)
         self = (BitString32) GLOBAL_CALLOC(1, sizeof(struct sBitString32));

    if (self != NULL)
        BitString32_initialize(self);

    self->objectAddress = ioa;
    self->value = value;

    return self;
}

uint32_t
BitString32_getValue(BitString32 self)
{
    return self->value;
}

QualityDescriptor
BitString32_getQuality(BitString32 self)
{
    return self->quality;
}

BitString32
BitString32_getFromBuffer(BitString32 self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (BitString32) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            BitString32_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/**********************************************
 * Bitstring32WithCP24Time2a
 **********************************************/

struct sBitstring32WithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static bool
Bitstring32WithCP24Time2a_encode(Bitstring32WithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT bitstring32WithCP24Time2aVFT = {
        (EncodeFunction) Bitstring32WithCP24Time2a_encode,
        (DestroyFunction) Bitstring32WithCP24Time2a_destroy
};

static void
Bitstring32WithCP24Time2a_initialize(Bitstring32WithCP24Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP24Time2aVFT);
    self->type = M_BO_TA_1;
}

void
Bitstring32WithCP24Time2a_destroy(Bitstring32WithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_create(Bitstring32WithCP24Time2a self, int ioa, uint32_t value, CP24Time2a timestamp)
{
    if (self == NULL)
         self = (Bitstring32WithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sBitstring32WithCP24Time2a));

    if (self != NULL)
        Bitstring32WithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
Bitstring32WithCP24Time2a_getTimestamp(Bitstring32WithCP24Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_getFromBuffer(Bitstring32WithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (Bitstring32WithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/**********************************************
 * Bitstring32WithCP56Time2a
 **********************************************/

struct sBitstring32WithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
Bitstring32WithCP56Time2a_encode(Bitstring32WithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 12 : (parameters->sizeOfIOA + 12);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT bitstring32WithCP56Time2aVFT = {
        (EncodeFunction) Bitstring32WithCP56Time2a_encode,
        (DestroyFunction) Bitstring32WithCP56Time2a_destroy
};

static void
Bitstring32WithCP56Time2a_initialize(Bitstring32WithCP56Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP56Time2aVFT);
    self->type = M_BO_TB_1;
}

void
Bitstring32WithCP56Time2a_destroy(Bitstring32WithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_create(Bitstring32WithCP56Time2a self, int ioa, uint32_t value, CP56Time2a timestamp)
{
    if (self == NULL)
         self = (Bitstring32WithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sBitstring32WithCP56Time2a));

    if (self != NULL)
        Bitstring32WithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
Bitstring32WithCP56Time2a_getTimestamp(Bitstring32WithCP56Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_getFromBuffer(Bitstring32WithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (Bitstring32WithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);
        self->value = value;

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/**********************************************
 * MeasuredValueNormalized
 **********************************************/

struct sMeasuredValueNormalized {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

static int
getScaledValue(uint8_t* encodedValue)
{
    int value;

    value = encodedValue[0];
    value += (encodedValue[1] * 0x100);

    if (value > 32767)
        value = value - 65536;

    return value;
}

static void
setScaledValue(uint8_t* encodedValue, int value)
{
    int valueToEncode;

    if (value < 0)
        valueToEncode = value + 65536;
    else
        valueToEncode = value;

    encodedValue[0] = (uint8_t) (valueToEncode % 256);
    encodedValue[1] = (uint8_t) (valueToEncode / 256);
}

static bool
MeasuredValueNormalized_encode(MeasuredValueNormalized self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 3 : (parameters->sizeOfIOA + 3);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->encodedValue[0]);
    Frame_setNextByte(frame, self->encodedValue[1]);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    return true;
}

struct sInformationObjectVFT measuredValueNormalizedVFT = {
        (EncodeFunction) MeasuredValueNormalized_encode,
        (DestroyFunction) MeasuredValueNormalized_destroy
};

static void
MeasuredValueNormalized_initialize(MeasuredValueNormalized self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedVFT);
    self->type = M_ME_NA_1;
}

void
MeasuredValueNormalized_destroy(MeasuredValueNormalized self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalized
MeasuredValueNormalized_create(MeasuredValueNormalized self, int ioa, float value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueNormalized) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalized));

    if (self != NULL)
        MeasuredValueNormalized_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue(self, value);

    self->quality = quality;

    return self;
}

float
MeasuredValueNormalized_getValue(MeasuredValueNormalized self)
{
    float nv = (float) (getScaledValue(self->encodedValue)) / 32767.f;

    return nv;
}

void
MeasuredValueNormalized_setValue(MeasuredValueNormalized self, float value)
{
    if (value > 1.0f)
        value = 1.0f;
    else if (value < -1.0f)
        value = -1.0f;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);
}

QualityDescriptor
MeasuredValueNormalized_getQuality(MeasuredValueNormalized self)
{
    return self->quality;
}

MeasuredValueNormalized
MeasuredValueNormalized_getFromBuffer(MeasuredValueNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalized) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalized));

        if (self != NULL)
            MeasuredValueNormalized_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/**********************************************
 * ParameterNormalizedValue
 **********************************************/

void
ParameterNormalizedValue_destroy(ParameterNormalizedValue self)
{
    GLOBAL_FREEMEM(self);
}

ParameterNormalizedValue
ParameterNormalizedValue_create(ParameterNormalizedValue self, int ioa, float value, QualifierOfParameterMV quality)
{
    ParameterNormalizedValue pvn =
            MeasuredValueNormalized_create(self, ioa, value, (QualityDescriptor) quality);

    pvn->type = P_ME_NA_1;

    return pvn;
}

float
ParameterNormalizedValue_getValue(ParameterNormalizedValue self)
{
    return MeasuredValueNormalized_getValue(self);
}

void
ParameterNormalizedValue_setValue(ParameterNormalizedValue self, float value)
{
    MeasuredValueNormalized_setValue(self, value);
}

QualifierOfParameterMV
ParameterNormalizedValue_getQPM(ParameterNormalizedValue self)
{
    return self->quality;
}

ParameterNormalizedValue
ParameterNormalizedValue_getFromBuffer(ParameterNormalizedValue self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    MeasuredValueNormalized pvn =
            MeasuredValueNormalized_getFromBuffer(self, parameters, msg, msgSize, startIndex, false);

    pvn->type = P_ME_NA_1;

    return (ParameterNormalizedValue) pvn;
}


/*************************************************************
 * MeasuredValueNormalizedWithoutQuality : InformationObject
 *************************************************************/

struct sMeasuredValueNormalizedWithoutQuality {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];
};

static bool
MeasuredValueNormalizedWithoutQuality_encode(MeasuredValueNormalizedWithoutQuality self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 2 : (parameters->sizeOfIOA + 2);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->encodedValue[0]);
    Frame_setNextByte(frame, self->encodedValue[1]);

    return true;
}

struct sInformationObjectVFT measuredValueNormalizedWithoutQualityVFT = {
        (EncodeFunction) MeasuredValueNormalizedWithoutQuality_encode,
        (DestroyFunction) MeasuredValueNormalizedWithoutQuality_destroy
};

static void
MeasuredValueNormalizedWithoutQuality_initialize(MeasuredValueNormalizedWithoutQuality self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithoutQualityVFT);
    self->type = M_ME_ND_1;
}

void
MeasuredValueNormalizedWithoutQuality_destroy(MeasuredValueNormalizedWithoutQuality self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalizedWithoutQuality
MeasuredValueNormalizedWithoutQuality_create(MeasuredValueNormalizedWithoutQuality self, int ioa, float value)
{
    if (self == NULL)
        self = (MeasuredValueNormalizedWithoutQuality) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalizedWithoutQuality));

    if (self != NULL)
        MeasuredValueNormalizedWithoutQuality_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalizedWithoutQuality_setValue(self, value);

    return self;
}

float
MeasuredValueNormalizedWithoutQuality_getValue(MeasuredValueNormalizedWithoutQuality self)
{
    float nv = ((float) (getScaledValue(self->encodedValue) + 0.5) / 32767.5f);

    return nv;
}

void
MeasuredValueNormalizedWithoutQuality_setValue(MeasuredValueNormalizedWithoutQuality self, float value)
{
    if (value > 1.0f)
        value = 1.0f;
    else if (value < -1.0f)
        value = -1.0f;

    int scaledValue = (int) ((value * 32767.5f) - 0.5);

    setScaledValue(self->encodedValue, scaledValue);
}

MeasuredValueNormalizedWithoutQuality
MeasuredValueNormalizedWithoutQuality_getFromBuffer(MeasuredValueNormalizedWithoutQuality self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
        self = (MeasuredValueNormalizedWithoutQuality) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithoutQuality));

        if (self != NULL)
            MeasuredValueNormalizedWithoutQuality_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];
    }

    return self;
}

/***********************************************************************
 * MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
 ***********************************************************************/

struct sMeasuredValueNormalizedWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static bool
MeasuredValueNormalizedWithCP24Time2a_encode(MeasuredValueNormalizedWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 6 : (parameters->sizeOfIOA + 6);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters, isSequence);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT measuredValueNormalizedWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueNormalizedWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueNormalizedWithCP24Time2a_destroy
};

static void
MeasuredValueNormalizedWithCP24Time2a_initialize(MeasuredValueNormalizedWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP24Time2aVFT);
    self->type = M_ME_TA_1;
}

void
MeasuredValueNormalizedWithCP24Time2a_destroy(MeasuredValueNormalizedWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_create(MeasuredValueNormalizedWithCP24Time2a self, int ioa,
            float value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueNormalizedWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalizedWithCP24Time2a));

    if (self != NULL)
        MeasuredValueNormalizedWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue((MeasuredValueNormalized) self, value);

    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getTimestamp(MeasuredValueNormalizedWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueNormalizedWithCP24Time2a_setTimestamp(MeasuredValueNormalizedWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(MeasuredValueNormalizedWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalizedWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithCP24Time2a));

        if (self != NULL)
            MeasuredValueNormalizedWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
             InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

             startIndex += parameters->sizeOfIOA; /* skip IOA */
         }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * MeasuredValueNormalizedWithCP56Time2a : MeasuredValueNormalized
 ***********************************************************************/

struct sMeasuredValueNormalizedWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
MeasuredValueNormalizedWithCP56Time2a_encode(MeasuredValueNormalizedWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 10 : (parameters->sizeOfIOA + 10);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters, isSequence);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT measuredValueNormalizedWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueNormalizedWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueNormalizedWithCP56Time2a_destroy
};

static void
MeasuredValueNormalizedWithCP56Time2a_initialize(MeasuredValueNormalizedWithCP56Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP56Time2aVFT);
    self->type = M_ME_TD_1;
}

void
MeasuredValueNormalizedWithCP56Time2a_destroy(MeasuredValueNormalizedWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_create(MeasuredValueNormalizedWithCP56Time2a self, int ioa,
            float value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueNormalizedWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueNormalizedWithCP56Time2a));

    if (self != NULL)
        MeasuredValueNormalizedWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;

    MeasuredValueNormalized_setValue((MeasuredValueNormalized) self, value);

    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}


CP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getTimestamp(MeasuredValueNormalizedWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueNormalizedWithCP56Time2a_setTimestamp(MeasuredValueNormalizedWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getFromBuffer(MeasuredValueNormalizedWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueNormalizedWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithCP56Time2a));

        if (self != NULL)
            MeasuredValueNormalizedWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}



/*******************************************
 * MeasuredValueScaled
 *******************************************/

struct sMeasuredValueScaled {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

static bool
MeasuredValueScaled_encode(MeasuredValueScaled self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    return MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters, isSequence);
}

struct sInformationObjectVFT measuredValueScaledVFT = {
        (EncodeFunction) MeasuredValueScaled_encode,
        (DestroyFunction) MeasuredValueScaled_destroy
};

static void
MeasuredValueScaled_initialize(MeasuredValueScaled self)
{
    self->virtualFunctionTable = &(measuredValueScaledVFT);
    self->type = M_ME_NB_1;
}

MeasuredValueScaled
MeasuredValueScaled_create(MeasuredValueScaled self, int ioa, int value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueScaled) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaled));

    if (self != NULL)
        MeasuredValueScaled_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;

    return self;
}


void
MeasuredValueScaled_destroy(MeasuredValueScaled self)
{
    GLOBAL_FREEMEM(self);
}


int
MeasuredValueScaled_getValue(MeasuredValueScaled self)
{
    return getScaledValue(self->encodedValue);
}

void
MeasuredValueScaled_setValue(MeasuredValueScaled self, int value)
{
    setScaledValue(self->encodedValue, value);
}

QualityDescriptor
MeasuredValueScaled_getQuality(MeasuredValueScaled self)
{
    return self->quality;
}

void
MeasuredValueScaled_setQuality(MeasuredValueScaled self, QualityDescriptor quality)
{
    self->quality = quality;
}

MeasuredValueScaled
MeasuredValueScaled_getFromBuffer(MeasuredValueScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaled) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaled));

        if (self != NULL)
            MeasuredValueScaled_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/******************************************************
 * ParameterScaledValue : MeasuredValueScaled
 *****************************************************/

void
ParameterScaledValue_destroy(ParameterScaledValue self)
{
    GLOBAL_FREEMEM(self);
}

ParameterScaledValue
ParameterScaledValue_create(ParameterScaledValue self, int ioa, int value, QualifierOfParameterMV qpm)
{
    ParameterScaledValue pvn =
            MeasuredValueScaled_create(self, ioa, value, qpm);

    pvn->type = P_ME_NB_1;

    return pvn;
}

int
ParameterScaledValue_getValue(ParameterScaledValue self)
{
    return getScaledValue(self->encodedValue);
}

void
ParameterScaledValue_setValue(ParameterScaledValue self, int value)
{
    setScaledValue(self->encodedValue, value);
}

QualifierOfParameterMV
ParameterScaledValue_getQPM(ParameterScaledValue self)
{
    return self->quality;
}

ParameterScaledValue
ParameterScaledValue_getFromBuffer(ParameterScaledValue self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    MeasuredValueScaled psv =
            MeasuredValueScaled_getFromBuffer(self, parameters, msg, msgSize, startIndex, false);

    psv->type = P_ME_NB_1;

    return (ParameterScaledValue) psv;
}


/*******************************************
 * MeasuredValueScaledWithCP24Time2a
 *******************************************/

struct sMeasuredValueScaledWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static bool
MeasuredValueScaledWithCP24Time2a_encode(MeasuredValueScaledWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 6 : (parameters->sizeOfIOA + 6);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT measuredValueScaledWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueScaledWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueScaled_destroy
};

static void
MeasuredValueScaledWithCP24Time2a_initialize(MeasuredValueScaledWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueScaledWithCP24Time2aVFT);
    self->type = M_ME_TB_1;
}

void
MeasuredValueScaledWithCP24Time2a_destroy(MeasuredValueScaledWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_create(MeasuredValueScaledWithCP24Time2a self, int ioa,
        int value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueScaledWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaledWithCP24Time2a));

    if (self != NULL)
        MeasuredValueScaledWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
MeasuredValueScaledWithCP24Time2a_getTimestamp(MeasuredValueScaledWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueScaledWithCP24Time2a_setTimestamp(MeasuredValueScaledWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_getFromBuffer(MeasuredValueScaledWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaledWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaledWithCP24Time2a));

        if (self != NULL)
            MeasuredValueScaledWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueScaledWithCP56Time2a
 *******************************************/

struct sMeasuredValueScaledWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
MeasuredValueScaledWithCP56Time2a_encode(MeasuredValueScaledWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 10 : (parameters->sizeOfIOA + 10);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters, isSequence);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT measuredValueScaledWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueScaledWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueScaledWithCP56Time2a_destroy
};

static void
MeasuredValueScaledWithCP56Time2a_initialize(MeasuredValueScaledWithCP56Time2a self)
{
    self->virtualFunctionTable = &measuredValueScaledWithCP56Time2aVFT;
    self->type = M_ME_TE_1;
}

void
MeasuredValueScaledWithCP56Time2a_destroy(MeasuredValueScaledWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_create(MeasuredValueScaledWithCP56Time2a self, int ioa,
        int value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueScaledWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueScaledWithCP56Time2a));

    if (self != NULL)
        MeasuredValueScaledWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    setScaledValue(self->encodedValue, value);
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
MeasuredValueScaledWithCP56Time2a_getTimestamp(MeasuredValueScaledWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueScaledWithCP56Time2a_setTimestamp(MeasuredValueScaledWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueScaledWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaledWithCP56Time2a));

        if (self != NULL)
            MeasuredValueScaledWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* scaled value */
        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueShort
 *******************************************/

struct sMeasuredValueShort {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;
};

static bool
MeasuredValueShort_encode(MeasuredValueShort self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif

    Frame_setNextByte(frame, (uint8_t) self->quality);

    return true;
}

struct sInformationObjectVFT measuredValueShortVFT = {
        (EncodeFunction) MeasuredValueShort_encode,
        (DestroyFunction) MeasuredValueShort_destroy
};

static void
MeasuredValueShort_initialize(MeasuredValueShort self)
{
    self->virtualFunctionTable = &(measuredValueShortVFT);
    self->type = M_ME_NC_1;
}

void
MeasuredValueShort_destroy(MeasuredValueShort self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShort
MeasuredValueShort_create(MeasuredValueShort self, int ioa, float value, QualityDescriptor quality)
{
    if (self == NULL)
        self = (MeasuredValueShort) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShort));

    if (self != NULL)
        MeasuredValueShort_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;

    return self;
}

float
MeasuredValueShort_getValue(MeasuredValueShort self)
{
    return self->value;
}

void
MeasuredValueShort_setValue(MeasuredValueShort self, float value)
{
    self->value = value;
}

QualityDescriptor
MeasuredValueShort_getQuality(MeasuredValueShort self)
{
    return self->quality;
}

MeasuredValueShort
MeasuredValueShort_getFromBuffer(MeasuredValueShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShort) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShort));

        if (self != NULL)
            MeasuredValueShort_initialize(self);
    }

    if (self != NULL) {
        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/******************************************************
 * ParameterFloatValue : MeasuredValueShort
 *****************************************************/

void
ParameterFloatValue_destroy(ParameterFloatValue self)
{
    GLOBAL_FREEMEM(self);
}

ParameterFloatValue
ParameterFloatValue_create(ParameterFloatValue self, int ioa, float value, QualifierOfParameterMV qpm)
{
    ParameterFloatValue pvf =
            MeasuredValueShort_create(self, ioa, value, (QualityDescriptor) qpm);

    pvf->type = P_ME_NC_1;

    return pvf;
}

float
ParameterFloatValue_getValue(ParameterFloatValue self)
{
    return self->value;
}

void
ParameterFloatValue_setValue(ParameterFloatValue self, float value)
{
    self->value = value;
}

QualifierOfParameterMV
ParameterFloatValue_getQPM(ParameterFloatValue self)
{
    return self->quality;
}

ParameterFloatValue
ParameterFloatValue_getFromBuffer(ParameterFloatValue self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    ParameterFloatValue psv =
            MeasuredValueShort_getFromBuffer(self, parameters, msg, msgSize, startIndex, false);

    psv->type = P_ME_NC_1;

    return (ParameterFloatValue) psv;
}

/*******************************************
 * MeasuredValueFloatWithCP24Time2a
 *******************************************/

struct sMeasuredValueShortWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static bool
MeasuredValueShortWithCP24Time2a_encode(MeasuredValueShortWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueShort_encode((MeasuredValueShort) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT measuredValueShortWithCP24Time2aVFT = {
        (EncodeFunction) MeasuredValueShortWithCP24Time2a_encode,
        (DestroyFunction) MeasuredValueShortWithCP24Time2a_destroy
};

static void
MeasuredValueShortWithCP24Time2a_initialize(MeasuredValueShortWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueShortWithCP24Time2aVFT);
    self->type = M_ME_TC_1;
}

void
MeasuredValueShortWithCP24Time2a_destroy(MeasuredValueShortWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_create(MeasuredValueShortWithCP24Time2a self, int ioa,
        float value, QualityDescriptor quality, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueShortWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShortWithCP24Time2a));

    if (self != NULL)
        MeasuredValueShortWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP24Time2a
MeasuredValueShortWithCP24Time2a_getTimestamp(MeasuredValueShortWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueShortWithCP24Time2a_setTimestamp(MeasuredValueShortWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_getFromBuffer(MeasuredValueShortWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShortWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShortWithCP24Time2a));

        if (self != NULL)
            MeasuredValueShortWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueFloatWithCP56Time2a
 *******************************************/

struct sMeasuredValueShortWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static bool
MeasuredValueShortWithCP56Time2a_encode(MeasuredValueShortWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 12 : (parameters->sizeOfIOA + 12);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    MeasuredValueShort_encode((MeasuredValueShort) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT measuredValueShortWithCP56Time2aVFT = {
        (EncodeFunction) MeasuredValueShortWithCP56Time2a_encode,
        (DestroyFunction) MeasuredValueShortWithCP56Time2a_destroy
};

static void
MeasuredValueShortWithCP56Time2a_initialize(MeasuredValueShortWithCP56Time2a self)
{
    self->virtualFunctionTable = &(measuredValueShortWithCP56Time2aVFT);
    self->type = M_ME_TF_1;
}

void
MeasuredValueShortWithCP56Time2a_destroy(MeasuredValueShortWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_create(MeasuredValueShortWithCP56Time2a self, int ioa,
        float value, QualityDescriptor quality, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (MeasuredValueShortWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sMeasuredValueShortWithCP56Time2a));

    if (self != NULL)
        MeasuredValueShortWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->value = value;
    self->quality = quality;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
MeasuredValueShortWithCP56Time2a_getTimestamp(MeasuredValueShortWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueShortWithCP56Time2a_setTimestamp(MeasuredValueShortWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_getFromBuffer(MeasuredValueShortWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (MeasuredValueShortWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sMeasuredValueShortWithCP56Time2a));

        if (self != NULL)
            MeasuredValueShortWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * IntegratedTotals
 *******************************************/

struct sIntegratedTotals {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;
};

static bool
IntegratedTotals_encode(IntegratedTotals self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->totals.encodedValue, 5);

    return true;
}

struct sInformationObjectVFT integratedTotalsVFT = {
        (EncodeFunction) IntegratedTotals_encode,
        (DestroyFunction) IntegratedTotals_destroy
};

static void
IntegratedTotals_initialize(IntegratedTotals self)
{
    self->virtualFunctionTable = &(integratedTotalsVFT);
    self->type = M_IT_NA_1;
}

void
IntegratedTotals_destroy(IntegratedTotals self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotals
IntegratedTotals_create(IntegratedTotals self, int ioa, BinaryCounterReading value)
{
    if (self == NULL)
        self = (IntegratedTotals) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotals));

    if (self != NULL)
        IntegratedTotals_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;

    return self;
}

BinaryCounterReading
IntegratedTotals_getBCR(IntegratedTotals self)
{
    return &(self->totals);
}

void
IntegratedTotals_setBCR(IntegratedTotals self, BinaryCounterReading value)
{
    int i;

    for (i = 0; i < 5; i++)
        self->totals.encodedValue[i] = value->encodedValue[i];
}

IntegratedTotals
IntegratedTotals_getFromBuffer(IntegratedTotals self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotals) GLOBAL_MALLOC(sizeof(struct sIntegratedTotals));

        if (self != NULL)
            IntegratedTotals_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];
    }

    return self;
}

/***********************************************************************
 * IntegratedTotalsWithCP24Time2a : IntegratedTotals
 ***********************************************************************/

struct sIntegratedTotalsWithCP24Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP24Time2a timestamp;
};

static bool
IntegratedTotalsWithCP24Time2a_encode(IntegratedTotalsWithCP24Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    IntegratedTotals_encode((IntegratedTotals) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT integratedTotalsWithCP24Time2aVFT = {
        (EncodeFunction) IntegratedTotalsWithCP24Time2a_encode,
        (DestroyFunction) IntegratedTotalsWithCP24Time2a_destroy
};

static void
IntegratedTotalsWithCP24Time2a_initialize(IntegratedTotalsWithCP24Time2a self)
{
    self->virtualFunctionTable = &(integratedTotalsWithCP24Time2aVFT);
    self->type = M_IT_TA_1;
}

void
IntegratedTotalsWithCP24Time2a_destroy(IntegratedTotalsWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_create(IntegratedTotalsWithCP24Time2a self, int ioa,
        BinaryCounterReading value, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (IntegratedTotalsWithCP24Time2a) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotalsWithCP24Time2a));

    if (self != NULL)
        IntegratedTotalsWithCP24Time2a_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;
    self->timestamp = *timestamp;

    return self;
}


CP24Time2a
IntegratedTotalsWithCP24Time2a_getTimestamp(IntegratedTotalsWithCP24Time2a self)
{
    return &(self->timestamp);
}

void
IntegratedTotalsWithCP24Time2a_setTimestamp(IntegratedTotalsWithCP24Time2a self,
        CP24Time2a value)
{
    int i;
    for (i = 0; i < 3; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_getFromBuffer(IntegratedTotalsWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotalsWithCP24Time2a) GLOBAL_MALLOC(sizeof(struct sIntegratedTotalsWithCP24Time2a));

        if (self != NULL)
            IntegratedTotalsWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * IntegratedTotalsWithCP56Time2a : IntegratedTotals
 ***********************************************************************/

struct sIntegratedTotalsWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP56Time2a timestamp;
};

static bool
IntegratedTotalsWithCP56Time2a_encode(IntegratedTotalsWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 12 : (parameters->sizeOfIOA + 12);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    IntegratedTotals_encode((IntegratedTotals) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT integratedTotalsWithCP56Time2aVFT = {
        (EncodeFunction) IntegratedTotalsWithCP56Time2a_encode,
        (DestroyFunction) IntegratedTotalsWithCP56Time2a_destroy
};

static void
IntegratedTotalsWithCP56Time2a_initialize(IntegratedTotalsWithCP56Time2a self)
{
    self->virtualFunctionTable = &(integratedTotalsWithCP56Time2aVFT);
    self->type = M_IT_TB_1;
}

void
IntegratedTotalsWithCP56Time2a_destroy(IntegratedTotalsWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_create(IntegratedTotalsWithCP56Time2a self, int ioa,
        BinaryCounterReading value, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (IntegratedTotalsWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sIntegratedTotalsWithCP56Time2a));

    if (self != NULL)
        IntegratedTotalsWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->totals = *value;
    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
IntegratedTotalsWithCP56Time2a_getTimestamp(IntegratedTotalsWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
IntegratedTotalsWithCP56Time2a_setTimestamp(IntegratedTotalsWithCP56Time2a self,
        CP56Time2a value)
{
    int i;
    for (i = 0; i < 7; i++) {
        self->timestamp.encodedValue[i] = value->encodedValue[i];
    }
}


IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_getFromBuffer(IntegratedTotalsWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    //TODO check message size

    if (self == NULL) {
		self = (IntegratedTotalsWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sIntegratedTotalsWithCP56Time2a));

        if (self != NULL)
            IntegratedTotalsWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* BCR */
        int i = 0;

        for (i = 0; i < 5; i++)
            self->totals.encodedValue[i] = msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/***********************************************************************
 * EventOfProtectionEquipment : InformationObject
 ***********************************************************************/

struct sEventOfProtectionEquipment {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tSingleEvent event;

    struct sCP16Time2a elapsedTime;

    struct sCP24Time2a timestamp;
};

static bool
EventOfProtectionEquipment_encode(EventOfProtectionEquipment self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 6 : (parameters->sizeOfIOA + 6);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->event);

    Frame_appendBytes(frame, self->elapsedTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT eventOfProtectionEquipmentVFT = {
        (EncodeFunction) EventOfProtectionEquipment_encode,
        (DestroyFunction) EventOfProtectionEquipment_destroy
};

static void
EventOfProtectionEquipment_initialize(EventOfProtectionEquipment self)
{
    self->virtualFunctionTable = &(eventOfProtectionEquipmentVFT);
    self->type = M_EP_TA_1;
}

void
EventOfProtectionEquipment_destroy(EventOfProtectionEquipment self)
{
    GLOBAL_FREEMEM(self);
}

EventOfProtectionEquipment
EventOfProtectionEquipment_create(EventOfProtectionEquipment self, int ioa,
        SingleEvent event, CP16Time2a elapsedTime, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (EventOfProtectionEquipment) GLOBAL_CALLOC(1, sizeof(struct sEventOfProtectionEquipment));

    if (self != NULL)
        EventOfProtectionEquipment_initialize(self);

    self->objectAddress = ioa;
    self->event = *event;
    self->elapsedTime = *elapsedTime;
    self->timestamp = *timestamp;

    return self;
}

EventOfProtectionEquipment
EventOfProtectionEquipment_getFromBuffer(EventOfProtectionEquipment self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 6))
        return NULL;

    if (self == NULL) {
        self = (EventOfProtectionEquipment) GLOBAL_MALLOC(sizeof(struct sEventOfProtectionEquipment));

        if (self != NULL)
            EventOfProtectionEquipment_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* event */
        self->event = msg[startIndex++];

        /* elapsed time */
        CP16Time2a_getFromBuffer(&(self->elapsedTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


SingleEvent
EventOfProtectionEquipment_getEvent(EventOfProtectionEquipment self)
{
    return &(self->event);
}

CP16Time2a
EventOfProtectionEquipment_getElapsedTime(EventOfProtectionEquipment self)
{
    return &(self->elapsedTime);
}

CP24Time2a
EventOfProtectionEquipment_getTimestamp(EventOfProtectionEquipment self)
{
    return &(self->timestamp);
}

/***********************************************************************
 * EventOfProtectionEquipmentWithCP56Time2a : InformationObject
 ***********************************************************************/

struct sEventOfProtectionEquipmentWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tSingleEvent event;

    struct sCP16Time2a elapsedTime;

    struct sCP56Time2a timestamp;
};

static bool
EventOfProtectionEquipmentWithCP56Time2a_encode(EventOfProtectionEquipmentWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 10 : (parameters->sizeOfIOA + 10);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->event);

    Frame_appendBytes(frame, self->elapsedTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT eventOfProtectionEquipmentWithCP56Time2aVFT = {
        (EncodeFunction) EventOfProtectionEquipmentWithCP56Time2a_encode,
        (DestroyFunction) EventOfProtectionEquipmentWithCP56Time2a_destroy
};

static void
EventOfProtectionEquipmentWithCP56Time2a_initialize(EventOfProtectionEquipmentWithCP56Time2a self)
{
    self->virtualFunctionTable = &(eventOfProtectionEquipmentWithCP56Time2aVFT);
    self->type = M_EP_TD_1;
}

void
EventOfProtectionEquipmentWithCP56Time2a_destroy(EventOfProtectionEquipmentWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

EventOfProtectionEquipmentWithCP56Time2a
EventOfProtectionEquipmentWithCP56Time2a_create(EventOfProtectionEquipmentWithCP56Time2a self, int ioa,
        SingleEvent event, CP16Time2a elapsedTime, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (EventOfProtectionEquipmentWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sEventOfProtectionEquipmentWithCP56Time2a));

    if (self != NULL)
        EventOfProtectionEquipmentWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->event = *event;
    self->elapsedTime = *elapsedTime;
    self->timestamp = *timestamp;

    return self;
}

SingleEvent
EventOfProtectionEquipmentWithCP56Time2a_getEvent(EventOfProtectionEquipmentWithCP56Time2a self)
{
    return &(self->event);
}

CP16Time2a
EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(EventOfProtectionEquipmentWithCP56Time2a self)
{
    return &(self->elapsedTime);
}

CP56Time2a
EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(EventOfProtectionEquipmentWithCP56Time2a self)
{
    return &(self->timestamp);
}

EventOfProtectionEquipmentWithCP56Time2a
EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer(EventOfProtectionEquipmentWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 6))
        return NULL;

    if (self == NULL) {
        self = (EventOfProtectionEquipmentWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sEventOfProtectionEquipmentWithCP56Time2a));

        if (self != NULL)
            EventOfProtectionEquipmentWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* event */
        self->event = msg[startIndex++];

        /* elapsed time */
        CP16Time2a_getFromBuffer(&(self->elapsedTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * PackedStartEventsOfProtectionEquipment : InformationObject
 ***********************************************************************/

struct sPackedStartEventsOfProtectionEquipment {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    StartEvent event;

    QualityDescriptorP qdp;

    struct sCP16Time2a elapsedTime;

    struct sCP24Time2a timestamp;
};

static bool
PackedStartEventsOfProtectionEquipment_encode(PackedStartEventsOfProtectionEquipment self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 7 : (parameters->sizeOfIOA + 7);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->event);

    Frame_setNextByte(frame, (uint8_t) self->qdp);

    Frame_appendBytes(frame, self->elapsedTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT packedStartEventsOfProtectionEquipmentVFT = {
        (EncodeFunction) PackedStartEventsOfProtectionEquipment_encode,
        (DestroyFunction) PackedStartEventsOfProtectionEquipment_destroy
};

static void
PackedStartEventsOfProtectionEquipment_initialize(PackedStartEventsOfProtectionEquipment self)
{
    self->virtualFunctionTable = &(packedStartEventsOfProtectionEquipmentVFT);
    self->type = M_EP_TB_1;
}

void
PackedStartEventsOfProtectionEquipment_destroy(PackedStartEventsOfProtectionEquipment self)
{
    GLOBAL_FREEMEM(self);
}

PackedStartEventsOfProtectionEquipment
PackedStartEventsOfProtectionEquipment_create(PackedStartEventsOfProtectionEquipment self, int ioa,
        StartEvent event, QualityDescriptorP qdp, CP16Time2a elapsedTime, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (PackedStartEventsOfProtectionEquipment) GLOBAL_CALLOC(1, sizeof(struct sPackedStartEventsOfProtectionEquipment));

    if (self != NULL)
        PackedStartEventsOfProtectionEquipment_initialize(self);

    self->objectAddress = ioa;
    self->event = event;
    self->qdp = qdp;
    self->elapsedTime = *elapsedTime;
    self->timestamp = *timestamp;

    return self;
}

StartEvent
PackedStartEventsOfProtectionEquipment_getEvent(PackedStartEventsOfProtectionEquipment self)
{
    return self->event;
}

QualityDescriptorP
PackedStartEventsOfProtectionEquipment_getQuality(PackedStartEventsOfProtectionEquipment self)
{
    return self->qdp;
}

CP16Time2a
PackedStartEventsOfProtectionEquipment_getElapsedTime(PackedStartEventsOfProtectionEquipment self)
{
    return &(self->elapsedTime);
}

CP24Time2a
PackedStartEventsOfProtectionEquipment_getTimestamp(PackedStartEventsOfProtectionEquipment self)
{
    return &(self->timestamp);
}

PackedStartEventsOfProtectionEquipment
PackedStartEventsOfProtectionEquipment_getFromBuffer(PackedStartEventsOfProtectionEquipment self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 7))
        return NULL;

    if (self == NULL) {
        self = (PackedStartEventsOfProtectionEquipment) GLOBAL_MALLOC(sizeof(struct sPackedStartEventsOfProtectionEquipment));

        if (self != NULL)
            PackedStartEventsOfProtectionEquipment_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* event */
        self->event = msg[startIndex++];

        /* qdp */
        self->qdp = msg[startIndex++];

        /* elapsed time */
        CP16Time2a_getFromBuffer(&(self->elapsedTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***************************************************************************
 * PackedStartEventsOfProtectionEquipmentWithCP56Time2a : InformationObject
 ***************************************************************************/

struct sPackedStartEventsOfProtectionEquipmentWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    StartEvent event;

    QualityDescriptorP qdp;

    struct sCP16Time2a elapsedTime;

    struct sCP56Time2a timestamp;
};

static bool
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_encode(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 11 : (parameters->sizeOfIOA + 11);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->event);

    Frame_setNextByte(frame, (uint8_t) self->qdp);

    Frame_appendBytes(frame, self->elapsedTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT packedStartEventsOfProtectionEquipmentWithCP56Time2aVFT = {
        (EncodeFunction) PackedStartEventsOfProtectionEquipmentWithCP56Time2a_encode,
        (DestroyFunction) PackedStartEventsOfProtectionEquipmentWithCP56Time2a_destroy
};

static void
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_initialize(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    self->virtualFunctionTable = &(packedStartEventsOfProtectionEquipmentWithCP56Time2aVFT);
    self->type = M_EP_TE_1;
}

void
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_destroy(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

PackedStartEventsOfProtectionEquipmentWithCP56Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self, int ioa,
        StartEvent event, QualityDescriptorP qdp, CP16Time2a elapsedTime, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (PackedStartEventsOfProtectionEquipmentWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sPackedStartEventsOfProtectionEquipmentWithCP56Time2a));

    if (self != NULL)
        PackedStartEventsOfProtectionEquipmentWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->event = event;
    self->qdp = qdp;
    self->elapsedTime = *elapsedTime;
    self->timestamp = *timestamp;

    return self;
}

StartEvent
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    return self->event;
}

QualityDescriptorP
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    return self->qdp;
}

CP16Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    return &(self->elapsedTime);
}

CP56Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self)
{
    return &(self->timestamp);
}

PackedStartEventsOfProtectionEquipmentWithCP56Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 7))
        return NULL;

    if (self == NULL) {
        self = (PackedStartEventsOfProtectionEquipmentWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sPackedStartEventsOfProtectionEquipmentWithCP56Time2a));

        if (self != NULL)
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* event */
        self->event = msg[startIndex++];

        /* qdp */
        self->qdp = msg[startIndex++];

        /* elapsed time */
        CP16Time2a_getFromBuffer(&(self->elapsedTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/***********************************************************************
 * PacketOutputCircuitInfo : InformationObject
 ***********************************************************************/

struct sPackedOutputCircuitInfo {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    OutputCircuitInfo oci;

    QualityDescriptorP qdp;

    struct sCP16Time2a operatingTime;

    struct sCP24Time2a timestamp;
};

static bool
PacketOutputCircuitInfo_encode(PackedOutputCircuitInfo self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 7 : (parameters->sizeOfIOA + 7);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->oci);

    Frame_setNextByte(frame, (uint8_t) self->qdp);

    Frame_appendBytes(frame, self->operatingTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);

    return true;
}

struct sInformationObjectVFT packedOutputCircuitInfoVFT = {
        (EncodeFunction) PacketOutputCircuitInfo_encode,
        (DestroyFunction) PackedOutputCircuitInfo_destroy
};

static void
PacketOutputCircuitInfo_initialize(PackedOutputCircuitInfo self)
{
    self->virtualFunctionTable = &(packedOutputCircuitInfoVFT);
    self->type = M_EP_TC_1;
}

void
PackedOutputCircuitInfo_destroy(PackedOutputCircuitInfo self)
{
    GLOBAL_FREEMEM(self);
}

PackedOutputCircuitInfo
PackedOutputCircuitInfo_create(PackedOutputCircuitInfo self, int ioa,
        OutputCircuitInfo oci, QualityDescriptorP qdp, CP16Time2a operatingTime, CP24Time2a timestamp)
{
    if (self == NULL)
        self = (PackedOutputCircuitInfo) GLOBAL_CALLOC(1, sizeof(struct sPackedOutputCircuitInfo));

    if (self != NULL)
        PacketOutputCircuitInfo_initialize(self);

    self->objectAddress = ioa;
    self->oci = oci;
    self->qdp = qdp;
    self->operatingTime = *operatingTime;
    self->timestamp = *timestamp;

    return self;
}

OutputCircuitInfo
PackedOutputCircuitInfo_getOCI(PackedOutputCircuitInfo self)
{
    return self->oci;
}

QualityDescriptorP
PackedOutputCircuitInfo_getQuality(PackedOutputCircuitInfo self)
{
    return self->qdp;
}

CP16Time2a
PackedOutputCircuitInfo_getOperatingTime(PackedOutputCircuitInfo self)
{
    return &(self->operatingTime);
}

CP24Time2a
PackedOutputCircuitInfo_getTimestamp(PackedOutputCircuitInfo self)
{
    return &(self->timestamp);
}

PackedOutputCircuitInfo
PackedOutputCircuitInfo_getFromBuffer(PackedOutputCircuitInfo self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 7))
        return NULL;

    if (self == NULL) {
        self = (PackedOutputCircuitInfo) GLOBAL_MALLOC(sizeof(struct sPackedOutputCircuitInfo));

        if (self != NULL)
            PacketOutputCircuitInfo_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* OCI - output circuit information */
        self->oci = msg[startIndex++];

        /* qdp */
        self->qdp = msg[startIndex++];

        /* operating time */
        CP16Time2a_getFromBuffer(&(self->operatingTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP24Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/***********************************************************************
 * PackedOutputCircuitInfoWithCP56Time2a : InformationObject
 ***********************************************************************/

struct sPackedOutputCircuitInfoWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    OutputCircuitInfo oci;

    QualityDescriptorP qdp;

    struct sCP16Time2a operatingTime;

    struct sCP56Time2a timestamp;
};

static bool
PackedOutputCircuitInfoWithCP56Time2a_encode(PackedOutputCircuitInfoWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 11 : (parameters->sizeOfIOA + 11);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, (uint8_t) self->oci);

    Frame_setNextByte(frame, (uint8_t) self->qdp);

    Frame_appendBytes(frame, self->operatingTime.encodedValue, 2);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT packedOutputCircuitInfoWithCP56Time2aVFT = {
        (EncodeFunction) PackedOutputCircuitInfoWithCP56Time2a_encode,
        (DestroyFunction) PackedOutputCircuitInfoWithCP56Time2a_destroy
};

static void
PackedOutputCircuitInfoWithCP56Time2a_initialize(PackedOutputCircuitInfoWithCP56Time2a self)
{
    self->virtualFunctionTable = &(packedOutputCircuitInfoWithCP56Time2aVFT);
    self->type = M_EP_TF_1;
}

void
PackedOutputCircuitInfoWithCP56Time2a_destroy(PackedOutputCircuitInfoWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

PackedOutputCircuitInfoWithCP56Time2a
PackedOutputCircuitInfoWithCP56Time2a_create(PackedOutputCircuitInfoWithCP56Time2a self, int ioa,
        OutputCircuitInfo oci, QualityDescriptorP qdp, CP16Time2a operatingTime, CP56Time2a timestamp)
{
    if (self == NULL)
        self = (PackedOutputCircuitInfoWithCP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sPackedOutputCircuitInfoWithCP56Time2a));

    if (self != NULL)
        PackedOutputCircuitInfoWithCP56Time2a_initialize(self);

    self->objectAddress = ioa;
    self->oci = oci;
    self->qdp = qdp;
    self->operatingTime = *operatingTime;
    self->timestamp = *timestamp;

    return self;
}

OutputCircuitInfo
PackedOutputCircuitInfoWithCP56Time2a_getOCI(PackedOutputCircuitInfoWithCP56Time2a self)
{
    return self->oci;
}

QualityDescriptorP
PackedOutputCircuitInfoWithCP56Time2a_getQuality(PackedOutputCircuitInfoWithCP56Time2a self)
{
    return self->qdp;
}

CP16Time2a
PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(PackedOutputCircuitInfoWithCP56Time2a self)
{
    return &(self->operatingTime);
}

CP56Time2a
PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(PackedOutputCircuitInfoWithCP56Time2a self)
{
    return &(self->timestamp);
}

PackedOutputCircuitInfoWithCP56Time2a
PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer(PackedOutputCircuitInfoWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 7))
        return NULL;

    if (self == NULL) {
        self = (PackedOutputCircuitInfoWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sPackedOutputCircuitInfoWithCP56Time2a));

        if (self != NULL)
            PackedOutputCircuitInfoWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* OCI - output circuit information */
        self->oci = msg[startIndex++];

        /* qdp */
        self->qdp = msg[startIndex++];

        /* operating time */
        CP16Time2a_getFromBuffer(&(self->operatingTime), msg, msgSize, startIndex);
        startIndex += 2;

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/***********************************************************************
 * PackedSinglePointWithSCD : InformationObject
 ***********************************************************************/

struct sPackedSinglePointWithSCD {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tStatusAndStatusChangeDetection scd;

    QualityDescriptor qds;
};

static bool
PackedSinglePointWithSCD_encode(PackedSinglePointWithSCD self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->scd.encodedValue, 4);

    Frame_setNextByte(frame, (uint8_t) self->qds);

    return true;
}

struct sInformationObjectVFT packedSinglePointWithSCDVFT = {
        (EncodeFunction) PackedSinglePointWithSCD_encode,
        (DestroyFunction) PackedSinglePointWithSCD_destroy
};

static void
PackedSinglePointWithSCD_initialize(PackedSinglePointWithSCD self)
{
    self->virtualFunctionTable = &(packedSinglePointWithSCDVFT);
    self->type = M_PS_NA_1;
}

void
PackedSinglePointWithSCD_destroy(PackedSinglePointWithSCD self)
{
    GLOBAL_FREEMEM(self);
}

PackedSinglePointWithSCD
PackedSinglePointWithSCD_create(PackedSinglePointWithSCD self, int ioa,
        StatusAndStatusChangeDetection scd, QualityDescriptor qds)
{
    if (self == NULL)
        self = (PackedSinglePointWithSCD) GLOBAL_CALLOC(1, sizeof(struct sPackedSinglePointWithSCD));

    if (self != NULL)
        PackedSinglePointWithSCD_initialize(self);

    self->objectAddress = ioa;
    self->scd = *scd;
    self->qds = qds;

    return self;
}

QualityDescriptor
PackedSinglePointWithSCD_getQuality(PackedSinglePointWithSCD self)
{
    return self->qds;
}

StatusAndStatusChangeDetection
PackedSinglePointWithSCD_getSCD(PackedSinglePointWithSCD self)
{
    return &(self->scd);
}

PackedSinglePointWithSCD
PackedSinglePointWithSCD_getFromBuffer(PackedSinglePointWithSCD self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 5))
        return NULL;

    if (self == NULL) {
        self = (PackedSinglePointWithSCD) GLOBAL_MALLOC(sizeof(struct sPackedSinglePointWithSCD));

        if (self != NULL)
            PackedSinglePointWithSCD_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        /* SCD */
        self->scd.encodedValue[0] = msg[startIndex++];
        self->scd.encodedValue[1] = msg[startIndex++];
        self->scd.encodedValue[2] = msg[startIndex++];
        self->scd.encodedValue[3] = msg[startIndex++];

        /* QDS */
        self->qds = msg[startIndex];
    }

    return self;
}


/*******************************************
 * SingleCommand
 *******************************************/

struct sSingleCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;
};

static bool
SingleCommand_encode(SingleCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->sco);

    return true;
}

struct sInformationObjectVFT singleCommandVFT = {
        (EncodeFunction) SingleCommand_encode,
        (DestroyFunction) SingleCommand_destroy
};

static void
SingleCommand_initialize(SingleCommand self)
{
    self->virtualFunctionTable = &(singleCommandVFT);
    self->type = C_SC_NA_1;
}

void
SingleCommand_destroy(SingleCommand self)
{
    GLOBAL_FREEMEM(self);
}

SingleCommand
SingleCommand_create(SingleCommand self, int ioa, bool command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (SingleCommand) GLOBAL_MALLOC(sizeof(struct sSingleCommand));

        if (self == NULL)
            return NULL;
        else
            SingleCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t sco = ((qu & 0x1f) * 4);

    if (command) sco |= 0x01;

    if (selectCommand) sco |= 0x80;

    self->sco = sco;

    return self;
}

int
SingleCommand_getQU(SingleCommand self)
{
    return ((self->sco & 0x7c) / 4);
}

bool
SingleCommand_getState(SingleCommand self)
{
    return ((self->sco & 0x01) == 0x01);
}

bool
SingleCommand_isSelect(SingleCommand self)
{
    return ((self->sco & 0x80) == 0x80);
}

SingleCommand
SingleCommand_getFromBuffer(SingleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (SingleCommand) GLOBAL_MALLOC(sizeof(struct sSingleCommand));

        if (self != NULL)
            SingleCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->sco = msg[startIndex];
    }

    return self;
}


/***********************************************************************
 * SingleCommandWithCP56Time2a : SingleCommand
 ***********************************************************************/

struct sSingleCommandWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;

    struct sCP56Time2a timestamp;
};

static bool
SingleCommandWithCP56Time2a_encode(SingleCommandWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    SingleCommand_encode((SingleCommand) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT singleCommandWithCP56Time2aVFT = {
        (EncodeFunction) SingleCommandWithCP56Time2a_encode,
        (DestroyFunction) SingleCommandWithCP56Time2a_destroy
};

static void
SingleCommandWithCP56Time2a_initialize(SingleCommandWithCP56Time2a self)
{
    self->virtualFunctionTable = &(singleCommandWithCP56Time2aVFT);
    self->type = C_SC_TA_1;
}

void
SingleCommandWithCP56Time2a_destroy(SingleCommandWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_create(SingleCommandWithCP56Time2a self, int ioa, bool command, bool selectCommand, int qu, CP56Time2a timestamp)
{
    if (self == NULL) {
		self = (SingleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSingleCommandWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            SingleCommandWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t sco = ((qu & 0x1f) * 4);

    if (command) sco |= 0x01;

    if (selectCommand) sco |= 0x80;

    self->sco = sco;

    self->timestamp = *timestamp;

    return self;
}

CP56Time2a
SingleCommandWithCP56Time2a_getTimestamp(SingleCommandWithCP56Time2a self)
{
    return &(self->timestamp);
}

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_getFromBuffer(SingleCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (SingleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSingleCommandWithCP56Time2a));

        if (self != NULL)
            SingleCommandWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->sco = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * DoubleCommand : InformationObject
 *******************************************/

struct sDoubleCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

static bool
DoubleCommand_encode(DoubleCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->dcq);

    return true;
}

struct sInformationObjectVFT doubleCommandVFT = {
        (EncodeFunction) DoubleCommand_encode,
        (DestroyFunction) DoubleCommand_destroy
};

static void
DoubleCommand_initialize(DoubleCommand self)
{
    self->virtualFunctionTable = &(doubleCommandVFT);
    self->type = C_DC_NA_1;
}

void
DoubleCommand_destroy(DoubleCommand self)
{
    GLOBAL_FREEMEM(self);
}

DoubleCommand
DoubleCommand_create(DoubleCommand self, int ioa, int command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (DoubleCommand) GLOBAL_MALLOC(sizeof(struct sDoubleCommand));

        if (self == NULL)
            return NULL;
        else
            DoubleCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    return self;
}

int
DoubleCommand_getQU(DoubleCommand self)
{
    return ((self->dcq & 0x7c) / 4);
}

int
DoubleCommand_getState(DoubleCommand self)
{
    return (self->dcq & 0x03);
}

bool
DoubleCommand_isSelect(DoubleCommand self)
{
    return ((self->dcq & 0x80) == 0x80);
}

DoubleCommand
DoubleCommand_getFromBuffer(DoubleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (DoubleCommand) GLOBAL_MALLOC(sizeof(struct sDoubleCommand));

        if (self != NULL)
            DoubleCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->dcq = msg[startIndex];
    }

    return self;
}

/**********************************************
 * DoubleCommandWithCP56Time2a : DoubleCommand
 **********************************************/

struct sDoubleCommandWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;

    struct sCP56Time2a timestamp;
};

static bool
DoubleCommandWithCP56Time2a_encode(DoubleCommandWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    DoubleCommand_encode((DoubleCommand) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT doubleCommandWithCP56Time2aVFT = {
        (EncodeFunction) DoubleCommandWithCP56Time2a_encode,
        (DestroyFunction) DoubleCommandWithCP56Time2a_destroy
};

static void
DoubleCommandWithCP56Time2a_initialize(DoubleCommandWithCP56Time2a self)
{
    self->virtualFunctionTable = &(doubleCommandWithCP56Time2aVFT);
    self->type = C_DC_TA_1;
}

void
DoubleCommandWithCP56Time2a_destroy(DoubleCommandWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

DoubleCommandWithCP56Time2a
DoubleCommandWithCP56Time2a_create(DoubleCommandWithCP56Time2a self, int ioa, int command, bool selectCommand, int qu, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (DoubleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sDoubleCommandWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            DoubleCommandWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    self->timestamp = *timestamp;

    return self;
}

int
DoubleCommandWithCP56Time2a_getQU(DoubleCommandWithCP56Time2a self)
{
    return DoubleCommand_getQU((DoubleCommand) self);
}

int
DoubleCommandWithCP56Time2a_getState(DoubleCommandWithCP56Time2a self)
{
    return DoubleCommand_getState((DoubleCommand) self);
}

bool
DoubleCommandWithCP56Time2a_isSelect(DoubleCommandWithCP56Time2a self)
{
    return DoubleCommand_isSelect((DoubleCommand) self);
}

DoubleCommandWithCP56Time2a
DoubleCommandWithCP56Time2a_getFromBuffer(DoubleCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
        self = (DoubleCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sDoubleCommandWithCP56Time2a));

        if (self != NULL)
            DoubleCommandWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* DCQ */
        self->dcq = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * StepCommand : InformationObject
 *******************************************/

struct sStepCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

static bool
StepCommand_encode(StepCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->dcq);

    return true;
}

struct sInformationObjectVFT stepCommandVFT = {
        (EncodeFunction) StepCommand_encode,
        (DestroyFunction) StepCommand_destroy
};

static void
StepCommand_initialize(StepCommand self)
{
    self->virtualFunctionTable = &(stepCommandVFT);
    self->type = C_RC_NA_1;
}

void
StepCommand_destroy(StepCommand self)
{
    GLOBAL_FREEMEM(self);
}

StepCommand
StepCommand_create(StepCommand self, int ioa, StepCommandValue command, bool selectCommand, int qu)
{
    if (self == NULL) {
		self = (StepCommand) GLOBAL_MALLOC(sizeof(struct sStepCommand));

        if (self == NULL)
            return NULL;
        else
            StepCommand_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    return self;
}

int
StepCommand_getQU(StepCommand self)
{
    return ((self->dcq & 0x7c) / 4);
}

StepCommandValue
StepCommand_getState(StepCommand self)
{
    return (StepCommandValue) (self->dcq & 0x03);
}

bool
StepCommand_isSelect(StepCommand self)
{
    return ((self->dcq & 0x80) == 0x80);
}

StepCommand
StepCommand_getFromBuffer(StepCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 1))
        return NULL;

    if (self == NULL) {
		self = (StepCommand) GLOBAL_MALLOC(sizeof(struct sStepCommand));

        if (self != NULL)
            StepCommand_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->dcq = msg[startIndex];
    }

    return self;
}


/*************************************************
 * StepCommandWithCP56Time2a : InformationObject
 *************************************************/

struct sStepCommandWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;

    struct sCP56Time2a timestamp;
};

static bool
StepCommandWithCP56Time2a_encode(StepCommandWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 8 : (parameters->sizeOfIOA + 8);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    StepCommand_encode((StepCommand) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT stepCommandWithCP56Time2aVFT = {
        (EncodeFunction) StepCommandWithCP56Time2a_encode,
        (DestroyFunction) StepCommandWithCP56Time2a_destroy
};

static void
StepCommandWithCP56Time2a_initialize(StepCommandWithCP56Time2a self)
{
    self->virtualFunctionTable = &(stepCommandWithCP56Time2aVFT);
    self->type = C_RC_TA_1;
}

void
StepCommandWithCP56Time2a_destroy(StepCommand self)
{
    GLOBAL_FREEMEM(self);
}

StepCommandWithCP56Time2a
StepCommandWithCP56Time2a_create(StepCommandWithCP56Time2a self, int ioa, StepCommandValue command, bool selectCommand, int qu, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (StepCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sStepCommandWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            StepCommandWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    uint8_t dcq = ((qu & 0x1f) * 4);

    dcq += (uint8_t) (command & 0x03);

    if (selectCommand) dcq |= 0x80;

    self->dcq = dcq;

    self->timestamp = *timestamp;

    return self;
}

int
StepCommandWithCP56Time2a_getQU(StepCommandWithCP56Time2a self)
{
    return StepCommand_getQU((StepCommand) self);
}

StepCommandValue
StepCommandWithCP56Time2a_getState(StepCommandWithCP56Time2a self)
{
    return StepCommand_getState((StepCommand) self);
}

bool
StepCommandWithCP56Time2a_isSelect(StepCommandWithCP56Time2a self)
{
    return StepCommand_isSelect((StepCommand) self);
}

StepCommandWithCP56Time2a
StepCommandWithCP56Time2a_getFromBuffer(StepCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 8))
        return NULL;

    if (self == NULL) {
        self = (StepCommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sStepCommandWithCP56Time2a));

        if (self != NULL)
            StepCommandWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* SCO */
        self->dcq = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*************************************************
 * SetpointCommandNormalized : InformationObject
 ************************************************/

struct sSetpointCommandNormalized {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

static bool
SetpointCommandNormalized_encode(SetpointCommandNormalized self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 3 : (parameters->sizeOfIOA + 3);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->encodedValue, 2);
    Frame_setNextByte(frame, self->qos);

    return true;
}

struct sInformationObjectVFT setpointCommandNormalizedVFT = {
        (EncodeFunction) SetpointCommandNormalized_encode,
        (DestroyFunction) SetpointCommandNormalized_destroy
};

static void
SetpointCommandNormalized_initialize(SetpointCommandNormalized self)
{
    self->virtualFunctionTable = &(setpointCommandNormalizedVFT);
    self->type = C_SE_NA_1;
}

void
SetpointCommandNormalized_destroy(SetpointCommandNormalized self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandNormalized
SetpointCommandNormalized_create(SetpointCommandNormalized self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandNormalized) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalized));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandNormalized_initialize(self);
    }

    self->objectAddress = ioa;

    int scaledValue = (int)((value * 32767.5) - 0.5);

    setScaledValue(self->encodedValue, scaledValue);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

float
SetpointCommandNormalized_getValue(SetpointCommandNormalized self)
{
    float nv = ((float) getScaledValue(self->encodedValue) + 0.5) / 32767.5;

    return nv;
}

int
SetPointCommandNormalized_getQL(SetpointCommandNormalized self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandNormalized_isSelect(SetpointCommandNormalized self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandNormalized
SetpointCommandNormalized_getFromBuffer(SetpointCommandNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 3))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandNormalized) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalized));

        if (self != NULL)
            SetpointCommandNormalized_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}

/**********************************************************************
 * SetpointCommandNormalizedWithCP56Time2a : SetpointCommandNormalized
 **********************************************************************/

struct sSetpointCommandNormalizedWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

static bool
SetpointCommandNormalizedWithCP56Time2a_encode(SetpointCommandNormalizedWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 10 : (parameters->sizeOfIOA + 10);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    SetpointCommandNormalized_encode((SetpointCommandNormalized) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT setpointCommandNormalizedWithCP56Time2aVFT = {
        (EncodeFunction) SetpointCommandNormalizedWithCP56Time2a_encode,
        (DestroyFunction) SetpointCommandNormalizedWithCP56Time2a_destroy
};

static void
SetpointCommandNormalizedWithCP56Time2a_initialize(SetpointCommandNormalizedWithCP56Time2a self)
{
    self->virtualFunctionTable = &(setpointCommandNormalizedWithCP56Time2aVFT);
    self->type = C_SE_TA_1;
}

void
SetpointCommandNormalizedWithCP56Time2a_destroy(SetpointCommandNormalizedWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandNormalizedWithCP56Time2a
SetpointCommandNormalizedWithCP56Time2a_create(SetpointCommandNormalizedWithCP56Time2a self, int ioa, float value, bool selectCommand, int ql, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (SetpointCommandNormalizedWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalizedWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandNormalizedWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    self->timestamp = *timestamp;

    return self;
}

float
SetpointCommandNormalizedWithCP56Time2a_getValue(SetpointCommandNormalizedWithCP56Time2a self)
{
    return SetpointCommandNormalized_getValue((SetpointCommandNormalized) self);
}

int
SetpointCommandNormalizedWithCP56Time2a_getQL(SetpointCommandNormalizedWithCP56Time2a self)
{
    return SetPointCommandNormalized_getQL((SetpointCommandNormalized) self);
}

bool
SetpointCommandNormalizedWithCP56Time2a_isSelect(SetpointCommandNormalizedWithCP56Time2a self)
{
    return SetpointCommandNormalized_isSelect((SetpointCommandNormalized) self);
}

SetpointCommandNormalizedWithCP56Time2a
SetpointCommandNormalizedWithCP56Time2a_getFromBuffer(SetpointCommandNormalizedWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 10))
        return NULL;

    if (self == NULL) {
        self = (SetpointCommandNormalizedWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandNormalizedWithCP56Time2a));

        if (self != NULL)
            SetpointCommandNormalizedWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*************************************************
 * SetpointCommandScaled: InformationObject
 ************************************************/

struct sSetpointCommandScaled {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

static bool
SetpointCommandScaled_encode(SetpointCommandScaled self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 3 : (parameters->sizeOfIOA + 3);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->encodedValue, 2);
    Frame_setNextByte(frame, self->qos);

    return true;
}

struct sInformationObjectVFT setpointCommandScaledVFT = {
        (EncodeFunction) SetpointCommandScaled_encode,
        (DestroyFunction) SetpointCommandScaled_destroy
};

static void
SetpointCommandScaled_initialize(SetpointCommandScaled self)
{
    self->virtualFunctionTable = &(setpointCommandScaledVFT);
    self->type = C_SE_NB_1;
}

void
SetpointCommandScaled_destroy(SetpointCommandScaled self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandScaled
SetpointCommandScaled_create(SetpointCommandScaled self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandScaled) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaled));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandScaled_initialize(self);
    }

    self->objectAddress = ioa;

    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, scaledValue);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

int
SetpointCommandScaled_getValue(SetpointCommandScaled self)
{
    return getScaledValue(self->encodedValue);
}

int
SetpointCommandScaled_getQL(SetpointCommandScaled self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandScaled_isSelect(SetpointCommandScaled self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandScaled
SetpointCommandScaled_getFromBuffer(SetpointCommandScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 3))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandScaled) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaled));

        if (self != NULL)
            SetpointCommandScaled_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}

/**********************************************************************
 * SetpointCommandScaledWithCP56Time2a : SetpointCommandScaled
 **********************************************************************/

struct sSetpointCommandScaledWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

static bool
SetpointCommandScaledWithCP56Time2a_encode(SetpointCommandScaledWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 10 : (parameters->sizeOfIOA + 10);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    SetpointCommandScaled_encode((SetpointCommandScaled) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT setpointCommandScaledWithCP56Time2aVFT = {
        (EncodeFunction) SetpointCommandScaledWithCP56Time2a_encode,
        (DestroyFunction) SetpointCommandScaledWithCP56Time2a_destroy
};

static void
SetpointCommandScaledWithCP56Time2a_initialize(SetpointCommandScaledWithCP56Time2a self)
{
    self->virtualFunctionTable = &(setpointCommandScaledWithCP56Time2aVFT);
    self->type = C_SE_TB_1;
}

void
SetpointCommandScaledWithCP56Time2a_destroy(SetpointCommandScaledWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandScaledWithCP56Time2a
SetpointCommandScaledWithCP56Time2a_create(SetpointCommandScaledWithCP56Time2a self, int ioa, int value, bool selectCommand, int ql, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (SetpointCommandScaledWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaledWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandScaledWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    setScaledValue(self->encodedValue, value);

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    self->timestamp = *timestamp;

    return self;
}

int
SetpointCommandScaledWithCP56Time2a_getValue(SetpointCommandScaledWithCP56Time2a self)
{
    return SetpointCommandScaled_getValue((SetpointCommandScaled) self);
}

int
SetpointCommandScaledWithCP56Time2a_getQL(SetpointCommandScaledWithCP56Time2a self)
{
    return SetpointCommandScaled_getQL((SetpointCommandScaled) self);
}

bool
SetpointCommandScaledWithCP56Time2a_isSelect(SetpointCommandScaledWithCP56Time2a self)
{
    return SetpointCommandScaled_isSelect((SetpointCommandScaled) self);
}

SetpointCommandScaledWithCP56Time2a
SetpointCommandScaledWithCP56Time2a_getFromBuffer(SetpointCommandScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 10))
        return NULL;

    if (self == NULL) {
        self = (SetpointCommandScaledWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandScaledWithCP56Time2a));

        if (self != NULL)
            SetpointCommandScaledWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg[startIndex++];
        self->encodedValue[1] = msg[startIndex++];

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*************************************************
 * SetpointCommandShort: InformationObject
 ************************************************/

struct sSetpointCommandShort {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    uint8_t qos; /* Qualifier of setpoint command */
};

static bool
SetpointCommandShort_encode(SetpointCommandShort self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif

    Frame_setNextByte(frame, self->qos);

    return true;
}

struct sInformationObjectVFT setpointCommandShortVFT = {
        (EncodeFunction) SetpointCommandShort_encode,
        (DestroyFunction) SetpointCommandShort_destroy
};

static void
SetpointCommandShort_initialize(SetpointCommandShort self)
{
    self->virtualFunctionTable = &(setpointCommandShortVFT);
    self->type = C_SE_NC_1;
}

void
SetpointCommandShort_destroy(SetpointCommandShort self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandShort
SetpointCommandShort_create(SetpointCommandShort self, int ioa, float value, bool selectCommand, int ql)
{
    if (self == NULL) {
		self = (SetpointCommandShort) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShort));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandShort_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    uint8_t qos = ql & 0x7f;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    return self;
}

float
SetpointCommandShort_getValue(SetpointCommandShort self)
{
    return self->value;
}

int
SetpointCommandShort_getQL(SetpointCommandShort self)
{
    return (int) (self->qos & 0x7f);
}

bool
SetpointCommandShort_isSelect(SetpointCommandShort self)
{
    return ((self->qos & 0x80) == 0x80);
}

SetpointCommandShort
SetpointCommandShort_getFromBuffer(SetpointCommandShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 5))
        return NULL;

    if (self == NULL) {
		self = (SetpointCommandShort) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShort));

        if (self != NULL)
            SetpointCommandShort_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif


        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex];
    }

    return self;
}


/**********************************************************************
 * SetpointCommandShortWithCP56Time2a : SetpointCommandShort
 **********************************************************************/

struct sSetpointCommandShortWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

static bool
SetpointCommandShortWithCP56Time2a_encode(SetpointCommandShortWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 12 : (parameters->sizeOfIOA + 12);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    SetpointCommandShort_encode((SetpointCommandShort) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT setpointCommandShortWithCP56Time2aVFT = {
        (EncodeFunction) SetpointCommandShortWithCP56Time2a_encode,
        (DestroyFunction) SetpointCommandShortWithCP56Time2a_destroy
};

static void
SetpointCommandShortWithCP56Time2a_initialize(SetpointCommandShortWithCP56Time2a self)
{
    self->virtualFunctionTable = &(setpointCommandShortWithCP56Time2aVFT);
    self->type = C_SE_TC_1;
}

void
SetpointCommandShortWithCP56Time2a_destroy(SetpointCommandShortWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

SetpointCommandShortWithCP56Time2a
SetpointCommandShortWithCP56Time2a_create(SetpointCommandShortWithCP56Time2a self, int ioa, float value, bool selectCommand, int ql, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (SetpointCommandShortWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShortWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            SetpointCommandShortWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    uint8_t qos = ql;

    if (selectCommand) qos |= 0x80;

    self->qos = qos;

    self->timestamp = *timestamp;

    return self;
}

float
SetpointCommandShortWithCP56Time2a_getValue(SetpointCommandShortWithCP56Time2a self)
{
    return SetpointCommandShort_getValue((SetpointCommandShort) self);
}

int
SetpointCommandShortWithCP56Time2a_getQL(SetpointCommandShortWithCP56Time2a self)
{
    return SetpointCommandShort_getQL((SetpointCommandShort) self);
}

bool
SetpointCommandShortWithCP56Time2a_isSelect(SetpointCommandShortWithCP56Time2a self)
{
    return SetpointCommandShort_isSelect((SetpointCommandShort) self);
}

SetpointCommandShortWithCP56Time2a
SetpointCommandShortWithCP56Time2a_getFromBuffer(SetpointCommandShortWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 10))
        return NULL;

    if (self == NULL) {
        self = (SetpointCommandShortWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sSetpointCommandShortWithCP56Time2a));

        if (self != NULL)
            SetpointCommandShortWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* QOS - qualifier of setpoint command */
        self->qos = msg[startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*************************************************
 * Bitstring32Command : InformationObject
 ************************************************/

struct sBitstring32Command {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
};

static bool
Bitstring32Command_encode(Bitstring32Command self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 5 : (parameters->sizeOfIOA + 5);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
    Frame_appendBytes(frame, valueBytes, 4);
#else
    Frame_setNextByte(frame, valueBytes[3]);
    Frame_setNextByte(frame, valueBytes[2]);
    Frame_setNextByte(frame, valueBytes[1]);
    Frame_setNextByte(frame, valueBytes[0]);
#endif

    return true;
}

struct sInformationObjectVFT bitstring32CommandVFT = {
        (EncodeFunction) Bitstring32Command_encode,
        (DestroyFunction) Bitstring32Command_destroy
};

static void
Bitstring32Command_initialize(Bitstring32Command self)
{
    self->virtualFunctionTable = &(bitstring32CommandVFT);
    self->type = C_BO_NA_1;
}

Bitstring32Command
Bitstring32Command_create(Bitstring32Command self, int ioa, uint32_t value)
{
    if (self == NULL) {
		self = (Bitstring32Command) GLOBAL_MALLOC(sizeof(struct sBitstring32Command));

        if (self == NULL)
            return NULL;
        else
            Bitstring32Command_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    return self;
}

void
Bitstring32Command_destroy(Bitstring32Command self)
{
    GLOBAL_FREEMEM(self);
}

uint32_t
Bitstring32Command_getValue(Bitstring32Command self)
{
    return self->value;
}

Bitstring32Command
Bitstring32Command_getFromBuffer(Bitstring32Command self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 4))
        return NULL;

    if (self == NULL) {
		self = (Bitstring32Command) GLOBAL_MALLOC(sizeof(struct sBitstring32Command));

        if (self != NULL)
            Bitstring32Command_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif
    }

    return self;
}


/*******************************************************
 * Bitstring32CommandWithCP56Time2a: Bitstring32Command
 *******************************************************/

struct sBitstring32CommandWithCP56Time2a {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;

    struct sCP56Time2a timestamp;
};

static bool
Bitstring32CommandWithCP56Time2a_encode(Bitstring32CommandWithCP56Time2a self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 12 : (parameters->sizeOfIOA + 12);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    Bitstring32Command_encode((Bitstring32Command) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT bitstring32CommandWithCP56Time2aVFT = {
        (EncodeFunction) Bitstring32CommandWithCP56Time2a_encode,
        (DestroyFunction) Bitstring32CommandWithCP56Time2a_destroy
};

static void
Bitstring32CommandWithCP56Time2a_initialize(Bitstring32CommandWithCP56Time2a self)
{
    self->virtualFunctionTable = &(bitstring32CommandWithCP56Time2aVFT);
    self->type = C_BO_TA_1;
}

Bitstring32CommandWithCP56Time2a
Bitstring32CommandWithCP56Time2a_create(Bitstring32CommandWithCP56Time2a self, int ioa, uint32_t value, CP56Time2a timestamp)
{
    if (self == NULL) {
        self = (Bitstring32CommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sBitstring32CommandWithCP56Time2a));

        if (self == NULL)
            return NULL;
        else
            Bitstring32CommandWithCP56Time2a_initialize(self);
    }

    self->objectAddress = ioa;

    self->value = value;

    self->timestamp = *timestamp;

    return self;
}

void
Bitstring32CommandWithCP56Time2a_destroy(Bitstring32CommandWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

uint32_t
Bitstring32CommandWithCP56Time2a_getValue(Bitstring32CommandWithCP56Time2a self)
{
    return self->value;
}

CP56Time2a
Bitstring32CommandWithCP56Time2a_getTimestamp(Bitstring32CommandWithCP56Time2a self)
{
    return &(self->timestamp);
}

Bitstring32CommandWithCP56Time2a
Bitstring32CommandWithCP56Time2a_getFromBuffer(Bitstring32CommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA + 11))
        return NULL;

    if (self == NULL) {
        self = (Bitstring32CommandWithCP56Time2a) GLOBAL_MALLOC(sizeof(struct sBitstring32CommandWithCP56Time2a));

        if (self != NULL)
            Bitstring32CommandWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint8_t* valueBytes = (uint8_t*) &(self->value);

#if (ORDER_LITTLE_ENDIAN == 1)
        valueBytes[0] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[3] = msg [startIndex++];
#else
        valueBytes[3] = msg [startIndex++];
        valueBytes[2] = msg [startIndex++];
        valueBytes[1] = msg [startIndex++];
        valueBytes[0] = msg [startIndex++];
#endif

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}


/*************************************************
 * ReadCommand : InformationObject
 ************************************************/

struct sReadCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;
};

static bool
ReadCommand_encode(ReadCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 0 : (parameters->sizeOfIOA + 0);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    return true;
}

struct sInformationObjectVFT readCommandVFT = {
        (EncodeFunction) ReadCommand_encode,
        (DestroyFunction) ReadCommand_destroy
};

static void
ReadCommand_initialize(ReadCommand self)
{
    self->virtualFunctionTable = &(readCommandVFT);
    self->type = C_RD_NA_1;
}

ReadCommand
ReadCommand_create(ReadCommand self, int ioa)
{
    if (self == NULL) {
		self = (ReadCommand) GLOBAL_MALLOC(sizeof(struct sReadCommand));

        if (self == NULL)
            return NULL;
        else
            ReadCommand_initialize(self);
    }

    self->objectAddress = ioa;

    return self;
}

void
ReadCommand_destroy(ReadCommand self)
{
    GLOBAL_FREEMEM(self);
}


ReadCommand
ReadCommand_getFromBuffer(ReadCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA))
        return NULL;

    if (self == NULL) {
		self = (ReadCommand) GLOBAL_MALLOC(sizeof(struct sReadCommand));

        if (self != NULL)
            ReadCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);
    }

    return self;
}

/***************************************************
 * ClockSynchronizationCommand : InformationObject
 **************************************************/

struct sClockSynchronizationCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sCP56Time2a timestamp;
};

static bool
ClockSynchronizationCommand_encode(ClockSynchronizationCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 7 : (parameters->sizeOfIOA + 7);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT clockSynchronizationCommandVFT = {
        (EncodeFunction) ClockSynchronizationCommand_encode,
        (DestroyFunction) ClockSynchronizationCommand_destroy
};

static void
ClockSynchronizationCommand_initialize(ClockSynchronizationCommand self)
{
    self->virtualFunctionTable = &(clockSynchronizationCommandVFT);
    self->type = C_CS_NA_1;
}

ClockSynchronizationCommand
ClockSynchronizationCommand_create(ClockSynchronizationCommand self, int ioa, CP56Time2a timestamp)
{
    if (self == NULL) {
		self = (ClockSynchronizationCommand) GLOBAL_MALLOC(sizeof(struct sClockSynchronizationCommand));

        if (self == NULL)
            return NULL;
        else
            ClockSynchronizationCommand_initialize(self);
    }

    self->objectAddress = ioa;
    self->timestamp = *timestamp;

    return self;
}

void
ClockSynchronizationCommand_destroy(ClockSynchronizationCommand self)
{
    GLOBAL_FREEMEM(self);
}

CP56Time2a
ClockSynchronizationCommand_getTime(ClockSynchronizationCommand self)
{
    return &(self->timestamp);
}

ClockSynchronizationCommand
ClockSynchronizationCommand_getFromBuffer(ClockSynchronizationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 7)
        return NULL;

    if (self == NULL) {
		self = (ClockSynchronizationCommand) GLOBAL_MALLOC(sizeof(struct sClockSynchronizationCommand));

        if (self != NULL)
            ClockSynchronizationCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*************************************************
 * InterrogationCommand : InformationObject
 ************************************************/

struct sInterrogationCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t qoi;
};

static bool
InterrogationCommand_encode(InterrogationCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->qoi);

    return true;
}

struct sInformationObjectVFT interrogationCommandVFT = {
        (EncodeFunction) InterrogationCommand_encode,
        (DestroyFunction) InterrogationCommand_destroy
};

static void
InterrogationCommand_initialize(InterrogationCommand self)
{
    self->virtualFunctionTable = &(interrogationCommandVFT);
    self->type = C_IC_NA_1;
}

InterrogationCommand
InterrogationCommand_create(InterrogationCommand self, int ioa, uint8_t qoi)
{
    if (self == NULL) {
		self = (InterrogationCommand) GLOBAL_MALLOC(sizeof(struct sInterrogationCommand));

        if (self == NULL)
            return NULL;
        else
            InterrogationCommand_initialize(self);
    }

    self->objectAddress = ioa;

    self->qoi = qoi;

    return self;
}

void
InterrogationCommand_destroy(InterrogationCommand self)
{
    GLOBAL_FREEMEM(self);
}

uint8_t
InterrogationCommand_getQOI(InterrogationCommand self)
{
    return self->qoi;
}

InterrogationCommand
InterrogationCommand_getFromBuffer(InterrogationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
		self = (InterrogationCommand) GLOBAL_MALLOC(sizeof(struct sInterrogationCommand));

        if (self != NULL)
            InterrogationCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* QUI */
        self->qoi = msg[startIndex];
    }

    return self;
}

/**************************************************
 * CounterInterrogationCommand : InformationObject
 **************************************************/

struct sCounterInterrogationCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t qcc;
};

static bool
CounterInterrogationCommand_encode(CounterInterrogationCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->qcc);

    return true;
}

struct sInformationObjectVFT counterInterrogationCommandVFT = {
        (EncodeFunction) CounterInterrogationCommand_encode,
        (DestroyFunction) CounterInterrogationCommand_destroy
};

static void
CounterInterrogationCommand_initialize(CounterInterrogationCommand self)
{
    self->virtualFunctionTable = &(counterInterrogationCommandVFT);
    self->type = C_CI_NA_1;
}

CounterInterrogationCommand
CounterInterrogationCommand_create(CounterInterrogationCommand self, int ioa, QualifierOfCIC qcc)
{
    if (self == NULL) {
        self = (CounterInterrogationCommand) GLOBAL_MALLOC(sizeof(struct sCounterInterrogationCommand));

        if (self == NULL)
            return NULL;
        else
            CounterInterrogationCommand_initialize(self);
    }

    self->objectAddress = ioa;

    self->qcc = qcc;

    return self;
}

void
CounterInterrogationCommand_destroy(CounterInterrogationCommand self)
{
    GLOBAL_FREEMEM(self);
}

QualifierOfCIC
CounterInterrogationCommand_getQCC(CounterInterrogationCommand self)
{
    return self->qcc;
}

CounterInterrogationCommand
CounterInterrogationCommand_getFromBuffer(CounterInterrogationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
        self = (CounterInterrogationCommand) GLOBAL_MALLOC(sizeof(struct sCounterInterrogationCommand));

        if (self != NULL)
            CounterInterrogationCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* QCC */
        self->qcc = msg[startIndex];
    }

    return self;
}

/*************************************************
 * ResetProcessCommand : InformationObject
 ************************************************/

struct sResetProcessCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    QualifierOfRPC qrp;
};

static bool
ResetProcessCommand_encode(ResetProcessCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->qrp);

    return true;
}

struct sInformationObjectVFT resetProcessCommandVFT = {
        (EncodeFunction) ResetProcessCommand_encode,
        (DestroyFunction) ResetProcessCommand_destroy
};

static void
ResetProcessCommand_initialize(ResetProcessCommand self)
{
    self->virtualFunctionTable = &(resetProcessCommandVFT);
    self->type = C_RP_NA_1;
}

ResetProcessCommand
ResetProcessCommand_create(ResetProcessCommand self, int ioa,  QualifierOfRPC qrp)
{
    if (self == NULL) {
        self = (ResetProcessCommand) GLOBAL_MALLOC(sizeof(struct sResetProcessCommand));

        if (self == NULL)
            return NULL;
        else
            ResetProcessCommand_initialize(self);
    }

    self->objectAddress = ioa;

    self->qrp = qrp;

    return self;
}

void
ResetProcessCommand_destroy(ResetProcessCommand self)
{
    GLOBAL_FREEMEM(self);
}

QualifierOfRPC
ResetProcessCommand_getQRP(ResetProcessCommand self)
{
    return self->qrp;
}

ResetProcessCommand
ResetProcessCommand_getFromBuffer(ResetProcessCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
        self = (ResetProcessCommand) GLOBAL_MALLOC(sizeof(struct sResetProcessCommand));

        if (self != NULL)
            ResetProcessCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* QUI */
        self->qrp = msg[startIndex];
    }

    return self;
}

/*************************************************
 * DelayAcquisitionCommand : InformationObject
 ************************************************/

struct sDelayAcquisitionCommand {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sCP16Time2a delay;
};

static bool
DelayAcquisitionCommand_encode(DelayAcquisitionCommand self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 2 : (parameters->sizeOfIOA + 2);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_appendBytes(frame, self->delay.encodedValue, 2);

    return true;
}

struct sInformationObjectVFT DelayAcquisitionCommandVFT = {
        (EncodeFunction) DelayAcquisitionCommand_encode,
        (DestroyFunction) DelayAcquisitionCommand_destroy
};

static void
DelayAcquisitionCommand_initialize(DelayAcquisitionCommand self)
{
    self->virtualFunctionTable = &(DelayAcquisitionCommandVFT);
    self->type = C_CD_NA_1;
}

DelayAcquisitionCommand
DelayAcquisitionCommand_create(DelayAcquisitionCommand self, int ioa,  CP16Time2a delay)
{
    if (self == NULL) {
        self = (DelayAcquisitionCommand) GLOBAL_MALLOC(sizeof(struct sDelayAcquisitionCommand));

        if (self == NULL)
            return NULL;
        else
            DelayAcquisitionCommand_initialize(self);
    }

    self->objectAddress = ioa;

    self->delay = *delay;

    return self;
}

void
DelayAcquisitionCommand_destroy(DelayAcquisitionCommand self)
{
    GLOBAL_FREEMEM(self);
}

CP16Time2a
DelayAcquisitionCommand_getDelay(DelayAcquisitionCommand self)
{
    return &(self->delay);
}

DelayAcquisitionCommand
DelayAcquisitionCommand_getFromBuffer(DelayAcquisitionCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
        self = (DelayAcquisitionCommand) GLOBAL_MALLOC(sizeof(struct sDelayAcquisitionCommand));

        if (self != NULL)
            DelayAcquisitionCommand_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* delay */
        CP16Time2a_getFromBuffer(&(self->delay), msg, msgSize, startIndex);
    }

    return self;
}


/*******************************************
 * ParameterActivation : InformationObject
 *******************************************/

struct sParameterActivation {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    QualifierOfParameterActivation qpa;
};

static bool
ParameterActivation_encode(ParameterActivation self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->qpa);

    return true;
}

struct sInformationObjectVFT parameterActivationVFT = {
        (EncodeFunction) ParameterActivation_encode,
        (DestroyFunction) ParameterActivation_destroy
};

static void
ParameterActivation_initialize(ParameterActivation self)
{
    self->virtualFunctionTable = &(parameterActivationVFT);
    self->type = P_AC_NA_1;
}

void
ParameterActivation_destroy(ParameterActivation self)
{
    GLOBAL_FREEMEM(self);
}


ParameterActivation
ParameterActivation_create(ParameterActivation self, int ioa, QualifierOfParameterActivation qpa)
{
    if (self == NULL)
        self = (ParameterActivation) GLOBAL_CALLOC(1, sizeof(struct sParameterActivation));

    if (self != NULL)
        ParameterActivation_initialize(self);

    self->objectAddress = ioa;
    self->qpa = qpa;

    return self;
}


QualifierOfParameterActivation
ParameterActivation_getQuality(ParameterActivation self)
{
    return self->qpa;
}

ParameterActivation
ParameterActivation_getFromBuffer(ParameterActivation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = (ParameterActivation) GLOBAL_MALLOC(sizeof(struct sParameterActivation));

        if (self != NULL)
            ParameterActivation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* QPA */
        self->qpa = (QualifierOfParameterActivation) msg [startIndex++];
    }

    return self;
}

/*******************************************
 * EndOfInitialization : InformationObject
 *******************************************/

struct sEndOfInitialization {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t coi;
};

static bool
EndOfInitialization_encode(EndOfInitialization self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte(frame, self->coi);

    return true;
}

struct sInformationObjectVFT EndOfInitializationVFT = {
        (EncodeFunction) EndOfInitialization_encode,
        (DestroyFunction) EndOfInitialization_destroy
};

static void
EndOfInitialization_initialize(EndOfInitialization self)
{
    self->virtualFunctionTable = &(EndOfInitializationVFT);
    self->type = M_EI_NA_1;
}

EndOfInitialization
EndOfInitialization_create(EndOfInitialization self, uint8_t coi)
{
    if (self == NULL) {
       self = (EndOfInitialization) GLOBAL_MALLOC(sizeof(struct sEndOfInitialization));

        if (self == NULL)
            return NULL;
        else
            EndOfInitialization_initialize(self);
    }

    self->objectAddress = 0;

    self->coi = coi;

    return self;
};

void
EndOfInitialization_destroy(EndOfInitialization self)
{
    GLOBAL_FREEMEM(self);
}

uint8_t
EndOfInitialization_getCOI(EndOfInitialization self)
{
    return self->coi;
}

EndOfInitialization
EndOfInitialization_getFromBuffer(EndOfInitialization self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 1)
        return NULL;

    if (self == NULL) {
       self = (EndOfInitialization) GLOBAL_MALLOC(sizeof(struct sEndOfInitialization));

        if (self != NULL)
            EndOfInitialization_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* COI */
        self->coi = msg[startIndex];
    }

    return self;
};

/*******************************************
 * FileReady : InformationObject
 *******************************************/

struct sFileReady {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint32_t lengthOfFile;

    uint8_t frq; /* file ready qualifier */
};

static bool
FileReady_encode(FileReady self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, (uint8_t)(self->lengthOfFile % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfFile / 0x100) % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfFile / 0x10000) % 0x100));

    Frame_setNextByte (frame, self->frq);

    return true;
}

struct sInformationObjectVFT FileReadyVFT = {
        (EncodeFunction) FileReady_encode,
        (DestroyFunction) FileReady_destroy
};

static void
FileReady_initialize(FileReady self)
{
    self->virtualFunctionTable = &(FileReadyVFT);
    self->type = F_FR_NA_1;
}

FileReady
FileReady_create(FileReady self, int ioa, uint16_t nof, uint32_t lengthOfFile, bool positive)
{
    if (self == NULL) {
       self = (FileReady) GLOBAL_MALLOC(sizeof(struct sFileReady));

        if (self == NULL)
            return NULL;
        else
            FileReady_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->lengthOfFile = lengthOfFile;

    if (positive)
        self->frq = 0x80;
    else
        self->frq = 0;

    return self;
}

uint8_t
FileReady_getFRQ(FileReady self)
{
    return self->frq;
}

void
FileReady_setFRQ(FileReady self, uint8_t frq)
{
    self->frq = frq;
}

bool
FileReady_isPositive(FileReady self)
{
    return ((self->frq & 0x80) == 0x80);
}

uint16_t
FileReady_getNOF(FileReady self)
{
    return self->nof;
}

uint32_t
FileReady_getLengthOfFile(FileReady self)
{
    return self->lengthOfFile;
}

void
FileReady_destroy(FileReady self)
{
    GLOBAL_FREEMEM(self);
}

FileReady
FileReady_getFromBuffer(FileReady self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 6)
        return NULL;

    if (self == NULL) {
       self = (FileReady) GLOBAL_MALLOC(sizeof(struct sFileReady));

        if (self != NULL)
            FileReady_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->lengthOfFile = msg [startIndex++];
        self->lengthOfFile += (msg [startIndex++] * 0x100);
        self->lengthOfFile += (msg [startIndex++] * 0x10000);

        /* FRQ */
        self->frq = msg[startIndex];
    }

    return self;
};

/*******************************************
 * SectionReady : InformationObject
 *******************************************/

struct sSectionReady {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint32_t lengthOfSection;

    uint8_t srq; /* section ready qualifier */
};

static bool
SectionReady_encode(SectionReady self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, self->nameOfSection);

    Frame_setNextByte (frame, (uint8_t)(self->lengthOfSection % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfSection / 0x100) % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfSection / 0x10000) % 0x100));

    Frame_setNextByte (frame, self->srq);

    return true;
}

struct sInformationObjectVFT SectionReadyVFT = {
        (EncodeFunction) SectionReady_encode,
        (DestroyFunction) SectionReady_destroy
};

static void
SectionReady_initialize(SectionReady self)
{
    self->virtualFunctionTable = &(SectionReadyVFT);
    self->type = F_SR_NA_1;
}

SectionReady
SectionReady_create(SectionReady self, int ioa, uint16_t nof, uint8_t nos, uint32_t lengthOfSection, bool notReady)
{
    if (self == NULL) {
       self = (SectionReady) GLOBAL_MALLOC(sizeof(struct sSectionReady));

        if (self == NULL)
            return NULL;
        else
            SectionReady_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->nameOfSection = nos;
    self->lengthOfSection = lengthOfSection;

    if (notReady)
        self->srq = 0x80;
    else
        self->srq = 0;

    return self;
}


bool
SectionReady_isNotReady(SectionReady self)
{
    return ((self->srq & 0x80) == 0x80);
}

uint8_t
SectionReady_getSRQ(SectionReady self)
{
    return self->srq;
}

void
SectionReady_setSRQ(SectionReady self, uint8_t srq)
{
    self->srq = srq;
}

uint16_t
SectionReady_getNOF(SectionReady self)
{
    return self->nof;
}

uint8_t
SectionReady_getNameOfSection(SectionReady self)
{
    return self->nameOfSection;
}

uint32_t
SectionReady_getLengthOfSection(SectionReady self)
{
    return self->lengthOfSection;
}

void
SectionReady_destroy(SectionReady self)
{
    GLOBAL_FREEMEM(self);
}


SectionReady
SectionReady_getFromBuffer(SectionReady self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 7)
        return NULL;

    if (self == NULL) {
       self = (SectionReady) GLOBAL_MALLOC(sizeof(struct sSectionReady));

        if (self != NULL)
            SectionReady_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->nameOfSection = msg[startIndex++];

        self->lengthOfSection = msg [startIndex++];
        self->lengthOfSection += (msg [startIndex++] * 0x100);
        self->lengthOfSection += (msg [startIndex++] * 0x10000);

        self->srq = msg[startIndex];
    }

    return self;
};


/*******************************************
 * FileCallOrSelect : InformationObject
 *******************************************/

struct sFileCallOrSelect {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t scq; /* select and call qualifier */
};

static bool
FileCallOrSelect_encode(FileCallOrSelect self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 1 : (parameters->sizeOfIOA + 1);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, self->nameOfSection);

    Frame_setNextByte (frame, self->scq);

    return true;
}

struct sInformationObjectVFT FileCallOrSelectVFT = {
        (EncodeFunction) FileCallOrSelect_encode,
        (DestroyFunction) FileCallOrSelect_destroy
};

static void
FileCallOrSelect_initialize(FileCallOrSelect self)
{
    self->virtualFunctionTable = &(FileCallOrSelectVFT);
    self->type = F_SC_NA_1;
}

FileCallOrSelect
FileCallOrSelect_create(FileCallOrSelect self, int ioa, uint16_t nof, uint8_t nos, uint8_t scq)
{
    if (self == NULL) {
       self = (FileCallOrSelect) GLOBAL_MALLOC(sizeof(struct sFileCallOrSelect));

        if (self == NULL)
            return NULL;
        else
            FileCallOrSelect_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->nameOfSection = nos;
    self->scq = scq;

    return self;
}

uint16_t
FileCallOrSelect_getNOF(FileCallOrSelect self)
{
    return self->nof;
}

uint8_t
FileCallOrSelect_getNameOfSection(FileCallOrSelect self)
{
    return self->nameOfSection;
}

uint8_t
FileCallOrSelect_getSCQ(FileCallOrSelect self)
{
    return self->scq;
}


void
FileCallOrSelect_destroy(FileCallOrSelect self)
{
    GLOBAL_FREEMEM(self);
}


FileCallOrSelect
FileCallOrSelect_getFromBuffer(FileCallOrSelect self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 4)
        return NULL;

    if (self == NULL) {
       self = (FileCallOrSelect) GLOBAL_MALLOC(sizeof(struct sFileCallOrSelect));

        if (self != NULL)
            FileCallOrSelect_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->nameOfSection = msg[startIndex++];

        self->scq = msg[startIndex];
    }

    return self;
};

/*************************************************
 * FileLastSegmentOrSection : InformationObject
 *************************************************/

struct sFileLastSegmentOrSection {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t lsq; /* last section or segment qualifier */

    uint8_t chs; /* checksum of section or segment */
};

static bool
FileLastSegmentOrSection_encode(FileLastSegmentOrSection self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, self->nameOfSection);

    Frame_setNextByte (frame, self->lsq);

    Frame_setNextByte (frame, self->chs);

    return true;
}

struct sInformationObjectVFT FileLastSegmentOrSectionVFT = {
        (EncodeFunction) FileLastSegmentOrSection_encode,
        (DestroyFunction) FileLastSegmentOrSection_destroy
};

static void
FileLastSegmentOrSection_initialize(FileLastSegmentOrSection self)
{
    self->virtualFunctionTable = &(FileLastSegmentOrSectionVFT);
    self->type = F_LS_NA_1;
}

FileLastSegmentOrSection
FileLastSegmentOrSection_create(FileLastSegmentOrSection self, int ioa, uint16_t nof, uint8_t nos, uint8_t lsq, uint8_t chs)
{
    if (self == NULL) {
       self = (FileLastSegmentOrSection) GLOBAL_MALLOC(sizeof(struct sFileLastSegmentOrSection));

        if (self == NULL)
            return NULL;
        else
            FileLastSegmentOrSection_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->nameOfSection = nos;
    self->lsq = lsq;
    self->chs = chs;

    return self;
}

uint16_t
FileLastSegmentOrSection_getNOF(FileLastSegmentOrSection self)
{
    return self->nof;
}

uint8_t
FileLastSegmentOrSection_getNameOfSection(FileLastSegmentOrSection self)
{
    return self->nameOfSection;
}

uint8_t
FileLastSegmentOrSection_getLSQ(FileLastSegmentOrSection self)
{
    return self->lsq;
}

uint8_t
FileLastSegmentOrSection_getCHS(FileLastSegmentOrSection self)
{
    return self->chs;
}

void
FileLastSegmentOrSection_destroy(FileLastSegmentOrSection self)
{
    GLOBAL_FREEMEM(self);
}

FileLastSegmentOrSection
FileLastSegmentOrSection_getFromBuffer(FileLastSegmentOrSection self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 5)
        return NULL;

    if (self == NULL) {
       self = (FileLastSegmentOrSection) GLOBAL_MALLOC(sizeof(struct sFileLastSegmentOrSection));

        if (self != NULL)
            FileLastSegmentOrSection_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->nameOfSection = msg[startIndex++];

        self->lsq = msg[startIndex];

        self->chs = msg[startIndex];
    }

    return self;
};

/*************************************************
 * FileACK : InformationObject
 *************************************************/

struct sFileACK {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t afq; /* AFQ (acknowledge file or section qualifier) */
};

static bool
FileACK_encode(FileACK self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, self->nameOfSection);

    Frame_setNextByte (frame, self->afq);

    return true;
}

struct sInformationObjectVFT FileACKVFT = {
        (EncodeFunction) FileACK_encode,
        (DestroyFunction) FileACK_destroy
};

static void
FileACK_initialize(FileACK self)
{
    self->virtualFunctionTable = &(FileACKVFT);
    self->type = F_AF_NA_1;
}


FileACK
FileACK_create(FileACK self, int ioa, uint16_t nof, uint8_t nos, uint8_t afq)
{
    if (self == NULL) {
       self = (FileACK) GLOBAL_MALLOC(sizeof(struct sFileACK));

        if (self == NULL)
            return NULL;
        else
            FileACK_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->nameOfSection = nos;
    self->afq = afq;

    return self;
}

uint16_t
FileACK_getNOF(FileACK self)
{
    return self->nof;
}

uint8_t
FileACK_getNameOfSection(FileACK self)
{
    return self->nameOfSection;
}

uint8_t
FileACK_getAFQ(FileACK self)
{
    return self->afq;
}

void
FileACK_destroy(FileACK self)
{
    GLOBAL_FREEMEM(self);
}

FileACK
FileACK_getFromBuffer(FileACK self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 4)
        return NULL;

    if (self == NULL) {
       self = (FileACK) GLOBAL_MALLOC(sizeof(struct sFileACK));

        if (self != NULL)
            FileACK_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->nameOfSection = msg[startIndex++];

        self->afq = msg[startIndex];
    }

    return self;
};


/*************************************************
 * FileSegment : InformationObject
 *************************************************/

struct sFileSegment {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t los; /* length of segment */

    uint8_t* data; /* user data buffer - file payload */
};

static bool
FileSegment_encode(FileSegment self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    if (self->los > FileSegment_GetMaxDataSize(parameters))
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, self->nameOfSection);

    Frame_setNextByte (frame, self->los);

    Frame_appendBytes (frame, self->data, self->los);

    return true;
}

struct sInformationObjectVFT FileSegmentVFT = {
        (EncodeFunction) FileSegment_encode,
        (DestroyFunction) FileSegment_destroy
};

static void
FileSegment_initialize(FileSegment self)
{
    self->virtualFunctionTable = &(FileSegmentVFT);
    self->type = F_SG_NA_1;
}

FileSegment
FileSegment_create(FileSegment self, int ioa, uint16_t nof, uint8_t nos, uint8_t* data, uint8_t los)
{
    if (self == NULL) {
       self = (FileSegment) GLOBAL_MALLOC(sizeof(struct sFileSegment));

        if (self == NULL)
            return NULL;
        else
            FileSegment_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->nameOfSection = nos;
    self->data = data;
    self->los = los;

    return self;
}

uint16_t
FileSegment_getNOF(FileSegment self)
{
    return self->nof;
}

uint8_t
FileSegment_getNameOfSection(FileSegment self)
{
    return self->nameOfSection;
}

uint8_t
FileSegment_getLengthOfSegment(FileSegment self)
{
    return self->los;
}

uint8_t*
FileSegment_getSegmentData(FileSegment self)
{
    return self->data;
}


int
FileSegment_GetMaxDataSize(ConnectionParameters parameters)
{
    int maxSize = 249 -
        parameters->sizeOfTypeId - parameters->sizeOfVSQ - parameters->sizeOfCA - parameters->sizeOfCOT;

    return maxSize;
}

void
FileSegment_destroy(FileSegment self)
{
    GLOBAL_FREEMEM(self);
}

FileSegment
FileSegment_getFromBuffer(FileSegment self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 4)
        return NULL;

    uint8_t los = msg[startIndex + 3 + parameters->sizeOfIOA];

    if ((msgSize - startIndex) < (parameters->sizeOfIOA) + 4 + los)
        return NULL;

    if (self == NULL) {
       self = (FileSegment) GLOBAL_MALLOC(sizeof(struct sFileSegment));

        if (self != NULL)
            FileSegment_initialize(self);
    }

    if (self != NULL) {
        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->nameOfSection = msg[startIndex++];

        self->los = msg[startIndex++];

        self->data = msg + startIndex;
    }

    return self;
};

/*************************************************
 * FileDirectory: InformationObject
 *************************************************/

struct sFileDirectory {

    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    int lengthOfFile; /* LOF */

    uint8_t sof; /* state of file */

    struct sCP56Time2a creationTime;
};

static bool
FileDirectory_encode(FileDirectory self, Frame frame, ConnectionParameters parameters, bool isSequence)
{
    int size = isSequence ? 13 : (parameters->sizeOfIOA + 13);

    if (Frame_getSpaceLeft(frame) < size)
        return false;

    InformationObject_encodeBase((InformationObject) self, frame, parameters, isSequence);

    Frame_setNextByte (frame, (uint8_t)((int) self->nof % 256));
    Frame_setNextByte (frame, (uint8_t)((int) self->nof / 256));

    Frame_setNextByte (frame, (uint8_t)(self->lengthOfFile % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfFile / 0x100) % 0x100));
    Frame_setNextByte (frame, (uint8_t)((self->lengthOfFile / 0x10000) % 0x100));

    Frame_setNextByte (frame, self->sof);

    Frame_appendBytes(frame, self->creationTime.encodedValue, 7);

    return true;
}

struct sInformationObjectVFT FileDirectoryVFT = {
        (EncodeFunction) FileDirectory_encode,
        (DestroyFunction) FileDirectory_destroy
};

static void
FileDirectory_initialize(FileDirectory self)
{
    self->virtualFunctionTable = &(FileDirectoryVFT);
    self->type = F_DR_TA_1;
}

FileDirectory
FileDirectory_create(FileDirectory self, int ioa, uint16_t nof, int lengthOfFile, uint8_t sof, CP56Time2a creationTime)
{
    if (self == NULL) {
       self = (FileDirectory) GLOBAL_MALLOC(sizeof(struct sFileDirectory));

        if (self == NULL)
            return NULL;
        else
            FileDirectory_initialize(self);
    }

    self->objectAddress = ioa;
    self->nof = nof;
    self->sof = sof;
    self->creationTime = *creationTime;

    return self;
}

uint16_t
FileDirectory_getNOF(FileDirectory self)
{
    return self->nof;
}

uint8_t
FileDirectory_getSOF(FileDirectory self)
{
    return self->sof;
}

int
FileDirectory_getSTATUS(FileDirectory self)
{
    return (int) (self->sof & CS101_SOF_STATUS);
}

bool
FileDirectory_getLFD(FileDirectory self)
{
    return ((self->sof & CS101_SOF_LFD) == CS101_SOF_LFD);
}

bool
FileDirectory_getFOR(FileDirectory self)
{
    return ((self->sof & CS101_SOF_FOR) == CS101_SOF_FOR);
}

bool
FileDirectory_getFA(FileDirectory self)
{
    return ((self->sof & CS101_SOF_FA) == CS101_SOF_FA);
}


uint8_t
FileDirectory_getLengthOfFile(FileDirectory self)
{
    return self->lengthOfFile;
}

CP56Time2a
FileDirectory_getCreationTime(FileDirectory self)
{
    return &(self->creationTime);
}

void
FileDirectory_destroy(FileDirectory self)
{
    GLOBAL_FREEMEM(self);
}

FileDirectory
FileDirectory_getFromBuffer(FileDirectory self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence)
{

    if (self == NULL) {
       self = (FileDirectory) GLOBAL_MALLOC(sizeof(struct sFileDirectory));

        if (self != NULL)
            FileDirectory_initialize(self);
    }

    if (self != NULL) {

        if (!isSequence) {
            InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

            startIndex += parameters->sizeOfIOA; /* skip IOA */
        }

        self->nof = msg[startIndex++];
        self->nof += (msg[startIndex++] * 0x100);

        self->lengthOfFile = msg [startIndex++];
        self->lengthOfFile += (msg [startIndex++] * 0x100);
        self->lengthOfFile += (msg [startIndex++] * 0x10000);


        self->sof = msg[startIndex++];

        CP56Time2a_getFromBuffer(&(self->creationTime), msg, msgSize, startIndex);
    }

    return self;
}


union uInformationObject {
    struct sSinglePointInformation m1;
    struct sStepPositionInformation m2;
    struct sStepPositionWithCP24Time2a m3;
    struct sStepPositionWithCP56Time2a m4;
    struct sDoublePointInformation m5;
    struct sDoublePointWithCP24Time2a m6;
    struct sDoublePointWithCP56Time2a m7;
    struct sSinglePointWithCP24Time2a m8;
    struct sSinglePointWithCP56Time2a m9;
    struct sBitString32 m10;
    struct sBitstring32WithCP24Time2a m11;
    struct sBitstring32WithCP56Time2a m12;
    struct sMeasuredValueNormalized m13;
    struct sMeasuredValueNormalizedWithCP24Time2a m14;
    struct sMeasuredValueNormalizedWithCP56Time2a m15;
    struct sMeasuredValueScaled m16;
    struct sMeasuredValueScaledWithCP24Time2a m17;
    struct sMeasuredValueScaledWithCP56Time2a m18;
    struct sMeasuredValueShort m19;
    struct sMeasuredValueShortWithCP24Time2a m20;
    struct sMeasuredValueShortWithCP56Time2a m21;
    struct sIntegratedTotals m22;
    struct sIntegratedTotalsWithCP24Time2a m23;
    struct sIntegratedTotalsWithCP56Time2a m24;
    struct sSingleCommand m25;
    struct sSingleCommandWithCP56Time2a m26;
    struct sDoubleCommand m27;
    struct sStepCommand m28;
    struct sSetpointCommandNormalized m29;
    struct sSetpointCommandScaled m30;
    struct sSetpointCommandShort m31;
    struct sBitstring32Command m32;
    struct sReadCommand m33;
    struct sClockSynchronizationCommand m34;
    struct sInterrogationCommand m35;
    struct sParameterActivation m36;
    struct sEventOfProtectionEquipmentWithCP56Time2a m37;
    struct sStepCommandWithCP56Time2a m38;
};

int
InformationObject_getMaxSizeInMemory()
{
    return sizeof(union uInformationObject);
}
