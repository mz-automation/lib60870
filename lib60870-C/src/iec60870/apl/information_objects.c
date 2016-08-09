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
#include "lib_memory.h"
#include "frame.h"

typedef enum {
    IEC60870_TYPE_SINGLE_POINT_INFORMATION = 1,
    IEC60870_TYPE_SINGLE_POINT_WITH_CP24TIME2A,
    IEC60870_TYPE_SINGLE_POINT_WITH_CP56TIME2A,
    IEC60870_TYPE_DOUBLE_POINT_INFORMATION,
    IEC60870_TYPE_DOUBLE_POINT_WITH_CP24TIME2A,
    IEC60870_TYPE_DOUBLE_POINT_WITH_CP56TIME2A,
    IEC60870_TYPE_STEP_POSITION_INFORMATION,
    IEC60870_TYPE_STEP_POSITION_WITH_CP24TIME2A,
    IEC60870_TYPE_STEP_POSITION_WITH_CP56TIME2A,
    IEC60870_TYPE_BITSTRING32,
    IEC60870_TYPE_BITSTRING32_WITH_CP24TIME2A,
    IEC60870_TYPE_BITSTRING32_WITH_CP56TIME2A,
    IEC60870_TYPE_MEAS_VALUE_NORM,
    IEC60870_TYPE_MEAS_VALUE_NORM_WITH_CP24TIME2A,
    IEC60870_TYPE_MEAS_VALUE_NORM_WITH_CP56TIME2A,
    IEC60870_TYPE_MEAS_VALUE_SCALED,
    IEC60870_TYPE_MEAS_VALUE_SCALED_WITH_CP24TIME2A,
    IEC60870_TYPE_MEAS_VALUE_SCALED_WITH_CP56TIME2A
} InformationObjectType;

typedef struct sInformationObjectVFT* InformationObjectVFT;

struct sInformationObjectVFT {
    void (*encode)(InformationObject self, Frame frame, ConnectionParameters parameters);
    void (*destroy)(InformationObject self);
#if 0
    const char* (*toString)(InformationObject self);
#endif
};

static void
SinglePointInformation_encode(SinglePointInformation self, Frame frame, ConnectionParameters parameters);


/*****************************************
 * Information object hierarchy
 *****************************************/

struct sInformationObject {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;
};

struct sSinglePointInformationWithCP24Time2a {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sSinglePointInformationWithCP56Time2a {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};



void
InformationObject_encode(InformationObject self, Frame frame, ConnectionParameters parameters)
{
    self->virtualFunctionTable->encode(self, frame, parameters);
}

void
InformationObject_destroy(InformationObject self)
{
    self->virtualFunctionTable->destroy(self);
}

static void
InformationObject_encodeBase(InformationObject self, Frame frame, ConnectionParameters parameters)
{
    Frame_setNextByte(frame, (uint8_t)(self->objectAddress & 0xff));

    if (parameters->sizeOfIOA > 1)
        Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x100) & 0xff));

    if (parameters->sizeOfIOA > 2)
        Frame_setNextByte(frame, (uint8_t)((self->objectAddress / 0x10000) & 0xff));
}

static void
InformationObject_getFromBuffer(InformationObject self, ConnectionParameters parameters,
        uint8_t* msg, int startIndex)
{
    /* parse information object address */
    self->objectAddress = msg [startIndex];

    if (parameters->sizeOfIOA > 1)
        self->objectAddress += (msg [startIndex + 1] * 0x100);

    if (parameters->sizeOfIOA > 2)
        self->objectAddress += (msg [startIndex + 2] * 0x10000);
}

/**********************************************
 * SinglePointInformation
 **********************************************/

struct sSinglePointInformation {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;
};

struct sInformationObjectVFT singlePointInformationVFT = {
        .encode = SinglePointInformation_encode,
        .destroy = SinglePointInformation_destroy
};

void
SinglePointInformation_initialize(SinglePointInformation self)
{
    self->virtualFunctionTable = &(singlePointInformationVFT);
    self->type = IEC60870_TYPE_SINGLE_POINT_INFORMATION;
}

void
SinglePointInformation_destroy(SinglePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sSinglePointInformation));

        if (self != NULL)
            SinglePointInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);
    }

    return self;
}


int
SinglePointInformation_getObjectAddress(SinglePointInformation self)
{
    return self->objectAddress;
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

static void
SinglePointInformation_encode(SinglePointInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);
}


