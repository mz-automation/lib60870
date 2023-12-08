/*
 *  apl_types_internal.h
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

#ifndef SRC_INC_APL_TYPES_INTERNAL_H_
#define SRC_INC_APL_TYPES_INTERNAL_H_

#include <stdint.h>
#include <stdbool.h>

#include "frame.h"
#include "iec60870_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void
CS101_ASDU_encode(CS101_ASDU self, Frame frame);

bool
CP16Time2a_getFromBuffer (CP16Time2a self, const uint8_t* msg, int msgSize, int startIndex);

uint8_t*
CP16Time2a_getEncodedValue(CP16Time2a self);

bool
CP24Time2a_getFromBuffer (CP24Time2a self, const uint8_t* msg, int msgSize, int startIndex);

bool
CP32Time2a_getFromBuffer (CP32Time2a self, const uint8_t* msg, int msgSize, int startIndex);

uint8_t*
CP32Time2a_getEncodedValue(CP32Time2a self);

bool
CP56Time2a_getFromBuffer (CP56Time2a self, const uint8_t* msg, int msgSize, int startIndex);

uint8_t*
CP56Time2a_getEncodedValue(CP56Time2a self);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_APL_TYPES_INTERNAL_H_ */
