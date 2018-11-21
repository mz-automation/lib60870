/*
 *  cs101_queue.h
 *
 *  Copyright 2017 MZ Automation GmbH
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

#ifndef SRC_INC_INTERNAL_CS101_QUEUE_H_
#define SRC_INC_INTERNAL_CS101_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#if ((CONFIG_USE_THREADS == 1) || (CONFIG_USE_SEMAPHORES == 1))
#include "hal_thread.h"
#endif

#ifdef CONFIG_SLAVE_MESSAGE_QUEUE_SIZE
#define CS101_MAX_QUEUE_SIZE CONFIG_SLAVE_MESSAGE_QUEUE_SIZE
#else
#define CS101_MAX_QUEUE_SIZE 10
#endif

typedef struct sCS101_QueueElement* CS101_QueueElement;

struct sCS101_QueueElement {
    uint8_t size;
    uint8_t buffer[256];
};

typedef struct sCS101_Queue* CS101_Queue;

struct sCS101_Queue {

    int size;
    int entryCounter;
    int lastMsgIndex;
    int firstMsgIndex;

    struct sBufferFrame encodeFrame;

#if (CS101_MAX_QUEUE_SIZE == -1)
    struct sCS101_QueueElement* elements;
#else
    struct sCS101_QueueElement elements[CS101_MAX_QUEUE_SIZE];
#endif

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore queueLock;
#endif
};

void
CS101_Queue_initialize(CS101_Queue self, int maxQueueSize);

void
CS101_Queue_dispose(CS101_Queue self);

void
CS101_Queue_lock(CS101_Queue self);

void
CS101_Queue_unlock(CS101_Queue self);

void
CS101_Queue_enqueue(CS101_Queue self, CS101_ASDU asdu);

    /*
     * NOTE: Locking has to be done by caller!
     */
Frame
CS101_Queue_dequeue(CS101_Queue self, Frame resultStorage);

bool
CS101_Queue_isFull(CS101_Queue self);

bool
CS101_Queue_isEmpty(CS101_Queue self);

void
CS101_Queue_flush(CS101_Queue self);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_INTERNAL_CS101_QUEUE_H_ */