/**********************************************
 * StepPositionInformation
 **********************************************/

struct sStepPositionInformation {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;
};

static void
StepPositionInformation_encode(StepPositionInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT stepPositionInformationVFT = {
        .encode = StepPositionInformation_encode,
        .destroy = StepPositionInformation_destroy
};

void
StepPositionInformation_initialize(StepPositionInformation self)
{
    self->virtualFunctionTable = &(stepPositionInformationVFT);
    self->type = IEC60870_TYPE_STEP_POSITION_INFORMATION;
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
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sStepPositionInformation));

        if (self != NULL)
            StepPositionInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
StepPositionWithCP56Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT stepPositionWithCP56Time2aVFT = {
        .encode = StepPositionWithCP56Time2a_encode,
        .destroy = StepPositionWithCP56Time2a_destroy
};

void
StepPositionWithCP56Time2a_initialize(StepPositionWithCP56Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP56Time2aVFT);
    self->type = IEC60870_TYPE_STEP_POSITION_WITH_CP56TIME2A;
}

void
StepPositionWithCP56Time2a_destroy(StepPositionWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP56Time2a
StepPositionWithCP56Time2a_getTimestamp(StepPositionWithCP56Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_getFromBuffer(StepPositionWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP56Time2a));

        if (self != NULL)
            StepPositionInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse VTI (value with transient state indication) */
        self->vti = msg [startIndex++];

        self->quality = (QualityDescriptor) msg [startIndex];

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


static void
StepPositionWithCP24Time2a_encode(StepPositionWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->vti);

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT stepPositionWithCP24Time2aVFT = {
        .encode = StepPositionWithCP24Time2a_encode,
        .destroy = StepPositionWithCP24Time2a_destroy
};

void
StepPositionWithCP24Time2a_initialize(StepPositionWithCP24Time2a self)
{
    self->virtualFunctionTable = &(stepPositionWithCP24Time2aVFT);
    self->type = IEC60870_TYPE_STEP_POSITION_WITH_CP24TIME2A;
}

void
StepPositionWithCP24Time2a_destroy(StepPositionWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP24Time2a
StepPositionWithCP24Time2a_getTimestamp(StepPositionWithCP24Time2a self)
{
    return &(self->timestamp);
}

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_getFromBuffer(StepPositionWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sStepPositionWithCP24Time2a));

        if (self != NULL)
            StepPositionInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;
};

static void
DoublePointInformation_encode(DoublePointInformation self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);
}

void
DoublePointInformation_destroy(DoublePointInformation self)
{
    GLOBAL_FREEMEM(self);
}

struct sInformationObjectVFT doublePointInformationVFT = {
        .encode = DoublePointInformation_encode,
        .destroy = DoublePointInformation_destroy
};

void
DoublePointInformation_initialize(DoublePointInformation self)
{
    self->virtualFunctionTable = &(doublePointInformationVFT);
    self->type = IEC60870_TYPE_DOUBLE_POINT_INFORMATION;
}

DoublePointInformation
DoublePointInformation_getFromBuffer(DoublePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sDoublePointInformation));

        if (self != NULL)
            DoublePointInformation_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);
    }

    return self;
}


/*******************************************
 * DoublePointWithCP24Time2a
 *******************************************/

struct sDoublePointWithCP24Time2a {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
DoublePointWithCP24Time2a_encode(DoublePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}


void
DoublePointWithCP24Time2a_destroy(DoublePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

struct sInformationObjectVFT doublePointWithCP24Time2aVFT = {
        .encode = DoublePointWithCP24Time2a_encode,
        .destroy = DoublePointWithCP24Time2a_destroy
};

void
DoublePointWithCP24Time2a_initialize(DoublePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP24Time2aVFT);
    self->type = IEC60870_TYPE_DOUBLE_POINT_WITH_CP24TIME2A;
}

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_getFromBuffer(DoublePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP24Time2a));

        if (self != NULL)
            DoublePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
DoublePointWithCP56Time2a_encode(DoublePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    val += (int) self->value;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}


