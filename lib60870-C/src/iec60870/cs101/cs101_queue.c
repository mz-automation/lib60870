/*
 *  cs101_queue.c
 *
 *  Copyright 2017-2018 MZ Automation GmbH
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
#include <stdio.h>
#include <string.h>
#include "buffer_frame.h"
#include "lib_memory.h"
#include "lib60870_config.h"
#include "lib60870_internal.h"
#include "apl_types_internal.h"
#include "cs101_queue.h"

/********************************************
 * CS101_Queue
 ********************************************/

void
CS101_Queue_initialize(CS101_Queue self, int maxQueueSize)
{
    self->entryCounter = 0;
    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    self->size = maxQueueSize;

    BufferFrame_initialize(&(self->encodeFrame), NULL, 0);

#if (CS101_MAX_QUEUE_SIZE == -1)
    int queueSize = maxQueueSize;

    if (maxQueueSize == -1)
        queueSize = 100;

    self->elements = (CS101_QueueElement) GLOBAL_CALLOC(queueSize, sizeof(struct sCS101_QueueElement));

    self->size = queueSize;
#else
    self->size = CS101_MAX_QUEUE_SIZE;
#endif


#if (CONFIG_USE_SEMAPHORES == 1)
    self->queueLock = Semaphore_create(1);
#endif
}

void
CS101_Queue_dispose(CS101_Queue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_destroy(self->queueLock);
#endif

#if (CS101_MAX_QUEUE_SIZE == -1)
    GLOBAL_FREEMEM(self->elements);
#endif
}

void
CS101_Queue_lock(CS101_Queue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif
}

void
CS101_Queue_unlock(CS101_Queue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

void
CS101_Queue_enqueue(CS101_Queue self, CS101_ASDU asdu)
{
    CS101_Queue_lock(self);

    int nextIndex;
    bool removeEntry = false;

    if (self->entryCounter == 0) {
        self->firstMsgIndex = 0;
        nextIndex = 0;
    }
    else
        nextIndex = self->lastMsgIndex + 1;

    if (nextIndex == self->size)
        nextIndex = 0;

    if (self->entryCounter == self->size)
        removeEntry = true;

    if (removeEntry == false) {
        DEBUG_PRINT("add entry (nextIndex:%i)\n", nextIndex);
        self->lastMsgIndex = nextIndex;
        self->entryCounter++;
    }
    else {
        DEBUG_PRINT("add entry (nextIndex:%i) -> remove oldest\n", nextIndex);

        /* remove oldest entry */
        self->lastMsgIndex = nextIndex;

        int firstIndex = nextIndex + 1;

        if (firstIndex == self->size)
            firstIndex = 0;

        self->firstMsgIndex = firstIndex;
    }

    self->encodeFrame.buffer = self->elements[nextIndex].buffer;
    self->encodeFrame.startSize = 0;
    self->encodeFrame.msgSize = 0;

    CS101_ASDU_encode(asdu, (Frame)&(self->encodeFrame));

    int srcSize = self->encodeFrame.msgSize;
    self->elements[nextIndex].size = srcSize;

    DEBUG_PRINT("Events in FIFO: %i (first: %i, last: %i)\n", self->entryCounter,
            self->firstMsgIndex, self->lastMsgIndex);

    CS101_Queue_unlock(self);
}


/*
 * NOTE: Locking has to be done by caller!
 */
Frame
CS101_Queue_dequeue(CS101_Queue self, Frame resultStorage)
{
    Frame frame = NULL;

    if (self->entryCounter != 0) {

        if (resultStorage) {
            frame = resultStorage;

            int currentIndex = self->firstMsgIndex;

            Frame_appendBytes(frame, self->elements[currentIndex].buffer, self->elements[currentIndex].size);

            self->firstMsgIndex = (currentIndex + 1) % self->size;
            self->entryCounter--;
        }
    }

    return frame;
}

bool
CS101_Queue_isFull(CS101_Queue self)
{
   return (self->entryCounter == self->size);
}

bool
CS101_Queue_isEmpty(CS101_Queue self)
{
   return (self->entryCounter == 0);
}


void
CS101_Queue_flush(CS101_Queue self)
{
    CS101_Queue_lock(self);
    self->entryCounter = 0;
    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    CS101_Queue_unlock(self);
}


/********************************************
 * END CS101_Queue
 ********************************************/
