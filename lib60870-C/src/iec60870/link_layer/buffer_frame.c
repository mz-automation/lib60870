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

#include <stdlib.h>
#include <stdint.h>

#include "frame.h"
#include "buffer_frame.h"

static struct sFrameVFT bufferFrameVFT = {
        BufferFrame_destroy,
        BufferFrame_resetFrame,
        BufferFrame_setNextByte,
        BufferFrame_appendBytes,
        BufferFrame_getMsgSize,
        BufferFrame_getBuffer,
        BufferFrame_getSpaceLeft
};

Frame
BufferFrame_initialize(BufferFrame self, uint8_t* buffer, int startSize)
{
    self->virtualFunctionTable = &bufferFrameVFT;
    self->buffer = buffer;

    self->startSize = startSize;
    self->msgSize = startSize;
    self->isUsed = false;

    return (Frame) self;
}

void
BufferFrame_destroy(Frame super)
{
    BufferFrame self = (BufferFrame) super;

    self->isUsed = false;
}

void
BufferFrame_resetFrame(Frame super)
{
    BufferFrame self = (BufferFrame) super;

    self->msgSize = self->startSize;
}

void
BufferFrame_setNextByte(Frame super, uint8_t byte)
{
    BufferFrame self = (BufferFrame) super;

    self->buffer[self->msgSize++] = byte;
}

void
BufferFrame_appendBytes(Frame super, const uint8_t* bytes, int numberOfBytes)
{
    BufferFrame self = (BufferFrame) super;

    int i;

    uint8_t* target = self->buffer + self->msgSize;

    for (i = 0; i < numberOfBytes; i++)
        target[i] = bytes[i];

    self->msgSize += numberOfBytes;
}

int
BufferFrame_getMsgSize(Frame super)
{
    BufferFrame self = (BufferFrame) super;

    return self->msgSize;
}

uint8_t*
BufferFrame_getBuffer(Frame super)
{
    BufferFrame self = (BufferFrame) super;

    return self->buffer;
}

int
BufferFrame_getSpaceLeft(Frame super)
{
    BufferFrame self = (BufferFrame) super;

    return ((self->startSize) - self->msgSize);
}

bool
BufferFrame_isUsed(BufferFrame self)
{
    return self->isUsed;
}

void
BufferFrame_markAsUsed(BufferFrame self)
{
    self->isUsed = true;
}
