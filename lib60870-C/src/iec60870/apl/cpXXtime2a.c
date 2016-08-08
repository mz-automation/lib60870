/*
 *  cpXXtime2a.c
 *
 *  Implementation of the types CP16Time2a, CP24Time2a and CP56Time2a
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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "lib_memory.h"

#include "iec60870_common.h"
#include "apl_types_internal.h"

/**********************************
 *  CP16Time2a type
 **********************************/

bool
CP16Time2a_getFromBuffer (CP16Time2a self, uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 2)
        return false;

    for (int i = 0; i < 2; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP16Time2a_getEplapsedTimeInMs(CP16Time2a self)
{
    return (self->encodedValue[0] + (self->encodedValue[1] * 0x100));
}

void
CP16Time2a_setEplapsedTimeInMs(CP16Time2a self, int value)
{
    self->encodedValue[0] = (uint8_t) (value % 0x100);
    self->encodedValue[1] = (uint8_t) (value / 0x100);
}

uint8_t*
CP16Time2a_getEncodedValue(CP16Time2a self)
{
    return self->encodedValue;
}

/************************************
 *  CP24Time2a and CP26Time2a common
 ************************************/

static int
getSecond(uint8_t* encodedValue)
{
    return  (encodedValue[0] + (encodedValue[1] * 0x100)) / 1000;
}

static void
setSecond(uint8_t* encodedValue, int value)
{
    int millies = encodedValue[0] + (encodedValue[1] * 0x100);

    int msPart = millies % 1000;

    millies = (value * 1000) + msPart;

    encodedValue[0] = (uint8_t) (millies & 0xff);
    encodedValue[1] = (uint8_t) ((millies / 0x100) & 0xff);
}

static int
getMinute(uint8_t* encodedValue)
{
    return (encodedValue[2] & 0x3f);
}

static void
setMinute(uint8_t* encodedValue, int value)
{
    encodedValue[2] = (uint8_t) ((encodedValue[2] & 0xc0) | (value & 0x3f));
}

static bool
isInvalid(uint8_t* encodedValue)
{
    return ((encodedValue[2] & 0x80) != 0);
}

static void
setInvalid(uint8_t* encodedValue, bool value)
{
    if (value)
        encodedValue[2] |= 0x80;
    else
        encodedValue[2] &= 0x7f;
}

static bool
isSubstituted(uint8_t* encodedValue)
{
    return ((encodedValue[2] & 0x40) == 0x40);
}

static void
setSubstituted(uint8_t* encodedValue, bool value)
{
    if (value)
        encodedValue[2] |= 0x40;
    else
        encodedValue[2] &= 0xbf;
}

/**********************************
 *  CP24Time2a type
 **********************************/

bool
CP24Time2a_getFromBuffer (CP24Time2a self, uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 3)
        return false;

    for (int i = 0; i < 3; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP24Time2a_getSecond(CP24Time2a self)
{
    return getSecond(self->encodedValue);
}

void
CP24Time2a_setSecond(CP24Time2a self, int value)
{
    setSecond(self->encodedValue, value);
}

int
CP24Time2a_getMinute(CP24Time2a self)
{
    return getMinute(self->encodedValue);
}


void
CP24Time2a_setMinute(CP24Time2a self, int value)
{
    setMinute(self->encodedValue, value);
}

bool
CP24Time2a_isInvalid(CP24Time2a self)
{
    return isInvalid(self->encodedValue);
}

void
CP24Time2a_setInvalid(CP24Time2a self, bool value)
{
    setInvalid(self->encodedValue, value);
}

bool
CP24Time2a_isSubstituted(CP24Time2a self)
{
    return isSubstituted(self->encodedValue);
}

void
CP24Time2a_setSubstituted(CP24Time2a self, bool value)
{
    setSubstituted(self->encodedValue, value);
}

/**********************************
 *  CP56Time2a type
 **********************************/

bool
CP56Time2a_getFromBuffer (CP56Time2a self, uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 7)
        return false;

    for (int i = 0; i < 7; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

CP56Time2a
CP56Time2a_createFromBuffer(uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 7)
        return NULL;

    CP56Time2a self = GLOBAL_MALLOC(sizeof(struct sCP56Time2a));

    if (self != NULL) {
        for (int i = 0; i < 7; i++)
            self->encodedValue[i] = msg[startIndex + i];
    }

    return self;
}

int
CP56Time2a_getMillisecond(CP56Time2a self)
{
    return (self->encodedValue[0] + (self->encodedValue[1] * 0x100)) % 1000;
}

void
CP56Time2a_setMillisecond(CP56Time2a self, int value)
{
    int millies = (CP56Time2a_getSecond(self) * 1000) + value;

    self->encodedValue[0] = (uint8_t) (millies & 0xff);
    self->encodedValue[1] = (uint8_t) ((millies / 0x100) & 0xff);
}

int
CP56Time2a_getSecond(CP56Time2a self)
{
    return getSecond(self->encodedValue);
}

void
CP56Time2a_setSecond(CP56Time2a self, int value)
{
    setSecond(self->encodedValue, value);
}

int
CP56Time2a_getMinute(CP56Time2a self)
{
    return getMinute(self->encodedValue);
}

void
CP56Time2a_setMinute(CP56Time2a self, int value)
{
    setMinute(self->encodedValue, value);
}

int
CP56Time2a_getHour(CP56Time2a self)
{
    return (self->encodedValue[3] & 0x1f);
}

void
CP56Time2a_setHour(CP56Time2a self, int value)
{
    self->encodedValue[3] = (uint8_t) ((self->encodedValue[3] & 0xe0) | (value & 0x1f));
}

int
CP56Time2a_getDayOfWeek(CP56Time2a self)
{
    return ((self->encodedValue[4] & 0xe0) >> 5);
}

void
CP56Time2a_setDayOfWeek(CP56Time2a self, int value)
{
    self->encodedValue[4] = (uint8_t) ((self->encodedValue[4] & 0x1f) | ((value & 0x07) << 5));
}

int
CP56Time2a_getDayOfMonth(CP56Time2a self)
{
    return (self->encodedValue[4] & 0x1f);
}

void
CP56Time2a_setDayOfMonth(CP56Time2a self, int value)
{
    self->encodedValue[4] = (uint8_t) ((self->encodedValue[4] & 0xe0) + (value & 0x1f));
}

int
CP56Time2a_getMonth(CP56Time2a self)
{
    return (self->encodedValue[5] & 0x0f);
}

void
CP56Time2a_setMonth(CP56Time2a self, int value)
{
    self->encodedValue[5] = (uint8_t) ((self->encodedValue[5] & 0xf0) + (value & 0x0f));
}

int
CP56Time2a_getYear(CP56Time2a self)
{
    return (self->encodedValue[6] & 0x7f);
}

void
CP56Time2a_setYear(CP56Time2a self, int value)
{
    self->encodedValue[6] = (uint8_t) ((self->encodedValue[6] & 0x80) + (value & 0x7f));
}

bool
CP56Time2a_isSummerTime(CP56Time2a self)
{
    return ((self->encodedValue[3] & 0x80) != 0);
}

void
CP56Time2a_setSummerTime(CP56Time2a self, bool value)
{
    if (value)
        self->encodedValue[3] |= 0x80;
    else
        self->encodedValue[3] &= 0x7f;
}

bool
CP56Time2a_isInvalid(CP56Time2a self)
{
    return isInvalid(self->encodedValue);
}

void
CP56Time2a_setInvalid(CP56Time2a self, bool value)
{
    setInvalid(self->encodedValue, value);
}

bool
CP56Time2a_isSubstituted(CP56Time2a self)
{
    return isSubstituted(self->encodedValue);
}

void
CP56Time2a_setSubstituted(CP56Time2a self, bool value)
{
    setSubstituted(self->encodedValue, value);
}

uint8_t*
CP56Time2a_getEncodedValue(CP56Time2a self)
{
    return self->encodedValue;
}
