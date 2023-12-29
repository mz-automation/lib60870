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

#include "cs104_frame.h"

#include <stdlib.h>
#include <stdint.h>

#include "frame.h"
#include "lib60870_internal.h"
#include "lib_memory.h"

#ifndef CONFIG_LIB60870_STATIC_FRAMES
#define CONFIG_LIB60870_STATIC_FRAMES 0
#endif


struct sT104Frame {
    FrameVFT virtualFunctionTable;

    uint8_t buffer[256];
    int msgSize;

#if (CONFIG_LIB60870_STATIC_FRAMES == 1)
    /* TODO move to base class? */
    uint8_t allocated;
#endif
};

static struct sFrameVFT t104FrameVFT = {
        T104Frame_destroy,
        T104Frame_resetFrame,
        T104Frame_setNextByte,
        T104Frame_appendBytes,
        T104Frame_getMsgSize,
        T104Frame_getBuffer,
        T104Frame_getSpaceLeft
};

#if (CONFIG_LIB60870_STATIC_FRAMES == 1)

static int staticFramesInitialized = 0;

#ifndef CONFIG_LIB60870_MAX_FRAMES
#define CONFIG_LIB60870_MAX_FRAMES 10
#endif

struct sT104Frame staticFrames[CONFIG_LIB60870_MAX_FRAMES];

static void
initializeFrames(void)
{
    int i;

    for (i = 0; i < CONFIG_LIB60870_MAX_FRAMES; i++) {
        staticFrames[i].virtualFunctionTable = &t104FrameVFT;
        staticFrames[i].allocated = 0;
        staticFrames[i].buffer[0] = 0x68;
    }
}

static T104Frame
getNextFreeFrame(void)
{
    int i;

    for (i = 0; i < CONFIG_LIB60870_MAX_FRAMES; i++) {

        if (staticFrames[i].allocated == 0) {
            staticFrames[i].msgSize = 6;
            staticFrames[i].allocated = 1;

            return &staticFrames[i];
        }
    }

    return NULL;
}

#endif /* (CONFIG_LIB60870_STATIC_FRAMES == 1) */


T104Frame
T104Frame_create()
{
#if (CONFIG_LIB60870_STATIC_FRAMES == 1)

    if (staticFramesInitialized == 0) {
        initializeFrames();
        staticFramesInitialized = 1;
    }

    T104Frame self = getNextFreeFrame();

#else
    T104Frame self = (T104Frame) GLOBAL_MALLOC(sizeof(struct sT104Frame));

    if (self != NULL) {

        int i;
        for (i = 0; i < 256; i++)
            self->buffer[i] = 0;

        self->virtualFunctionTable = &t104FrameVFT;
        self->buffer[0] = 0x68;
        self->msgSize = 6;
    }
#endif

    return self;
}

void
T104Frame_destroy(Frame super)
{
    T104Frame self = (T104Frame) super;

#if (CONFIG_LIB60870_STATIC_FRAMES == 1)
    self->allocated = 0;
#else
    GLOBAL_FREEMEM(self);
#endif
}

void
T104Frame_resetFrame(Frame super)
{
    T104Frame self = (T104Frame) super;

    self->msgSize = 6;
}

void
T104Frame_prepareToSend(T104Frame self, int sendCounter, int receiveCounter)
{
    uint8_t* buffer = self->buffer;

    buffer[1] = (uint8_t) (self->msgSize - 2);

    buffer[2] = (uint8_t) ((sendCounter % 128) * 2);
    buffer[3] = (uint8_t) (sendCounter / 128);

    buffer[4] = (uint8_t) ((receiveCounter % 128) * 2);
    buffer[5] = (uint8_t) (receiveCounter / 128);
}

void
T104Frame_setNextByte(Frame super, uint8_t byte)
{
    T104Frame self = (T104Frame) super;

    self->buffer[self->msgSize++] = byte;
}

void
T104Frame_appendBytes(Frame super, const uint8_t* bytes, int numberOfBytes)
{
    T104Frame self = (T104Frame) super;

    int i;

    uint8_t* target = self->buffer + self->msgSize;

    for (i = 0; i < numberOfBytes; i++)
        target[i] = bytes[i];

    self->msgSize += numberOfBytes;
}

int
T104Frame_getMsgSize(Frame super)
{
    T104Frame self = (T104Frame) super;

    return self->msgSize;
}

uint8_t*
T104Frame_getBuffer(Frame super)
{
    T104Frame self = (T104Frame) super;

    return self->buffer;
}

int
T104Frame_getSpaceLeft(Frame super)
{
    T104Frame self = (T104Frame) super;

    return (IEC60870_5_104_MAX_ASDU_LENGTH + IEC60870_5_104_APCI_LENGTH - self->msgSize);
}
