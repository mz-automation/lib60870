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

#ifndef SRC_INC_FRAME_H_
#define SRC_INC_FRAME_H_

#include <stdint.h>

#include "iec60870_common.h"

typedef struct sFrameVFT* FrameVFT;

struct sFrameVFT {
    void (*destroy)(Frame self);
    void (*resetFrame)(Frame self);
    void (*setNextByte)(Frame self, uint8_t byte);
    void (*appendBytes)(Frame self, const uint8_t* bytes, int numberOfBytes);
    int (*getMsgSize)(Frame self);
    uint8_t* (*getBuffer)(Frame self);
    int (*getSpaceLeft)(Frame self);
};

#endif /* SRC_INC_FRAME_H_ */
