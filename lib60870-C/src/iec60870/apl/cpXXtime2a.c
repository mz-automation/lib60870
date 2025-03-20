/*
 *  cpXXtime2a.c
 *
 *  Implementation of the types CP16Time2a, CP24Time2a and CP56Time2a
 *
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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "lib_memory.h"

#include "iec60870_common.h"
#include "apl_types_internal.h"

/**********************************
 *  CP16Time2a type
 **********************************/

bool
CP16Time2a_getFromBuffer (CP16Time2a self, const uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 2)
        return false;

    int i;

    for (i = 0; i < 2; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP16Time2a_getEplapsedTimeInMs(const CP16Time2a self)
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
getMillisecond(const uint8_t* encodedValue)
{
    return (encodedValue[0] + (encodedValue[1] * 0x100)) % 1000;
}

static void
setMillisecond(uint8_t* encodedValue, int value)
{
    int millies = encodedValue[0] + (encodedValue[1] * 0x100);

    /* erase sub-second part */
    millies = millies - (millies % 1000);

    millies = millies + value;

    encodedValue[0] = (uint8_t) (millies & 0xff);
    encodedValue[1] = (uint8_t) ((millies / 0x100) & 0xff);
}

static int
getSecond(const uint8_t* encodedValue)
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
getMinute(const uint8_t* encodedValue)
{
    return (encodedValue[2] & 0x3f);
}

static void
setMinute(uint8_t* encodedValue, int value)
{
    encodedValue[2] = (uint8_t) ((encodedValue[2] & 0xc0) | (value & 0x3f));
}

static bool
isInvalid(const uint8_t* encodedValue)
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
isSubstituted(const uint8_t* encodedValue)
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
CP24Time2a_getFromBuffer (CP24Time2a self, const uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 3)
        return false;

    int i;

    for (i = 0; i < 3; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP24Time2a_getMillisecond(const CP24Time2a self)
{
    return getMillisecond(self->encodedValue);
}

void
CP24Time2a_setMillisecond(CP24Time2a self, int value)
{
    setMillisecond(self->encodedValue, value);
}

int
CP24Time2a_getSecond(const CP24Time2a self)
{
    return getSecond(self->encodedValue);
}

void
CP24Time2a_setSecond(CP24Time2a self, int value)
{
    setSecond(self->encodedValue, value);
}

int
CP24Time2a_getMinute(const CP24Time2a self)
{
    return getMinute(self->encodedValue);
}

void
CP24Time2a_setMinute(CP24Time2a self, int value)
{
    setMinute(self->encodedValue, value);
}

bool
CP24Time2a_isInvalid(const CP24Time2a self)
{
    return isInvalid(self->encodedValue);
}

void
CP24Time2a_setInvalid(CP24Time2a self, bool value)
{
    setInvalid(self->encodedValue, value);
}

bool
CP24Time2a_isSubstituted(const CP24Time2a self)
{
    return isSubstituted(self->encodedValue);
}

void
CP24Time2a_setSubstituted(CP24Time2a self, bool value)
{
    setSubstituted(self->encodedValue, value);
}

#if 0
/* function found here: http://stackoverflow.com/questions/530519/stdmktime-and-timezone-info */
time_t my_timegm(register struct tm * t)
/* struct tm to seconds since Unix epoch */
{
    register long year;
    register time_t result;
#define MONTHSPERYEAR   12      /* months per calendar year */
    static const int cumdays[MONTHSPERYEAR] =
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    /*@ +matchanyintegral @*/
    year = 1900 + t->tm_year + t->tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + cumdays[t->tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
        (t->tm_mon % MONTHSPERYEAR) < 2)
        result--;
    result += t->tm_mday - 1;
    result *= 24;
    result += t->tm_hour;
    result *= 60;
    result += t->tm_min;
    result *= 60;
    result += t->tm_sec;
    if (t->tm_isdst == 1)
        result -= 3600;
    /*@ -matchanyintegral @*/
    return (result);
}
#endif

/* Conversion from UTC date to second, unsigned 32-bit Unix epoch version.
 * Written by François Grieu, 2015-07-21; public domain.
 *
 * my_mktime  converts from  struct tm  UTC to non-leap seconds since
 * 00:00:00 on the first UTC day of year 1970 (fixed).
 * It works from 1970 to 2105 inclusive. It strives to be compatible
 * with C compilers supporting // comments and claiming C89 conformance.
 *
 * input:   Pointer to a  struct tm  with field tm_year, tm_mon, tm_mday,
 *          tm_hour, tm_min, tm_sec set per  mktime  convention; thus
 *          - tm_year is year minus 1900
 *          - tm_mon is [0..11] for January to December, but [-2..14]
 *            works for November of previous year to February of next year
 *          - tm_mday, tm_hour, tm_min, tm_sec similarly can be offset to
 *            the full range [-32767 to 32768], as long as the combination
 *            with tm_year gives a result within years [1970..2105], and
 *            tm_year>0.
 * output:  Number of non-leap seconds since beginning of the first UTC
 *          day of year 1970, as an unsigned at-least-32-bit integer.
 *          The input is not changed (in particular, fields tm_wday,
 *          tm_yday, and tm_isdst are unchanged and ignored).
 */
static time_t
my_mktime(const struct tm * ptm)
{
    int m, y = ptm->tm_year;

    if ((m = ptm->tm_mon) < 2) {
        m += 12;
        --y;
    }

    return ((((time_t) (y - 69) * 365u + y / 4 - y / 100 * 3 / 4 + (m + 2) * 153 / 5 - 446 +
            ptm->tm_mday) * 24u + ptm->tm_hour) * 60u + ptm->tm_min) * 60u + ptm->tm_sec;
}

/**********************************
 *  CP32Time2a type
 **********************************/

CP32Time2a
CP32Time2a_create(CP32Time2a self)
{
    if (self == NULL)
        self = (CP32Time2a) GLOBAL_CALLOC(1, sizeof(struct sCP32Time2a));
    else
        memset (self, 0, sizeof(struct sCP32Time2a));

    return self;
}

bool
CP32Time2a_getFromBuffer (CP32Time2a self, const uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 4)
        return false;

    int i;

    for (i = 0; i < 4; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP32Time2a_getMillisecond(const CP32Time2a self)
{
    return (self->encodedValue[0] + (self->encodedValue[1] * 0x100)) % 1000;
}

void
CP32Time2a_setMillisecond(CP32Time2a self, int value)
{
    int millies = (CP32Time2a_getSecond(self) * 1000) + value;

    self->encodedValue[0] = (uint8_t) (millies & 0xff);
    self->encodedValue[1] = (uint8_t) ((millies / 0x100) & 0xff);
}

int
CP32Time2a_getSecond(const CP32Time2a self)
{
    return getSecond(self->encodedValue);
}

void
CP32Time2a_setSecond(CP32Time2a self, int value)
{
    setSecond(self->encodedValue, value);
}

int
CP32Time2a_getMinute(const CP32Time2a self)
{
    return getMinute(self->encodedValue);
}

void
CP32Time2a_setMinute(CP32Time2a self, int value)
{
    setMinute(self->encodedValue, value);
}

bool
CP32Time2a_isInvalid(const CP32Time2a self)
{
    return isInvalid(self->encodedValue);
}

void
CP32Time2a_setInvalid(CP32Time2a self, bool value)
{
    setInvalid(self->encodedValue, value);
}

bool
CP32Time2a_isSubstituted(const CP32Time2a self)
{
    return isSubstituted(self->encodedValue);
}

void
CP32Time2a_setSubstituted(CP32Time2a self, bool value)
{
    setSubstituted(self->encodedValue, value);
}

int
CP32Time2a_getHour(const CP32Time2a self)
{
    return (self->encodedValue[3] & 0x1f);
}

void
CP32Time2a_setHour(CP32Time2a self, int value)
{
    self->encodedValue[3] = (uint8_t) ((self->encodedValue[3] & 0xe0) | (value & 0x1f));
}

bool
CP32Time2a_isSummerTime(const CP32Time2a self)
{
    return ((self->encodedValue[3] & 0x80) != 0);
}

void
CP32Time2a_setSummerTime(CP32Time2a self, bool value)
{
    if (value)
        self->encodedValue[3] |= 0x80;
    else
        self->encodedValue[3] &= 0x7f;
}

void
CP32Time2a_setFromMsTimestamp(CP32Time2a self, uint64_t timestamp)
{
    memset(self->encodedValue, 0, 4);

    time_t timeVal = timestamp / 1000;

    int msPart = timestamp % 1000;

    struct tm tmTime;

#ifdef _WIN32
    gmtime_s(&tmTime, &timeVal);
#else
    gmtime_r(&timeVal, &tmTime);
#endif

    CP32Time2a_setSecond(self, tmTime.tm_sec);

    CP32Time2a_setMillisecond(self, msPart);

    CP32Time2a_setMinute(self, tmTime.tm_min);

    CP32Time2a_setHour(self, tmTime.tm_hour);
}

uint8_t*
CP32Time2a_getEncodedValue(CP32Time2a self)
{
    return self->encodedValue;
}

/**********************************
 *  CP56Time2a type
 **********************************/

CP56Time2a
CP56Time2a_createFromMsTimestamp(CP56Time2a self, uint64_t timestamp)
{
    if (self == NULL)
        self = (CP56Time2a) GLOBAL_CALLOC(1, sizeof(struct sCP56Time2a));
    else
        memset (self, 0, sizeof(struct sCP56Time2a));

    if (self != NULL)
        CP56Time2a_setFromMsTimestamp(self, timestamp);

    return self;
}

void
CP56Time2a_setFromMsTimestamp(CP56Time2a self, uint64_t timestamp)
{
    memset(self->encodedValue, 0, 7);

    time_t timeVal = timestamp / 1000;
    int msPart = timestamp % 1000;

    struct tm tmTime;

    /* TODO replace with portable implementation */
#ifdef _WIN32
	gmtime_s(&tmTime, &timeVal);
#else
    gmtime_r(&timeVal, &tmTime);
#endif

    CP56Time2a_setMillisecond(self, msPart);

    CP56Time2a_setSecond(self, tmTime.tm_sec);

    CP56Time2a_setMinute(self, tmTime.tm_min);

    CP56Time2a_setHour(self, tmTime.tm_hour);

    CP56Time2a_setDayOfMonth(self, tmTime.tm_mday);

    /* set day of week to 0 = not present */
    CP56Time2a_setDayOfWeek(self, 0);

    CP56Time2a_setMonth(self, tmTime.tm_mon + 1);

    CP56Time2a_setYear(self, tmTime.tm_year);
}

uint64_t
CP56Time2a_toMsTimestamp(const CP56Time2a self)
{
    struct tm tmTime;

    tmTime.tm_sec = CP56Time2a_getSecond(self);
    tmTime.tm_min = CP56Time2a_getMinute(self);
    tmTime.tm_hour = CP56Time2a_getHour(self);
    tmTime.tm_mday = CP56Time2a_getDayOfMonth(self);
    tmTime.tm_mon = CP56Time2a_getMonth(self) - 1;
    tmTime.tm_year = CP56Time2a_getYear(self) + 100;

    time_t timestamp = my_mktime(&tmTime);

    uint64_t msTimestamp = ((uint64_t) (timestamp * (uint64_t) 1000)) + CP56Time2a_getMillisecond(self);

    return msTimestamp;
}

/* private */ bool
CP56Time2a_getFromBuffer(CP56Time2a self, const uint8_t* msg, int msgSize, int startIndex)
{
    if (msgSize < startIndex + 7)
        return false;

    int i;

    for (i = 0; i < 7; i++)
        self->encodedValue[i] = msg[startIndex + i];

    return true;
}

int
CP56Time2a_getMillisecond(const CP56Time2a self)
{
    return getMillisecond(self->encodedValue);
}

void
CP56Time2a_setMillisecond(CP56Time2a self, int value)
{
    setMillisecond(self->encodedValue, value);
}

int
CP56Time2a_getSecond(const CP56Time2a self)
{
    return getSecond(self->encodedValue);
}

void
CP56Time2a_setSecond(CP56Time2a self, int value)
{
    setSecond(self->encodedValue, value);
}

int
CP56Time2a_getMinute(const CP56Time2a self)
{
    return getMinute(self->encodedValue);
}

void
CP56Time2a_setMinute(CP56Time2a self, int value)
{
    setMinute(self->encodedValue, value);
}

int
CP56Time2a_getHour(const CP56Time2a self)
{
    return (self->encodedValue[3] & 0x1f);
}

void
CP56Time2a_setHour(CP56Time2a self, int value)
{
    self->encodedValue[3] = (uint8_t) ((self->encodedValue[3] & 0xe0) | (value & 0x1f));
}

int
CP56Time2a_getDayOfWeek(const CP56Time2a self)
{
    return ((self->encodedValue[4] & 0xe0) >> 5);
}

void
CP56Time2a_setDayOfWeek(CP56Time2a self, int value)
{
    self->encodedValue[4] = (uint8_t) ((self->encodedValue[4] & 0x1f) | ((value & 0x07) << 5));
}

int
CP56Time2a_getDayOfMonth(const CP56Time2a self)
{
    return (self->encodedValue[4] & 0x1f);
}

void
CP56Time2a_setDayOfMonth(CP56Time2a self, int value)
{
    self->encodedValue[4] = (uint8_t) ((self->encodedValue[4] & 0xe0) + (value & 0x1f));
}

int
CP56Time2a_getMonth(const CP56Time2a self)
{
    return (self->encodedValue[5] & 0x0f);
}

void
CP56Time2a_setMonth(CP56Time2a self, int value)
{
    self->encodedValue[5] = (uint8_t) ((self->encodedValue[5] & 0xf0) + (value & 0x0f));
}

int
CP56Time2a_getYear(const CP56Time2a self)
{
    return (self->encodedValue[6] & 0x7f);
}

void
CP56Time2a_setYear(CP56Time2a self, int value)
{
    value = value % 100;

    self->encodedValue[6] = (uint8_t) ((self->encodedValue[6] & 0x80) + (value & 0x7f));
}

bool
CP56Time2a_isSummerTime(const CP56Time2a self)
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
CP56Time2a_isInvalid(const CP56Time2a self)
{
    return isInvalid(self->encodedValue);
}

void
CP56Time2a_setInvalid(CP56Time2a self, bool value)
{
    setInvalid(self->encodedValue, value);
}

bool
CP56Time2a_isSubstituted(const CP56Time2a self)
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