void
DoublePointWithCP56Time2a_destroy(DoublePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

struct sInformationObjectVFT doublePointWithCP56Time2aVFT = {
        .encode = DoublePointWithCP56Time2a_encode,
        .destroy = DoublePointWithCP56Time2a_destroy
};

void
DoublePointWithCP56Time2a_initialize(DoublePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(doublePointWithCP56Time2aVFT);
    self->type = IEC60870_TYPE_DOUBLE_POINT_WITH_CP56TIME2A;
}

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_getFromBuffer(DoublePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sDoublePointWithCP56Time2a));

        if (self != NULL)
            DoublePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = (DoublePointValue) ((siq & 0x01) == 0x03);

        self->quality = (QualityDescriptor) (siq & 0xf0);

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};


void
SinglePointWithCP24Time2a_destroy(SinglePointWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
SinglePointWithCP24Time2a_encode(SinglePointWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT singlePointWithCP24Time2aVFT = {
        .encode = SinglePointWithCP24Time2a_encode,
        .destroy = SinglePointWithCP24Time2a_destroy
};

void
SinglePointWithCP24Time2a_initialize(SinglePointWithCP24Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP24Time2aVFT);
    self->type = IEC60870_TYPE_SINGLE_POINT_WITH_CP24TIME2A;
}

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_getFromBuffer(SinglePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP24Time2a));

        if (self != NULL)
            SinglePointWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};


void
SinglePointWithCP56Time2a_destroy(SinglePointWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

static void
SinglePointWithCP56Time2a_encode(SinglePointWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    uint8_t val = (uint8_t) self->quality;

    if (self->value)
        val++;

    Frame_setNextByte(frame, val);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}


struct sInformationObjectVFT singlePointWithCP56Time2aVFT = {
        .encode = SinglePointWithCP56Time2a_encode,
        .destroy = SinglePointWithCP56Time2a_destroy
};


void
SinglePointWithCP56Time2a_initialize(SinglePointWithCP56Time2a self)
{
    self->virtualFunctionTable = &(singlePointWithCP56Time2aVFT);
    self->type = IEC60870_TYPE_SINGLE_POINT_WITH_CP56TIME2A;
}


SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_getFromBuffer(SinglePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sSinglePointWithCP56Time2a));

        if (self != NULL)
            SinglePointWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* parse SIQ (single point information with qualitiy) */
        uint8_t siq = msg [startIndex];

        self->value = ((siq & 0x01) == 0x01);

        self->quality = (QualityDescriptor) (siq & 0xf0);

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

/*******************************************
 * MeasuredValueScaledWithCP56Time2a
 *******************************************/

struct sMeasuredValueScaledWithCP56Time2a {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    int scaledValue;

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
MeasuredValueScaledWithCP56Time2a_encode(MeasuredValueScaledWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    /* scaledValue */
    int valueToEncode;

    if (self->scaledValue < 0)
        valueToEncode = self->scaledValue + 65536;
    else
        valueToEncode = self->scaledValue;

    Frame_setNextByte(frame, (uint8_t) (valueToEncode % 256));
    Frame_setNextByte(frame, (uint8_t) (valueToEncode / 256));

    /* quality */
    Frame_setNextByte(frame, (uint8_t) (self->quality));

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT measuredValueScaledWithCP56Time2aVFT = {
        .encode = MeasuredValueScaledWithCP56Time2a_encode,
        .destroy = MeasuredValueScaledWithCP56Time2a_destroy
};

void
MeasuredValueScaledWithCP56Time2a_initialize(MeasuredValueScaledWithCP56Time2a self)
{
    self->virtualFunctionTable = &measuredValueScaledWithCP56Time2aVFT;
    self->type = IEC60870_TYPE_MEAS_VALUE_SCALED_WITH_CP56TIME2A;
}

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaledWithCP56Time2a));

        if (self != NULL)
            MeasuredValueScaledWithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        /* scaled value */
        int scaledValue;

        scaledValue = msg [startIndex++];
        scaledValue += (msg [startIndex++] * 0x100);

        if (scaledValue > 32767)
            scaledValue = scaledValue - 65536;

        self->scaledValue = scaledValue;

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];

        /* timestamp */
        CP56Time2a_getFromBuffer(&(self->timestamp), msg, msgSize, startIndex);
    }

    return self;
}

int
MeasuredValueScaledWithCP56Time2a_getObjectAddress(MeasuredValueScaledWithCP56Time2a self)
{
   return self->objectAddress;
}

int
MeasuredValueScaledWithCP56Time2a_getScaledValue(MeasuredValueScaledWithCP56Time2a self)
{
   return self->scaledValue;
}

