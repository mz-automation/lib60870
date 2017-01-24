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

#include "frame.h"
#include "iec60870_common.h"

struct sFrame {
    FrameVFT virtualFunctionTable;
};


void
Frame_destroy(Frame self)
{
    self->virtualFunctionTable->destroy(self);
}

void
Frame_resetFrame(Frame self)
{
    self->virtualFunctionTable->resetFrame(self);
}

void
Frame_setNextByte(Frame self, uint8_t byte)
{
    self->virtualFunctionTable->setNextByte(self, byte);
}

void
Frame_appendBytes(Frame self, uint8_t* bytes, int numberOfBytes)
{
    self->virtualFunctionTable->appendBytes(self, bytes, numberOfBytes);
}

int
Frame_getMsgSize(Frame self)
{
    return self->virtualFunctionTable->getMsgSize(self);
}

uint8_t*
Frame_getBuffer(Frame self)
{
    return self->virtualFunctionTable->getBuffer(self);
}


int
Frame_getSpaceLeft(Frame self)
{
    return self->virtualFunctionTable->getSpaceLeft(self);
}
