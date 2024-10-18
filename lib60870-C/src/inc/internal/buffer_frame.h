/*
 *  Copyright 2017-2022 Michael Zillgith
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

#ifndef SRC_IEC60870_T104_BUFFER_FRAME_H_
#define SRC_IEC60870_T104_BUFFER_FRAME_H_

#include "frame.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sBufferFrame {
    FrameVFT virtualFunctionTable;

    uint8_t* buffer;
    int msgSize;
    int startSize;
    bool isUsed;
};

typedef struct sBufferFrame* BufferFrame;

Frame
BufferFrame_initialize(BufferFrame self, uint8_t* buffer, int startSize);

void
BufferFrame_destroy(Frame super);

void
BufferFrame_resetFrame(Frame super);

void
BufferFrame_setNextByte(Frame super, uint8_t byte);

void
BufferFrame_appendBytes(Frame super, const uint8_t* bytes, int numberOfBytes);

int
BufferFrame_getMsgSize(Frame super);

uint8_t*
BufferFrame_getBuffer(Frame super);

int
BufferFrame_getSpaceLeft(Frame super);

bool
BufferFrame_isUsed(BufferFrame self);

void
BufferFrame_markAsUsed(BufferFrame self);

#ifdef __cplusplus
}
#endif

#endif /* SRC_IEC60870_T104_BUFFER_FRAME_H_ */
