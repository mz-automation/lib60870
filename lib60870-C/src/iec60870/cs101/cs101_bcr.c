/*
 *  bcr.c
 *
 *  Implementation of Binary Counter Reading (BCR) data type.
 *
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

#include "iec60870_common.h"

#include <stdlib.h>

#include "platform_endian.h"
#include "apl_types_internal.h"
#include "lib_memory.h"

BinaryCounterReading
BinaryCounterReading_create(BinaryCounterReading self, int32_t value, int seqNumber,
        bool hasCarry, bool isAdjusted, bool isInvalid)
{
    if (self == NULL)
        self = (BinaryCounterReading) GLOBAL_MALLOC(sizeof(struct sBinaryCounterReading));

    if (self != NULL) {
        BinaryCounterReading_setValue(self, value);
        BinaryCounterReading_setSequenceNumber(self, seqNumber);
        BinaryCounterReading_setCarry(self, hasCarry);
        BinaryCounterReading_setAdjusted(self, isAdjusted);
        BinaryCounterReading_setInvalid(self, isInvalid);
    }

    return self;
}

void
BinaryCounterReading_destroy(BinaryCounterReading self)
{
    GLOBAL_FREEMEM(self);
}

int32_t
BinaryCounterReading_getValue(BinaryCounterReading self)
{
    int32_t value;

    uint8_t* valueBytes = (uint8_t*) &(value);
    uint8_t* encodedValue = self->encodedValue;

#if (ORDER_LITTLE_ENDIAN == 1)
    valueBytes[0] = encodedValue[0];
    valueBytes[1] = encodedValue[1];
    valueBytes[2] = encodedValue[2];
    valueBytes[3] = encodedValue[3];
#else
    valueBytes[0] = encodedValue[3];
    valueBytes[1] = encodedValue[2];
    valueBytes[2] = encodedValue[1];
    valueBytes[3] = encodedValue[0];
#endif

    return value;
}

void
BinaryCounterReading_setValue(BinaryCounterReading self, int32_t value)
{
    uint8_t* valueBytes = (uint8_t*) &(value);
    uint8_t* encodedValue = self->encodedValue;

#if (ORDER_LITTLE_ENDIAN == 1)
    encodedValue[0] = valueBytes[0];
    encodedValue[1] = valueBytes[1];
    encodedValue[2] = valueBytes[2];
    encodedValue[3] = valueBytes[3];
#else
    encodedValue[0] = valueBytes[3];
    encodedValue[1] = valueBytes[2];
    encodedValue[2] = valueBytes[1];
    encodedValue[3] = valueBytes[0];
#endif
}

int
BinaryCounterReading_getSequenceNumber(BinaryCounterReading self)
{
    return (self->encodedValue[4] & 0x1f);
}

void
BinaryCounterReading_setSequenceNumber(BinaryCounterReading self, int value)
{
    int seqNumber = value & 0x1f;
    int flags = self->encodedValue[4] & 0xe0;

    self->encodedValue[4] = flags | seqNumber;
}

bool
BinaryCounterReading_hasCarry(BinaryCounterReading self)
{
    return ((self->encodedValue[4] & 0x20) == 0x20);
}

void
BinaryCounterReading_setCarry(BinaryCounterReading self, bool value)
{
    if (value)
        self->encodedValue[4] |= 0x20;
    else
        self->encodedValue[4] &= 0xdf;
}

bool
BinaryCounterReading_isAdjusted(BinaryCounterReading self)
{
    return ((self->encodedValue[4] & 0x40) == 0x40);
}

void
BinaryCounterReading_setAdjusted(BinaryCounterReading self, bool value)
{
    if (value)
        self->encodedValue[4] |= 0x40;
    else
        self->encodedValue[4] &= 0xbf;
}

bool
BinaryCounterReading_isInvalid(BinaryCounterReading self)
{
    return ((self->encodedValue[4] & 0x80) == 0x80);
}

void
BinaryCounterReading_setInvalid(BinaryCounterReading self, bool value)
{
    if (value)
        self->encodedValue[4] |= 0x80;
    else
        self->encodedValue[4] &= 0x7f;
}