QualityDescriptor
MeasuredValueScaledWithCP56Time2a_getQuality(MeasuredValueScaledWithCP56Time2a self)
{
    return self->quality;
}

CP56Time2a
MeasuredValueScaledWithCP56Time2a_getTimestamp(MeasuredValueScaledWithCP56Time2a self)
{
    return &(self->timestamp);
}

void
MeasuredValueScaledWithCP56Time2a_destroy(MeasuredValueScaledWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

/**********************************************
 * BitString32
 **********************************************/

struct sBitString32 {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;
};

static void
BitString32_encode(BitString32 self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT bitString32VFT = {
        .encode = BitString32_encode,
        .destroy = BitString32_destroy
};

void
BitString32_initialize(BitString32 self)
{
    self->virtualFunctionTable = &(bitString32VFT);
    self->type = IEC60870_TYPE_BITSTRING32;
}

void
BitString32_destroy(BitString32 self)
{
    GLOBAL_FREEMEM(self);
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
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            BitString32_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
Bitstring32WithCP24Time2a_encode(Bitstring32WithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT bitstring32WithCP24Time2aVFT = {
        .encode = Bitstring32WithCP24Time2a_encode,
        .destroy = Bitstring32WithCP24Time2a_destroy
};

void
Bitstring32WithCP24Time2a_initialize(Bitstring32WithCP24Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP24Time2aVFT);
    self->type = IEC60870_TYPE_BITSTRING32_WITH_CP24TIME2A;
}

void
Bitstring32WithCP24Time2a_destroy(Bitstring32WithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP24Time2a
Bitstring32WithCP24Time2a_getTimestamp(Bitstring32WithCP24Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_getFromBuffer(Bitstring32WithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
Bitstring32WithCP56Time2a_encode(Bitstring32WithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    int value = self->value;

    Frame_setNextByte(frame, (uint8_t) (value % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x100) % 0x100));
    Frame_setNextByte(frame, (uint8_t) ((value / 0x10000) % 0x100));
    Frame_setNextByte(frame, (uint8_t) (value / 0x1000000));

    Frame_setNextByte(frame, (uint8_t) self->quality);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT bitstring32WithCP56Time2aVFT = {
        .encode = Bitstring32WithCP56Time2a_encode,
        .destroy = Bitstring32WithCP56Time2a_destroy
};

void
Bitstring32WithCP56Time2a_initialize(Bitstring32WithCP56Time2a self)
{
    self->virtualFunctionTable = &(bitstring32WithCP56Time2aVFT);
    self->type = IEC60870_TYPE_BITSTRING32_WITH_CP56TIME2A;
}

void
Bitstring32WithCP56Time2a_destroy(Bitstring32WithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
}

CP24Time2a
Bitstring32WithCP56Time2a_getTimestamp(Bitstring32WithCP56Time2a self)
{
    return &(self->timestamp);
}

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_getFromBuffer(Bitstring32WithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sBitString32));

        if (self != NULL)
            Bitstring32WithCP56Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        uint32_t value;

        value = msg [startIndex++];
        value += ((uint32_t)msg [startIndex++] * 0x100);
        value += ((uint32_t)msg [startIndex++] * 0x10000);
        value += ((uint32_t)msg [startIndex++] * 0x1000000);

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

    InformationObjectType type;

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

    encodedValue[0] = (uint8_t)(valueToEncode % 256);
    encodedValue[1] = (uint8_t)(valueToEncode / 256);
}

static void
MeasuredValueNormalized_encode(MeasuredValueNormalized self, Frame frame, ConnectionParameters parameters)
{
    InformationObject_encodeBase((InformationObject) self, frame, parameters);

    Frame_setNextByte(frame, self->encodedValue[0]);
    Frame_setNextByte(frame, self->encodedValue[1]);

    Frame_setNextByte(frame, (uint8_t) self->quality);
}

struct sInformationObjectVFT measuredValueNormalizedVFT = {
        .encode = MeasuredValueNormalized_encode,
        .destroy = MeasuredValueNormalized_destroy
};

void
MeasuredValueNormalized_initialize(MeasuredValueNormalized self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedVFT);
    self->type = IEC60870_TYPE_MEAS_VALUE_NORM;
}

void
MeasuredValueNormalized_destroy(MeasuredValueNormalized self)
{
    GLOBAL_FREEMEM(self);
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
    //TODO check boundaries
    int scaledValue = (int)(value * 32767.f);

    setScaledValue(self->encodedValue, value);
}

QualityDescriptor
MeasuredValueNormalized_getQuality(MeasuredValueNormalized self)
{
    return self->quality;
}

MeasuredValueNormalized
MeasuredValueNormalized_getFromBuffer(MeasuredValueNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalized));

        if (self != NULL)
            MeasuredValueNormalized_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}

/***********************************************************************
 * MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
 ***********************************************************************/

struct sMeasuredValueNormalizedWithCP24Time2a {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

static void
MeasuredValueNormalizedWithCP24Time2a_encode(MeasuredValueNormalizedWithCP24Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 3);
}

struct sInformationObjectVFT measuredValueNormalizedWithCP24Time2aVFT = {
        .encode = MeasuredValueNormalizedWithCP24Time2a_encode,
        .destroy = MeasuredValueNormalizedWithCP24Time2a_destroy
};

void
MeasuredValueNormalizedWithCP24Time2a_initialize(MeasuredValueNormalizedWithCP24Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP24Time2aVFT);
    self->type = IEC60870_TYPE_MEAS_VALUE_NORM_WITH_CP24TIME2A;
}

void
MeasuredValueNormalizedWithCP24Time2a_destroy(MeasuredValueNormalizedWithCP24Time2a self)
{
    GLOBAL_FREEMEM(self);
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
        self->timestamp.encodedValue[i] = self->encodedValue[i];
    }
}

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(MeasuredValueNormalizedWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sMeasuredValueNormalizedWithCP24Time2a));

        if (self != NULL)
            MeasuredValueNormalizedWithCP24Time2a_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

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

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

static void
MeasuredValueNormalizedWithCP56Time2a_encode(MeasuredValueNormalizedWithCP56Time2a self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);

    /* timestamp */
    Frame_appendBytes(frame, self->timestamp.encodedValue, 7);
}

struct sInformationObjectVFT measuredValueNormalizedWithCP56Time2aVFT = {
        .encode = MeasuredValueNormalizedWithCP56Time2a_encode,
        .destroy = MeasuredValueNormalizedWithCP56Time2a_destroy
};

void
MeasuredValueNormalizedWithCP56Time2a_initialize(MeasuredValueNormalizedWithCP56Time2a self)
{
    self->virtualFunctionTable = &(measuredValueNormalizedWithCP56Time2aVFT);
    self->type = IEC60870_TYPE_MEAS_VALUE_NORM_WITH_CP56TIME2A;
}

void
MeasuredValueNormalizedWithCP56Time2a_destroy(MeasuredValueNormalizedWithCP56Time2a self)
{
    GLOBAL_FREEMEM(self);
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
        self->timestamp.encodedValue[i] = self->encodedValue[i];
    }
}


/*******************************************
 * MeasuredValueScaled
 *******************************************/

struct sMeasuredValueScaled {

    int objectAddress;

    InformationObjectType type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

static void
MeasuredValueScaled_encode(MeasuredValueScaled self, Frame frame, ConnectionParameters parameters)
{
    MeasuredValueNormalized_encode((MeasuredValueNormalized) self, frame, parameters);
}

struct sInformationObjectVFT measuredValueScaledVFT = {
        .encode = MeasuredValueScaled_encode,
        .destroy = MeasuredValueScaled_destroy
};

void
MeasuredValueScaled_initialize(MeasuredValueScaled self)
{
    self->virtualFunctionTable = &(measuredValueScaledVFT);
    self->type = IEC60870_TYPE_MEAS_VALUE_SCALED;
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
    //TODO check boundaries
    setScaledValue(self->encodedValue, value);
}

QualityDescriptor
MeasuredValueScaled_getQuality(MeasuredValueScaled self)
{
    return self->quality;
}

MeasuredValueScaled
MeasuredValueScaled_getFromBuffer(MeasuredValueScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex)
{
    //TODO check message size

    if (self == NULL) {
        self = GLOBAL_MALLOC(sizeof(struct sMeasuredValueScaled));

        if (self != NULL)
            MeasuredValueScaled_initialize(self);
    }

    if (self != NULL) {

        InformationObject_getFromBuffer((InformationObject) self, parameters, msg, startIndex);

        startIndex += parameters->sizeOfIOA; /* skip IOA */

        self->encodedValue[0] = msg [startIndex++];
        self->encodedValue[1] = msg [startIndex++];

        /* quality */
        self->quality = (QualityDescriptor) msg [startIndex++];
    }

    return self;
}


