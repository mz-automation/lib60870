/*
 *  Copyright 2016-2025 Michael Zillgith
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

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_frame.h"
#include "cs104_frame.h"
#include "cs104_slave.h"
#include "frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"
#include "linked_list.h"

#include "iec60870_slave.h"
#include "lib60870_config.h"
#include "lib60870_internal.h"

#include "apl_types_internal.h"
#include "cs101_asdu_internal.h"

#if (CONFIG_CS104_SUPPORT_TLS == 1)
#include "tls_socket.h"
#endif

#if ((CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP != 1) &&                                         \
     (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP != 1) &&                                                \
     (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS != 1))
#error Illegal configuration: Define either CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP or CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP or CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS
#endif

typedef enum
{
    M_CON_STATE_STOPPED,            /* only U frames allowed */
    M_CON_STATE_STARTED,            /* U, I, S frames allowed */
    M_CON_STATE_UNCONFIRMED_STOPPED /* only U, S frames allowed */
} MasterConnectionState;

typedef struct sMasterConnection* MasterConnection;

static void
MasterConnection_close(MasterConnection self);

static void
MasterConnection_deactivate(MasterConnection self);

static bool
MasterConnection_hasUnconfirmedMessages(MasterConnection self);

static void
MasterConnection_activate(MasterConnection self);

static bool
MasterConnection_isActive(MasterConnection self);

#define CS104_DEFAULT_PORT 2404

static struct sCS104_APCIParameters defaultConnectionParameters = {
    /* .k = */ 12,
    /* .w = */ 8,
    /* .t0 = */ 10,
    /* .t1 = */ 15,
    /* .t2 = */ 10,
    /* .t3 = */ 20};

static struct sCS101_AppLayerParameters defaultAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249};

typedef struct
{
    uint8_t msg[256];
    int msgSize;
} FrameBuffer;

typedef enum
{
    QUEUE_ENTRY_STATE_NOT_USED_OR_CONFIRMED,
    QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION,
    QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED
} QueueEntryState;

/***************************************************
 * MessageQueue
 ***************************************************/

struct sMessageQueueEntryInfo
{
    uint64_t entryId;
    unsigned int entryState : 2;
    unsigned int size : 8;
};

struct sMessageQueue
{
    int size;         /* size of buffer in bytes */
    int entryCounter; /* number of messages (ASDU) in the queue */

    uint8_t* firstEntry;        /* first entry in FIFO */
    uint8_t* lastEntry;         /* last entry in FIFO */
    uint8_t* lastInBufferEntry; /* entry with highest address in FIFO buffer */

    uint64_t entryId; /* ID of next entry; will be increased by one for each new entry */
    uint8_t* buffer;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore queueLock;
#endif
};

typedef struct sMessageQueue* MessageQueue;

static void
MessageQueue_initialize(MessageQueue self)
{
    self->entryCounter = 0;

    self->firstEntry = NULL;
    self->lastEntry = NULL;
    self->lastInBufferEntry = NULL;
    self->entryId = 1;
}

static MessageQueue
MessageQueue_create(int maxQueueSize)
{
    MessageQueue self = (MessageQueue)GLOBAL_MALLOC(sizeof(struct sMessageQueue));

    if (self)
    {

        self->size = maxQueueSize * (sizeof(struct sMessageQueueEntryInfo) + 256);

        DEBUG_PRINT("CS104 SLAVE: event queue buffer size: %i bytes\n", self->size);

        self->buffer = (uint8_t*)GLOBAL_CALLOC(1, self->size);

#if (CONFIG_USE_SEMAPHORES == 1)
        self->queueLock = Semaphore_create(1);
#endif

        MessageQueue_initialize(self);
    }

    return self;
}

static void
MessageQueue_destroy(MessageQueue self)
{
    if (self != NULL)
    {

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_destroy(self->queueLock);
#endif

        GLOBAL_FREEMEM(self->buffer);
        GLOBAL_FREEMEM(self);
    }
}

static void
MessageQueue_lock(MessageQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif
}

static void
MessageQueue_unlock(MessageQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static int
MessageQueue_getEntryCount(MessageQueue self)
{
    int count = 0;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    count = self->entryCounter;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif

    return count;
}

static int
MessageQueue_countEntriesUntilEndOfBuffer(MessageQueue self, uint8_t* firstEntry)
{
    int count = 0;

    uint8_t* entryPtr = firstEntry;

    while (entryPtr)
    {

        struct sMessageQueueEntryInfo entryInfo;

        memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

        count++;

        /* move to next entry */
        if (entryPtr == self->lastInBufferEntry)
            break;
        else
            entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
    }

    return count;
}

/**
 * Add an ASDU to the queue. When queue is full, override oldest entry.
 */
static void
MessageQueue_enqueueASDU(MessageQueue self, CS101_ASDU asdu)
{
    int asduSize = asdu->asduHeaderLength + asdu->payloadSize;

    if (asduSize > 256 - IEC60870_5_104_APCI_LENGTH)
    {
        DEBUG_PRINT("CS104 SLAVE: ASDU too large!\n");
        return;
    }

    int entrySize = sizeof(struct sMessageQueueEntryInfo) + asduSize;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    struct sMessageQueueEntryInfo entryInfo;

    uint8_t* nextMsgPtr;

    if (self->entryCounter == 0)
    {
        self->firstEntry = self->buffer;
        self->lastInBufferEntry = self->firstEntry;
        nextMsgPtr = self->buffer;
    }
    else
    {
        memcpy(&entryInfo, self->lastEntry, sizeof(struct sMessageQueueEntryInfo));
        nextMsgPtr = self->lastEntry + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;

        /* Check if ASDU fits into the buffer */
        if (nextMsgPtr + entrySize > self->buffer + self->size)
        {
            /* remove all entries from last entry to end of buffer */
            if (nextMsgPtr <= self->firstEntry)
            {
                self->entryCounter -= MessageQueue_countEntriesUntilEndOfBuffer(self, self->firstEntry);
                self->firstEntry = self->buffer;
            }

            /* put new message at beginning of buffer */
            nextMsgPtr = self->buffer;

            if (self->lastEntry > self->firstEntry)
                self->lastInBufferEntry = self->lastEntry;
        }

        if (nextMsgPtr <= self->firstEntry)
        {
            /* remove old entries until we have enough space for the new ASDU */
            while ((nextMsgPtr + entrySize > self->firstEntry) && (self->entryCounter > 0))
            {
                self->entryCounter--;

                if (self->firstEntry == self->lastInBufferEntry)
                {
                    self->firstEntry = self->buffer;
                    self->lastInBufferEntry = nextMsgPtr;
                    break;
                }
                else
                {
                    memcpy(&entryInfo, self->firstEntry, sizeof(struct sMessageQueueEntryInfo));
                    self->firstEntry = self->firstEntry + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
                }
            }
        }
    }

    self->lastEntry = nextMsgPtr;

    if (self->lastEntry > self->lastInBufferEntry)
        self->lastInBufferEntry = self->lastEntry;

    self->entryCounter++;

    struct sBufferFrame bufferFrame;

    Frame frame = BufferFrame_initialize(&bufferFrame, nextMsgPtr + sizeof(struct sMessageQueueEntryInfo), 0);
    CS101_ASDU_encode(asdu, frame);

    entryInfo.size = asduSize;
    entryInfo.entryId = self->entryId++;
    entryInfo.entryState = QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION;

    memcpy(nextMsgPtr, &entryInfo, sizeof(struct sMessageQueueEntryInfo));

    DEBUG_PRINT("CS104 SLAVE: ASDUs in FIFO: %i (new(size=%i/%i): %p, first: %p, last: %p lastInBuf: %p)\n",
                self->entryCounter, entrySize, asduSize, nextMsgPtr, self->firstEntry, self->lastEntry,
                self->lastInBufferEntry);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static bool
MessageQueue_isAsduAvailable(MessageQueue self)
{
    bool retVal = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    if (self->entryCounter != 0)
    {
        uint8_t* entryPtr = self->firstEntry;

        struct sMessageQueueEntryInfo entryInfo;

        memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

        while (entryInfo.entryState != QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION)
        {
            if (entryPtr == self->lastEntry)
                break;

            /* move to next entry */
            if (entryPtr == self->lastInBufferEntry)
                entryPtr = self->buffer;
            else
                entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;

            memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));
        }

        if (entryInfo.entryState == QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION)
        {
            retVal = true;
        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif

    return retVal;
}

static uint8_t*
MessageQueue_getNextWaitingASDU(MessageQueue self, uint64_t* entryId, uint8_t** queueEntry, int* size)
{
    uint8_t* buffer = NULL;

    if (self->entryCounter != 0)
    {
        uint8_t* entryPtr = self->firstEntry;

        struct sMessageQueueEntryInfo entryInfo;

        memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

        while (entryInfo.entryState != QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION)
        {
            if (entryPtr == self->lastEntry)
                break;

            /* move to next entry */
            if (entryPtr == self->lastInBufferEntry)
                entryPtr = self->buffer;
            else
                entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;

            memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));
        }

        if (entryInfo.entryState == QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION)
        {
            *entryId = entryInfo.entryId;
            *queueEntry = entryPtr;
            entryInfo.entryState = QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED;

            memcpy(entryPtr, &entryInfo, sizeof(struct sMessageQueueEntryInfo));

            buffer = entryPtr + sizeof(struct sMessageQueueEntryInfo);
            *size = entryInfo.size;
        }
    }

    return buffer;
}

static bool
MessageQueue_hasUnconfirmedIMessages(MessageQueue self)
{
    bool retVal = false;

    if (self->entryCounter != 0)
    {
        uint8_t* entryPtr = self->firstEntry;

        struct sMessageQueueEntryInfo entryInfo;

        while (entryPtr)
        {
            memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

            if (entryInfo.entryState == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED)
            {
                retVal = true;
                break;
            }

            if (entryPtr == self->lastEntry)
                break;

            /* move to next entry */
            if (entryPtr == self->lastInBufferEntry)
                entryPtr = self->buffer;
            else
                entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
        }
    }

    return retVal;
}

static void
MessageQueue_setWaitingForTransmissionWhenNotConfirmed(MessageQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    if (self->entryCounter != 0)
    {
        uint8_t* entryPtr = self->firstEntry;

        struct sMessageQueueEntryInfo entryInfo;

        while (entryPtr)
        {
            memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

            if (entryInfo.entryState == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED)
            {
                entryInfo.entryState = QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION;
            }

            memcpy(entryPtr, &entryInfo, sizeof(struct sMessageQueueEntryInfo));

            if (entryPtr == self->lastEntry)
                break;

            /* move to next entry */
            if (entryPtr == self->lastInBufferEntry)
                entryPtr = self->buffer;
            else
                entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static void
MessageQueue_releaseAllQueuedASDUs(MessageQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    self->firstEntry = NULL;
    self->lastEntry = NULL;
    self->lastInBufferEntry = NULL;
    self->entryCounter = 0;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static void
removeFirstEntry(MessageQueue self)
{
    if (self->firstEntry == self->lastInBufferEntry)
    {
        if (self->firstEntry == self->lastEntry)
        {
            self->firstEntry = NULL;
            self->lastEntry = NULL;
            self->lastInBufferEntry = NULL;
        }
        else
        {
            self->firstEntry = self->buffer;
            self->lastInBufferEntry = self->lastEntry;
        }
    }
    else
    {
        struct sMessageQueueEntryInfo entryInfo;

        memcpy(&entryInfo, self->firstEntry, sizeof(struct sMessageQueueEntryInfo));
        self->firstEntry = self->firstEntry + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
    }

    self->entryCounter--;
}

static void
MessageQueue_markAsduAsConfirmed(MessageQueue self, uint8_t* queueEntry, uint64_t entryId)
{
    if (self->entryCounter > 0)
    {
        /* entryId plausibility check */
        uint64_t entryIdDiff = self->entryId - 1 - entryId;

        if (entryIdDiff < (unsigned)self->entryCounter)
        {
            struct sMessageQueueEntryInfo entryInfo;
            memcpy(&entryInfo, queueEntry, sizeof(struct sMessageQueueEntryInfo));

            /* check if ASDU is matching */
            if (entryInfo.entryId == entryId)
            {
                entryInfo.entryState = QUEUE_ENTRY_STATE_NOT_USED_OR_CONFIRMED;
                memcpy(queueEntry, &entryInfo, sizeof(struct sMessageQueueEntryInfo));

                if (queueEntry == self->firstEntry)
                {
                    removeFirstEntry(self);
                }
            }
            else
            {
                /* we shouldn't be here - probably bug in queue handling code */
                DEBUG_PRINT("CS104 SLAVE: message queue corrupted\n");
            }
        }
    }
}

/***************************************************
 * HighPriorityASDUQueue
 ***************************************************/

struct sHighPriorityASDUQueue
{
    int size;         /* size of buffer in bytes */
    int entryCounter; /* number of messages (ASDU) in the queue */

    uint8_t* firstEntry;
    uint8_t* lastEntry;
    uint8_t* lastInBufferEntry;

    uint8_t* buffer;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore queueLock;
#endif
};

typedef struct sHighPriorityASDUQueue* HighPriorityASDUQueue;

static void
HighPriorityASDUQueue_initialize(HighPriorityASDUQueue self)
{
    self->entryCounter = 0;

    self->firstEntry = NULL;
    self->lastEntry = NULL;
    self->lastInBufferEntry = NULL;
}

static HighPriorityASDUQueue
HighPriorityASDUQueue_create(int maxQueueSize)
{
    HighPriorityASDUQueue self = (HighPriorityASDUQueue)GLOBAL_MALLOC(sizeof(struct sHighPriorityASDUQueue));

    if (self)
    {
        self->size = maxQueueSize * (sizeof(uint16_t) + 256);

        self->buffer = (uint8_t*)GLOBAL_CALLOC(1, self->size);

#if (CONFIG_USE_SEMAPHORES == 1)
        self->queueLock = Semaphore_create(1);
#endif

        HighPriorityASDUQueue_initialize(self);
    }

    return self;
}

static void
HighPriorityASDUQueue_destroy(HighPriorityASDUQueue self)
{
    if (self)
    {
        if (self->buffer)
            GLOBAL_FREEMEM(self->buffer);

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_destroy(self->queueLock);
#endif

        GLOBAL_FREEMEM(self);
    }
}

static void
HighPriorityASDUQueue_lock(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif
}

static void
HighPriorityASDUQueue_unlock(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static bool
HighPriorityASDUQueue_isAsduAvailable(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    bool retVal;

    if (self->entryCounter > 0)
        retVal = true;
    else
        retVal = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif

    return retVal;
}

static uint8_t*
HighPriorityASDUQueue_getNextASDU(HighPriorityASDUQueue self, int* size)
{
    uint8_t* buffer = NULL;

    if (self->entryCounter > 0)
    {
        self->entryCounter--;

        uint16_t msgSize;

        memcpy(&msgSize, self->firstEntry, 2);
        *size = (int)msgSize;

        buffer = self->firstEntry + 2;

        if (self->entryCounter > 0)
        {
            if (self->firstEntry == self->lastEntry)
            {
                self->firstEntry = NULL;
                self->lastEntry = NULL;
                self->lastInBufferEntry = NULL;
            }
            else
            {
                if (self->firstEntry == self->lastInBufferEntry)
                {
                    self->firstEntry = self->buffer;
                    self->lastInBufferEntry = self->lastEntry;
                }
                else
                {
                    self->firstEntry = self->firstEntry + 2 + msgSize;
                }
            }
        }
    }

    return buffer;
}

/* Depends on ASDU size! */
static bool
HighPriorityASDUQueue_isFull(HighPriorityASDUQueue self)
{
    bool full = false;

    int entrySize = sizeof(uint16_t) + (256 - IEC60870_5_104_APCI_LENGTH);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    uint16_t msgSize;

    uint8_t* nextMsgPtr;

    if (self->entryCounter > 0)
    {
        memcpy(&msgSize, self->lastEntry, sizeof(uint16_t));
        nextMsgPtr = self->lastEntry + sizeof(uint16_t) + msgSize;

        if (nextMsgPtr + entrySize > self->buffer + self->size)
        {
            nextMsgPtr = self->buffer;
        }

        if (nextMsgPtr <= self->firstEntry)
        {
            if (nextMsgPtr + entrySize > self->firstEntry)
            {
                full = true;
            }
        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif

    return full;
}

static bool
HighPriorityASDUQueue_enqueue(HighPriorityASDUQueue self, CS101_ASDU asdu)
{
    int asduSize = asdu->asduHeaderLength + asdu->payloadSize;

    if (asduSize > 256 - IEC60870_5_104_APCI_LENGTH)
    {
        DEBUG_PRINT("CS104 SLAVE: ASDU too large!\n");
        return false;
    }

    int entrySize = sizeof(uint16_t) + asduSize;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    bool enqueued = true;

    uint16_t msgSize;

    uint8_t* nextMsgPtr;

    if (self->entryCounter == 0)
    {
        self->firstEntry = self->buffer;
        self->lastInBufferEntry = self->firstEntry;
        nextMsgPtr = self->buffer;
    }
    else
    {
        memcpy(&msgSize, self->lastEntry, sizeof(uint16_t));
        nextMsgPtr = self->lastEntry + sizeof(uint16_t) + msgSize;
    }

    if (nextMsgPtr + entrySize > self->buffer + self->size)
    {
        nextMsgPtr = self->buffer;
        self->lastInBufferEntry = self->lastEntry;
    }

    if (self->entryCounter > 0)
    {
        if (nextMsgPtr <= self->firstEntry)
        {
            if (nextMsgPtr + entrySize > self->firstEntry)
            {
                enqueued = false;
            }
        }
        else
        {
            self->lastInBufferEntry = nextMsgPtr;
        }
    }

    if (enqueued)
    {
        self->lastEntry = nextMsgPtr;
        self->entryCounter++;

        struct sBufferFrame bufferFrame;

        Frame frame = BufferFrame_initialize(&bufferFrame, nextMsgPtr + sizeof(uint16_t), 0);
        CS101_ASDU_encode(asdu, frame);

        msgSize = asduSize;

        memcpy(nextMsgPtr, &msgSize, sizeof(uint16_t));

        DEBUG_PRINT("CS104 SLAVE: ASDUs in PRIO-FIFO: %i (new(size=%i/%i): %p, first: %p, last: %p lastInBuf: %p)\n",
                    self->entryCounter, entrySize, asduSize, nextMsgPtr, self->firstEntry, self->lastEntry,
                    self->lastInBufferEntry);
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif

    return enqueued;
}

static void
HighPriorityASDUQueue_resetConnectionQueue(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->queueLock);
#endif

    self->firstEntry = NULL;
    self->lastEntry = NULL;
    self->lastInBufferEntry = NULL;
    self->entryCounter = 0;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->queueLock);
#endif
}

static bool
HighPriorityASDUQueue_hasUnconfirmedIMessages(HighPriorityASDUQueue self)
{
    bool retVal = false;

    if (self->entryCounter != 0)
    {
        uint8_t* entryPtr = self->firstEntry;

        struct sMessageQueueEntryInfo entryInfo;

        while (entryPtr)
        {
            memcpy(&entryInfo, entryPtr, sizeof(struct sMessageQueueEntryInfo));

            if (entryInfo.entryState == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED)
            {
                retVal = true;
                break;
            }

            if (entryPtr == self->lastEntry)
                break;

            /* move to next entry */
            if (entryPtr == self->lastInBufferEntry)
                entryPtr = self->buffer;
            else
                entryPtr = entryPtr + sizeof(struct sMessageQueueEntryInfo) + entryInfo.size;
        }
    }

    return retVal;
}

/***************************************************
 * RedundancyGroup
 ***************************************************/

typedef struct sCS104_IPAddress* CS104_IPAddress;

struct sCS104_IPAddress
{
    uint8_t address[16];
    eCS104_IPAddressType type;
};

static void
CS104_IPAddress_setFromString(CS104_IPAddress self, const char* ipAddrStr)
{
    if (strchr(ipAddrStr, '.') != NULL)
    {
        /* parse IPv4 string */
        self->type = IP_ADDRESS_TYPE_IPV4;

        int i;

        for (i = 0; i < 4; i++)
        {
            self->address[i] = (uint8_t)strtoul(ipAddrStr, NULL, 10);

            ipAddrStr = strchr(ipAddrStr, '.');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }
    }
    else
    {
        self->type = IP_ADDRESS_TYPE_IPV6;

        int i;

        for (i = 0; i < 8; i++)
        {
            uint32_t val = strtoul(ipAddrStr, NULL, 16);

            self->address[i * 2] = val / 0x100;
            self->address[i * 2 + 1] = val % 0x100;

            ipAddrStr = strchr(ipAddrStr, ':');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }
    }
}

static bool
CS104_IPAddress_equals(CS104_IPAddress self, CS104_IPAddress other)
{
    if (self->type != other->type)
        return false;

    int size;

    if (self->type == IP_ADDRESS_TYPE_IPV4)
        size = 4;
    else
        size = 16;

    int i;

    for (i = 0; i < size; i++)
    {
        if (self->address[i] != other->address[i])
            return false;
    }

    return true;
}

struct sCS104_RedundancyGroup
{

    char* name; /**< name of the group to be shown in debug messages, or NULL */

    MessageQueue asduQueue;                    /**< low priority ASDU queue and buffer */
    HighPriorityASDUQueue connectionAsduQueue; /**< high priority ASDU queue */

    LinkedList allowedClients;
};

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
static void
CS104_RedundancyGroup_initializeMessageQueues(CS104_RedundancyGroup self, int lowPrioMaxQueueSize,
                                              int highPrioMaxQueueSize)
{
    /* initialized low priority queue */
    if (lowPrioMaxQueueSize < 1)
        lowPrioMaxQueueSize = CONFIG_CS104_MESSAGE_QUEUE_SIZE;

    self->asduQueue = MessageQueue_create(lowPrioMaxQueueSize);

    /* initialize high priority queue */
    if (highPrioMaxQueueSize < 1)
        highPrioMaxQueueSize = CONFIG_CS104_MESSAGE_QUEUE_HIGH_PRIO_SIZE;

    self->connectionAsduQueue = HighPriorityASDUQueue_create(highPrioMaxQueueSize);
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

CS104_RedundancyGroup
CS104_RedundancyGroup_create(const char* name)
{
    CS104_RedundancyGroup self = (CS104_RedundancyGroup)GLOBAL_MALLOC(sizeof(struct sCS104_RedundancyGroup));

    if (self)
    {
        if (name)
            self->name = strdup(name);
        else
            self->name = NULL;

        self->asduQueue = NULL;
        self->connectionAsduQueue = NULL;

        self->allowedClients = NULL;
    }

    return self;
}

void
CS104_RedundancyGroup_destroy(CS104_RedundancyGroup self)
{
    if (self)
    {
        if (self->name)
            GLOBAL_FREEMEM(self->name);

        MessageQueue_destroy(self->asduQueue);
        HighPriorityASDUQueue_destroy(self->connectionAsduQueue);

        if (self->allowedClients)
            LinkedList_destroy(self->allowedClients);

        GLOBAL_FREEMEM(self);
    }
}

void
CS104_RedundancyGroup_addAllowedClient(CS104_RedundancyGroup self, const char* ipAddress)
{
    struct sCS104_IPAddress ipAddr;

    CS104_IPAddress_setFromString(&ipAddr, ipAddress);

    CS104_RedundancyGroup_addAllowedClientEx(self, ipAddr.address, ipAddr.type);
}

void
CS104_RedundancyGroup_addAllowedClientEx(CS104_RedundancyGroup self, const uint8_t* ipAddress,
                                         eCS104_IPAddressType addressType)
{
    if (self->allowedClients == NULL)
        self->allowedClients = LinkedList_create();

    CS104_IPAddress ipAddr = (CS104_IPAddress)GLOBAL_CALLOC(1, sizeof(struct sCS104_IPAddress));

    if (ipAddr)
    {
        ipAddr->type = addressType;

        int size;

        if (addressType == IP_ADDRESS_TYPE_IPV4)
            size = 4;
        else
            size = 16;

        int i;

        for (i = 0; i < size; i++)
            ipAddr->address[i] = ipAddress[i];

        LinkedList_add(self->allowedClients, ipAddr);
    }
    else
    {
        DEBUG_PRINT("CS104_SLAVE: failed to add allowed client");
    }
}

static bool
CS104_RedundancyGroup_matches(CS104_RedundancyGroup self, CS104_IPAddress ipAddress)
{
    if (self->allowedClients == NULL)
        return false;

    LinkedList element = LinkedList_getNext(self->allowedClients);

    while (element)
    {
        CS104_IPAddress allowedAddress = (CS104_IPAddress)LinkedList_getData(element);

        if (CS104_IPAddress_equals(ipAddress, allowedAddress))
            return true;

        element = LinkedList_getNext(element);
    }

    return false;
}

static bool
CS104_RedundancyGroup_isCatchAll(CS104_RedundancyGroup self)
{
    if (self->allowedClients)
        return false;
    else
        return true;
}

/***************************************************
 * Slave
 ***************************************************/

struct sCS104_Slave
{
    CS101_InterrogationHandler interrogationHandler;
    void* interrogationHandlerParameter;

    CS101_CounterInterrogationHandler counterInterrogationHandler;
    void* counterInterrogationHandlerParameter;

    CS101_ReadHandler readHandler;
    void* readHandlerParameter;

    CS101_ClockSynchronizationHandler clockSyncHandler;
    void* clockSyncHandlerParameter;

    CS101_ResetProcessHandler resetProcessHandler;
    void* resetProcessHandlerParameter;

    CS101_DelayAcquisitionHandler delayAcquisitionHandler;
    void* delayAcquisitionHandlerParameter;

    CS101_ASDUHandler asduHandler;
    void* asduHandlerParameter;

    CS104_ConnectionRequestHandler connectionRequestHandler;
    void* connectionRequestHandlerParameter;

    CS104_ConnectionEventHandler connectionEventHandler;
    void* connectionEventHandlerParameter;

    CS104_SlaveRawMessageHandler rawMessageHandler;
    void* rawMessageHandlerParameter;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    TLSConfiguration tlsConfig;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP)
    MessageQueue asduQueue;                    /**< low priority ASDU queue and buffer */
    HighPriorityASDUQueue connectionAsduQueue; /**< high priority ASDU queue */
#endif

    int maxLowPrioQueueSize;
    int maxHighPrioQueueSize;

    int openConnections; /**< number of connected clients */
    MasterConnection
        masterConnections[CONFIG_CS104_MAX_CLIENT_CONNECTIONS]; /**< references to all MasterConnection objects */

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore openConnectionsLock;
#endif

#if (CONFIG_USE_THREADS == 1)
    bool isThreadlessMode;
#endif

    int maxOpenConnections; /**< maximum accepted open client connections */

    struct sCS104_APCIParameters conParameters;

    struct sCS101_AppLayerParameters alParameters;

    bool isStarting;
    bool isRunning;
    bool stopRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore stateLock; /* protect isStarting, isRunning, stopRunning */
#endif

    int tcpPort;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS)
    LinkedList redundancyGroups;
#endif

    CS104_ServerMode serverMode;

    char* localAddress;

#if (CONFIG_USE_THREADS == 1)
    Thread listeningThread;
#endif

    ServerSocket serverSocket;

    LinkedList plugins;
};

typedef struct
{
    uint64_t entryId;    /* required to identify message in server (low-priority) queue */
    uint8_t* queueEntry; /* NULL if ASDU is not from low-priority queue */

    uint64_t sentTime; /* required for T1 timeout */
    int seqNo;
} SentASDUSlave;

struct sMasterConnection
{
    Socket socket;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    TLSSocket tlsSocket;
#endif

    /* can be moved to CS104_Slave struct */
    struct sIMasterConnection iMasterConnection;

    CS104_Slave slave;

    MasterConnectionState state;
    unsigned int isUsed : 1;
    unsigned int isRunning : 1;
    unsigned int timeoutT2Triggered : 1;
    unsigned int waitingForTestFRcon : 1;
    uint16_t maxSentASDUs;  /* k-parameter */
    int16_t oldestSentASDU; /* oldest sent ASDU in k-buffer */
    int16_t newestSentASDU; /* newest sent ASDU in k-buffer */
    uint16_t sendCount;     /* sent messages - sequence counter */
    uint16_t receiveCount;  /* received messages - sequence counter */

    int unconfirmedReceivedIMessages; /* number of unconfirmed messages received */

    /* timeout T2 handling */
    uint64_t lastConfirmationTime; /* timestamp when the last confirmation message (for I messages) was sent */

    uint64_t nextT3Timeout;
    uint64_t nextTestFRConTimeout; /* timeout T1 when waiting for TEST FR con */

    SentASDUSlave* sentASDUs;

#if (CONFIG_USE_THREADS == 1)
    Thread connectionThread;
#endif

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore sentASDUsLock;
    Semaphore stateLock;
#endif

    HandleSet handleSet;

    uint8_t recvBuffer[260];
    int recvBufPos;

    uint8_t sendBuffer[260];

    MessageQueue lowPrioQueue;
    HighPriorityASDUQueue highPrioQueue;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
    CS104_RedundancyGroup redundancyGroup;
#endif
};

static uint8_t STARTDT_CON_MSG[] = {0x68, 0x04, 0x0b, 0x00, 0x00, 0x00};

#define STARTDT_CON_MSG_SIZE 6

static uint8_t STOPDT_CON_MSG[] = {0x68, 0x04, 0x23, 0x00, 0x00, 0x00};

#define STOPDT_CON_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = {0x68, 0x04, 0x83, 0x00, 0x00, 0x00};

#define TESTFR_CON_MSG_SIZE 6

static uint8_t TESTFR_ACT_MSG[] = {0x68, 0x04, 0x43, 0x00, 0x00, 0x00};

#define TESTFR_ACT_MSG_SIZE 6

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
static void
initializeMessageQueues(CS104_Slave self, int lowPrioMaxQueueSize, int highPrioMaxQueueSize)
{
    /* initialized low priority queue */
    if (lowPrioMaxQueueSize < 1)
        lowPrioMaxQueueSize = CONFIG_CS104_MESSAGE_QUEUE_SIZE;

    self->asduQueue = MessageQueue_create(lowPrioMaxQueueSize);

    /* initialize high priority queue */
    if (highPrioMaxQueueSize < 1)
        highPrioMaxQueueSize = CONFIG_CS104_MESSAGE_QUEUE_HIGH_PRIO_SIZE;

    self->connectionAsduQueue = HighPriorityASDUQueue_create(highPrioMaxQueueSize);
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
static void
initializeConnectionSpecificQueues(CS104_Slave self)
{
    int i;

    for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
    {
        self->masterConnections[i]->lowPrioQueue = MessageQueue_create(self->maxLowPrioQueueSize);
        self->masterConnections[i]->highPrioQueue = HighPriorityASDUQueue_create(self->maxHighPrioQueueSize);
    }
}

static void
deleteConnectionSpecificQueues(CS104_Slave self)
{
    int i;

    for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
    {
        if (self->masterConnections[i]->lowPrioQueue)
        {
            MessageQueue_destroy(self->masterConnections[i]->lowPrioQueue);
            self->masterConnections[i]->lowPrioQueue = NULL;
        }

        if (self->masterConnections[i]->highPrioQueue)
        {
            HighPriorityASDUQueue_destroy(self->masterConnections[i]->highPrioQueue);
            self->masterConnections[i]->highPrioQueue = NULL;
        }
    }
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1) */

static bool
isRunning(CS104_Slave self)
{
    bool isRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    isRunning = self->isRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return isRunning;
}

static bool
isStarting(CS104_Slave self)
{
    bool isStarting;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    isStarting = self->isStarting;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return isStarting;
}

static bool
isStopRunningSet(CS104_Slave self)
{
    bool isStopRunningSet;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    isStopRunningSet = self->stopRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return isStopRunningSet;
}

static MasterConnection
MasterConnection_create(CS104_Slave slave);

static CS104_Slave
createSlave(int maxLowPrioQueueSize, int maxHighPrioQueueSize)
{
    CS104_Slave self = (CS104_Slave)GLOBAL_CALLOC(1, sizeof(struct sCS104_Slave));

    if (self != NULL)
    {
        self->conParameters = defaultConnectionParameters;
        self->alParameters = defaultAppLayerParameters;

        self->asduHandler = NULL;
        self->interrogationHandler = NULL;
        self->counterInterrogationHandler = NULL;
        self->readHandler = NULL;
        self->clockSyncHandler = NULL;
        self->resetProcessHandler = NULL;
        self->delayAcquisitionHandler = NULL;
        self->connectionRequestHandler = NULL;
        self->connectionEventHandler = NULL;
        self->rawMessageHandler = NULL;
        self->maxLowPrioQueueSize = maxLowPrioQueueSize;
        self->maxHighPrioQueueSize = maxHighPrioQueueSize;

        {
            int i;

            for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
            {
                self->masterConnections[i] = MasterConnection_create(self);
            }
        }

        self->maxOpenConnections = CONFIG_CS104_MAX_CLIENT_CONNECTIONS;
#if (CONFIG_USE_SEMAPHORES == 1)
        self->openConnectionsLock = Semaphore_create(1);
        self->stateLock = Semaphore_create(1);
#endif

#if (CONFIG_USE_THREADS == 1)
        self->isThreadlessMode = false;
#endif

        self->isRunning = false;
        self->stopRunning = false;

        self->localAddress = NULL;
        self->tcpPort = CS104_DEFAULT_PORT;
        self->openConnections = 0;

#if (CONFIG_USE_THREADS == 1)
        self->listeningThread = NULL;
#endif

        self->serverSocket = NULL;

        self->plugins = NULL;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        self->tlsConfig = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
        self->redundancyGroups = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        self->serverMode = CS104_MODE_SINGLE_REDUNDANCY_GROUP;
#else
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
        self->serverMode = CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP;
#endif
#endif
    }

    return self;
}

CS104_Slave
CS104_Slave_create(int maxLowPrioQueueSize, int maxHighPrioQueueSize)
{
    return createSlave(maxLowPrioQueueSize, maxHighPrioQueueSize);
}

#if (CONFIG_CS104_SUPPORT_TLS == 1)
CS104_Slave
CS104_Slave_createSecure(int maxLowPrioQueueSize, int maxHighPrioQueueSize, TLSConfiguration tlsConfig)
{
    CS104_Slave self = createSlave(maxLowPrioQueueSize, maxHighPrioQueueSize);

    if (self != NULL)
    {
        self->tcpPort = 19998;
        self->tlsConfig = tlsConfig;
    }

    return self;
}
#endif /* (CONFIG_CS104_SUPPORT_TLS == 1) */

void
CS104_Slave_addPlugin(CS104_Slave self, CS101_SlavePlugin plugin)
{
    if (self->plugins == NULL)
        self->plugins = LinkedList_create();

    if (self->plugins)
        LinkedList_add(self->plugins, plugin);
}

void
CS104_Slave_setServerMode(CS104_Slave self, CS104_ServerMode serverMode)
{
    self->serverMode = serverMode;
}

void
CS104_Slave_setLocalAddress(CS104_Slave self, const char* ipAddress)
{
    if (self->localAddress)
        GLOBAL_FREEMEM(self->localAddress);

    self->localAddress = (char*)GLOBAL_MALLOC(strlen(ipAddress) + 1);

    if (self->localAddress)
        strcpy(self->localAddress, ipAddress);
}

void
CS104_Slave_setLocalPort(CS104_Slave self, int tcpPort)
{
    self->tcpPort = tcpPort;
}

int
CS104_Slave_getOpenConnections(CS104_Slave self)
{
    int openConnections;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->openConnectionsLock);
#endif

    openConnections = self->openConnections;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->openConnectionsLock);
#endif

    return openConnections;
}

static MasterConnection
getFreeConnection(CS104_Slave self)
{
    MasterConnection connection = NULL;

    int i;

    for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
    {
        MasterConnection con = self->masterConnections[i];

        if (con)
        {
#if (CONFIG_USE_SEMAPHORES)
            Semaphore_wait(con->stateLock);
#endif

            if (con->isUsed == false)
            {
                connection = con;
                connection->isUsed = true;
            }

#if (CONFIG_USE_SEMAPHORES)
            Semaphore_post(con->stateLock);
#endif
        }

        if (connection)
            break;
    }

    return connection;
}

void
CS104_Slave_setMaxOpenConnections(CS104_Slave self, int maxOpenConnections)
{
    if (CONFIG_CS104_MAX_CLIENT_CONNECTIONS > 0)
    {
        if (maxOpenConnections > CONFIG_CS104_MAX_CLIENT_CONNECTIONS)
            maxOpenConnections = CONFIG_CS104_MAX_CLIENT_CONNECTIONS;
    }

    self->maxOpenConnections = maxOpenConnections;
}

void
CS104_Slave_setConnectionRequestHandler(CS104_Slave self, CS104_ConnectionRequestHandler handler, void* parameter)
{
    self->connectionRequestHandler = handler;
    self->connectionRequestHandlerParameter = parameter;
}

void
CS104_Slave_setConnectionEventHandler(CS104_Slave self, CS104_ConnectionEventHandler handler, void* parameter)
{
    self->connectionEventHandler = handler;
    self->connectionEventHandlerParameter = parameter;
}

/**
 * Activate connection and deactivate existing active connections if required
 */
static void
CS104_Slave_activate(CS104_Slave self, MasterConnection connectionToActivate)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
    {

        /* Deactivate all other connections */
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif
        int i;

        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            MasterConnection con = self->masterConnections[i];

            if (con && con->isUsed)
            {
                if (con != connectionToActivate)
                    MasterConnection_deactivate(con);
            }
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->openConnectionsLock);
#endif
    }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)

    if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
    {

        /* Deactivate all other connections of the same redundancy group */
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif

        int i;

        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            MasterConnection con = self->masterConnections[i];

            if (con && con->isUsed)
            {
                if (con->redundancyGroup == connectionToActivate->redundancyGroup)
                {
                    if (con != connectionToActivate)
                        MasterConnection_deactivate(con);
                }
            }
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->openConnectionsLock);
#endif
    }

#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

    MasterConnection_activate(connectionToActivate);
}

void
CS104_Slave_setInterrogationHandler(CS104_Slave self, CS101_InterrogationHandler handler, void* parameter)
{
    self->interrogationHandler = handler;
    self->interrogationHandlerParameter = parameter;
}

void
CS104_Slave_setCounterInterrogationHandler(CS104_Slave self, CS101_CounterInterrogationHandler handler, void* parameter)
{
    self->counterInterrogationHandler = handler;
    self->counterInterrogationHandlerParameter = parameter;
}

void
CS104_Slave_setReadHandler(CS104_Slave self, CS101_ReadHandler handler, void* parameter)
{
    self->readHandler = handler;
    self->readHandlerParameter = parameter;
}

void
CS104_Slave_setASDUHandler(CS104_Slave self, CS101_ASDUHandler handler, void* parameter)
{
    self->asduHandler = handler;
    self->asduHandlerParameter = parameter;
}

void
CS104_Slave_setClockSyncHandler(CS104_Slave self, CS101_ClockSynchronizationHandler handler, void* parameter)
{
    self->clockSyncHandler = handler;
    self->clockSyncHandlerParameter = parameter;
}

void
CS104_Slave_setRawMessageHandler(CS104_Slave self, CS104_SlaveRawMessageHandler handler, void* parameter)
{
    self->rawMessageHandler = handler;
    self->rawMessageHandlerParameter = parameter;
}

CS104_APCIParameters
CS104_Slave_getConnectionParameters(CS104_Slave self)
{
    return &(self->conParameters);
}

CS101_AppLayerParameters
CS104_Slave_getAppLayerParameters(CS104_Slave self)
{
    return &(self->alParameters);
}

/********************************************************
 * MasterConnection
 *********************************************************/

static void
printSendBuffer(MasterConnection self)
{
    if (self->oldestSentASDU != -1)
    {
        int currentIndex = self->oldestSentASDU;

        int nextIndex = 0;

        DEBUG_PRINT("CS104 SLAVE: ------k-buffer------\n");

        do
        {
            DEBUG_PRINT("CS104 SLAVE: %02i : SeqNo=%i time=%llu : queueEntry=%p\n", currentIndex,
                        self->sentASDUs[currentIndex].seqNo, self->sentASDUs[currentIndex].sentTime,
                        self->sentASDUs[currentIndex].queueEntry);

            if (currentIndex == self->newestSentASDU)
                nextIndex = -1;
            else
                currentIndex = (currentIndex + 1) % self->maxSentASDUs;

        } while (nextIndex != -1);

        DEBUG_PRINT("CS104 SLAVE: --------------------\n");
    }
    else
        DEBUG_PRINT("CS104 SLAVE: k-buffer is empty\n");
}

/**
 * \return number of bytes read, or -1 in case of an error
 */
static int
readFromSocket(MasterConnection self, uint8_t* buffer, int size)
{
#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket != NULL)
        return TLSSocket_read(self->tlsSocket, buffer, size);
    else
        return Socket_read(self->socket, buffer, size);
#else
    return Socket_read(self->socket, buffer, size);
#endif
}

/**
 * \brief Read message part into receive buffer
 *
 * \return -1 in case of an error, 0 when no complete message can be read, > 0 when a complete message is in buffer
 */
static int
receiveMessage(MasterConnection self)
{
    uint8_t* buffer = self->recvBuffer;
    int bufPos = self->recvBufPos;

    /* read start byte */
    if (bufPos == 0)
    {
        int readFirst = readFromSocket(self, buffer, 1);

        if (readFirst < 1)
            return readFirst;

        if (buffer[0] != 0x68)
            return -1; /* message error */

        bufPos++;
    }

    /* read length byte */
    if (bufPos == 1)
    {
        if (readFromSocket(self, buffer + 1, 1) != 1)
        {
            self->recvBufPos = 0;
            return -1;
        }

        bufPos++;
    }

    /* read remaining frame */
    if (bufPos > 1)
    {
        int length = buffer[1];

        int remainingLength = length - bufPos + 2;

        int readCnt = readFromSocket(self, buffer + bufPos, remainingLength);

        if (readCnt == remainingLength)
        {
            self->recvBufPos = 0;
            return length + 2;
        }
        else if (readCnt == -1)
        {
            self->recvBufPos = 0;
            return -1;
        }
        else
        {
            self->recvBufPos = bufPos + readCnt;
            return 0;
        }
    }

    self->recvBufPos = bufPos;
    return 0;
}

static int
writeToSocket(MasterConnection self, uint8_t* buf, int size)
{
    if (self->slave->rawMessageHandler)
        self->slave->rawMessageHandler(self->slave->rawMessageHandlerParameter, &(self->iMasterConnection), buf, size,
                                       true);

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket)
        return TLSSocket_write(self->tlsSocket, buf, size);
    else
        return Socket_write(self->socket, buf, size);
#else
    return Socket_write(self->socket, buf, size);
#endif
}

static int
sendIMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    buffer[0] = (uint8_t)0x68;
    buffer[1] = (uint8_t)(msgSize - 2);

    buffer[2] = (uint8_t)((self->sendCount % 128) * 2);
    buffer[3] = (uint8_t)(self->sendCount / 128);

    buffer[4] = (uint8_t)((self->receiveCount % 128) * 2);
    buffer[5] = (uint8_t)(self->receiveCount / 128);

    if (writeToSocket(self, buffer, msgSize) > 0)
    {
        DEBUG_PRINT("CS104 SLAVE: SEND I (size = %i) N(S) = %i N(R) = %i\n", msgSize, self->sendCount,
                    self->receiveCount);
        self->sendCount = (self->sendCount + 1) % 32768;
        self->unconfirmedReceivedIMessages = 0;
        self->timeoutT2Triggered = false;
    }
    else
        self->isRunning = false;

    self->unconfirmedReceivedIMessages = 0;

    int sendCount = self->sendCount;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return sendCount;
}

static bool
isSentBufferFull(MasterConnection self)
{
    /* locking of k-buffer has to be done by caller! */
    if (self->oldestSentASDU == -1)
        return false;

    int newIndex = (self->newestSentASDU + 1) % (self->maxSentASDUs);

    if (newIndex == self->oldestSentASDU)
        return true;
    else
        return false;
}

static void
sendASDU(MasterConnection self, uint8_t* buffer, int msgSize, uint64_t entryId, uint8_t* queueEntry)
{
    int currentIndex = 0;

    if (self->oldestSentASDU == -1)
    {
        self->oldestSentASDU = 0;
        self->newestSentASDU = 0;
    }
    else
    {
        currentIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;
    }

    self->sentASDUs[currentIndex].entryId = entryId;
    self->sentASDUs[currentIndex].queueEntry = queueEntry;
    self->sentASDUs[currentIndex].seqNo = sendIMessage(self, buffer, msgSize);
    self->sentASDUs[currentIndex].sentTime = Hal_getMonotonicTimeInMs();

    self->newestSentASDU = currentIndex;

    printSendBuffer(self);
}

static bool
sendASDUInternal(MasterConnection self, CS101_ASDU asdu)
{
    bool asduSent;

    if (MasterConnection_isActive(self))
    {
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->sentASDUsLock);
#endif

        if (isSentBufferFull(self) == false)
        {

            FrameBuffer frameBuffer;

            struct sBufferFrame bufferFrame;

            Frame frame = BufferFrame_initialize(&bufferFrame, frameBuffer.msg, IEC60870_5_104_APCI_LENGTH);
            CS101_ASDU_encode(asdu, frame);

            frameBuffer.msgSize = Frame_getMsgSize(frame);

            sendASDU(self, frameBuffer.msg, frameBuffer.msgSize, 0, NULL);

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->sentASDUsLock);
#endif

            asduSent = true;
        }
        else
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->sentASDUsLock);
#endif
            asduSent = HighPriorityASDUQueue_enqueue(self->highPrioQueue, asdu);
        }
    }
    else
        asduSent = false;

    if (asduSent == false)
        DEBUG_PRINT("CS104 SLAVE: unable to send response (state=%i)\n", self->state);

    return asduSent;
}

static void
responseNegative(CS101_ASDU asdu, MasterConnection self, CS101_CauseOfTransmission cot)
{
    CS101_ASDU_setCOT(asdu, cot);
    CS101_ASDU_setNegative(asdu, true);
    sendASDUInternal(self, asdu);
}

static void
responseCOTUnknown(CS101_ASDU asdu, MasterConnection self)
{
    DEBUG_PRINT("CS104 SLAVE:   with unknown COT\n");
    responseNegative(asdu, self, CS101_COT_UNKNOWN_COT);
}

/*
 * Handle received ASDUs
 *
 * Call the appropriate callbacks according to ASDU type and CoT
 *
 * \return true when ASDU is valid, false otherwise (e.g. corrupted message data)
 */
static bool
handleASDU(MasterConnection self, CS101_ASDU asdu)
{
    bool messageHandled = false;

    CS104_Slave slave = self->slave;

    /* call plugins */
    if (slave->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(slave->plugins);

        while (pluginElem)
        {
            CS101_SlavePlugin plugin = (CS101_SlavePlugin)LinkedList_getData(pluginElem);

            CS101_SlavePlugin_Result result = plugin->handleAsdu(plugin->parameter, &(self->iMasterConnection), asdu);

            if (result == CS101_PLUGIN_RESULT_HANDLED)
                return true;

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    uint8_t cot = CS101_ASDU_getCOT(asdu);

    switch (CS101_ASDU_getTypeID(asdu))
    {

    case C_IC_NA_1: /* 100 - interrogation command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd interrogation command C_IC_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION))
        {
            if (slave->interrogationHandler != NULL)
            {
                union uInformationObject _io;

                InterrogationCommand irc =
                    (InterrogationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (irc)
                {
                    /* Verify IOA = 0 */
                    if (InformationObject_getObjectAddress((InformationObject)irc) != 0)
                    {
                        DEBUG_PRINT("CS104 SLAVE: interrogation command has invalid IOA - should be 0\n");
                        responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);
                        messageHandled = true;
                    }
                    else
                    {
                        if (slave->interrogationHandler(slave->interrogationHandlerParameter,
                                                        &(self->iMasterConnection), asdu,
                                                        InterrogationCommand_getQOI(irc)))
                            messageHandled = true;
                    }
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_CI_NA_1: /* 101 - counter interrogation command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd counter interrogation command C_CI_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION))
        {
            if (slave->counterInterrogationHandler != NULL)
            {
                union uInformationObject _io;

                CounterInterrogationCommand cic =
                    (CounterInterrogationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (cic)
                {
                    /* Verify IOA = 0 */
                    if (InformationObject_getObjectAddress((InformationObject)cic) != 0)
                    {
                        DEBUG_PRINT("CS104 SLAVE: counter interrogation command has invalid IOA - should be 0\n");
                        responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);
                        messageHandled = true;
                    }
                    else
                    {
                        if (slave->counterInterrogationHandler(slave->counterInterrogationHandlerParameter,
                                                               &(self->iMasterConnection), asdu,
                                                               CounterInterrogationCommand_getQCC(cic)))
                            messageHandled = true;
                    }
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_RD_NA_1: /* 102 - read command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd read command C_RD_NA_1\n");

        if (cot == CS101_COT_REQUEST)
        {
            if (slave->readHandler != NULL)
            {
                union uInformationObject _io;

                ReadCommand rc = (ReadCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (rc)
                {
                    if (slave->readHandler(slave->readHandlerParameter, &(self->iMasterConnection), asdu,
                                           InformationObject_getObjectAddress((InformationObject)rc)))
                        messageHandled = true;
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd clock sync command C_CS_NA_1\n");

        if (cot == CS101_COT_ACTIVATION)
        {
            if (slave->clockSyncHandler != NULL)
            {
                union uInformationObject _io;

                ClockSynchronizationCommand csc =
                    (ClockSynchronizationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (csc)
                {
                    /* Verify IOA = 0 */
                    if (InformationObject_getObjectAddress((InformationObject)csc) != 0)
                    {
                        DEBUG_PRINT("CS104 SLAVE: time sync command has invalid IOA - should be 0\n");
                        responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);
                    }
                    else
                    {
                        CP56Time2a newTime = ClockSynchronizationCommand_getTime(csc);

                        if (slave->clockSyncHandler(slave->clockSyncHandlerParameter, &(self->iMasterConnection), asdu,
                                                    newTime))
                        {
                            CS101_ASDU_removeAllElements(asdu);

                            ClockSynchronizationCommand_create(csc, 0, newTime);

                            CS101_ASDU_addInformationObject(asdu, (InformationObject)csc);

                            CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);

                            sendASDUInternal(self, asdu);
                        }
                        else
                        {
                            CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                            CS101_ASDU_setNegative(asdu, true);

                            sendASDUInternal(self, asdu);
                        }
                    }

                    messageHandled = true;
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_TS_NA_1: /* 104 - test command */

#if (CONFIG_ALLOW_C_TS_NA_1_FOR_CS104 == 1)
        DEBUG_PRINT("CS104 SLAVE: Rcvd test command C_TS_NA_1\n");

        {
            union uInformationObject _io;

            TestCommand tc = (TestCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

            if (tc)
            {
                /* Verify IOA = 0 */
                if (InformationObject_getObjectAddress((InformationObject)tc) != 0)
                {
                    DEBUG_PRINT("CS104 SLAVE: test command has invalid IOA - should be 0\n");
                    responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);
                }
                else
                {
                    if (cot != CS101_COT_ACTIVATION)
                    {
                        DEBUG_PRINT("CS104 SLAVE: test command has invalid COT - should be ACTIVATION(6)\n");
                        CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
                        CS101_ASDU_setNegative(asdu, true);
                    }
                    else
                    {
                        CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                    }

                    sendASDUInternal(self, asdu);
                }
            }
            else
            {
                DEBUG_PRINT("CS104 SLAVE: invalid test command\n");
                return false;
            }

            messageHandled = true;
        }
#else
        /* this command is not supported/allowed for IEC 104 */
        DEBUG_PRINT("CS104 SLAVE: Rcvd test command C_TS_NA_1 -> not allowed\n");
        messageHandled = false;
#endif /* (CONFIG_ALLOW_C_TS_NA_1_FOR_CS104 == 1) */

        break;

    case C_RP_NA_1: /* 105 - Reset process command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd reset process command C_RP_NA_1\n");

        if (cot == CS101_COT_ACTIVATION)
        {
            if (slave->resetProcessHandler != NULL)
            {
                union uInformationObject _io;

                ResetProcessCommand rpc =
                    (ResetProcessCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (rpc)
                {
                    /* Verify IOA = 0 */
                    if (InformationObject_getObjectAddress((InformationObject)rpc) != 0)
                    {
                        DEBUG_PRINT("CS104 SLAVE: reset process command has invalid IOA - should be 0\n");
                        responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);

                        messageHandled = true;
                    }
                    else
                    {
                        if (slave->resetProcessHandler(slave->resetProcessHandlerParameter, &(self->iMasterConnection),
                                                       asdu, ResetProcessCommand_getQRP(rpc)))
                            messageHandled = true;
                    }
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_CD_NA_1: /* 106 - Delay acquisition command */

        DEBUG_PRINT("CS104 SLAVE: Rcvd delay acquisition command C_CD_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_SPONTANEOUS))
        {
            if (slave->delayAcquisitionHandler != NULL)
            {
                union uInformationObject _io;

                DelayAcquisitionCommand dac =
                    (DelayAcquisitionCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (dac)
                {
                    /* Verify IOA = 0 */
                    if (InformationObject_getObjectAddress((InformationObject)dac) != 0)
                    {
                        DEBUG_PRINT("CS104 SLAVE: delay aquisition command has invalid IOA - should be 0\n");
                        responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);

                        messageHandled = true;
                    }
                    else
                    {
                        if (slave->delayAcquisitionHandler(slave->delayAcquisitionHandlerParameter,
                                                           &(self->iMasterConnection), asdu,
                                                           DelayAcquisitionCommand_getDelay(dac)))
                            messageHandled = true;
                    }
                }
                else
                    return false;
            }
        }
        else
        {
            responseCOTUnknown(asdu, self);
            messageHandled = true;
        }

        break;

    case C_TS_TA_1: /* 107 - test command with timestamp */

        DEBUG_PRINT("CS104 SLAVE: Rcvd test command with CP56Time2a C_TS_TA_1\n");

        {
            union uInformationObject _io;

            TestCommandWithCP56Time2a tc =
                (TestCommandWithCP56Time2a)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

            if (tc)
            {
                /* Verify IOA = 0 */
                if (InformationObject_getObjectAddress((InformationObject)tc) != 0)
                {
                    DEBUG_PRINT("CS104 SLAVE: test command has invalid IOA - should be 0\n");
                    responseNegative(asdu, self, CS101_COT_UNKNOWN_IOA);
                    return true;
                }

                /* Only COT = ACTIVATION is allowed */
                if (cot != CS101_COT_ACTIVATION)
                {
                    DEBUG_PRINT("CS104 SLAVE: test command has invalid COT - should be ACTIVATION(6)\n");
                    responseNegative(asdu, self, CS101_COT_UNKNOWN_COT);
                    return true;
                }

                CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                sendASDUInternal(self, asdu);

                return true;
            }
            else
            {
                DEBUG_PRINT("CS104 SLAVE: invalid test command\n");
                return false;
            }
        }

        break;

    default: /* no special handler available -> use default handler */
        break;
    }

    if ((messageHandled == false) && (slave->asduHandler != NULL))
        if (slave->asduHandler(slave->asduHandlerParameter, &(self->iMasterConnection), asdu))
            messageHandled = true;

    if (messageHandled == false)
    {
        /* send error response */
        CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
        CS101_ASDU_setNegative(asdu, true);
        sendASDUInternal(self, asdu);
    }

    return true;
}

static bool
checkSequenceNumber(MasterConnection self, int seqNo)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    /* check if received sequence number is valid */

    bool seqNoIsValid = false;
    bool counterOverflowDetected = false;
    int oldestValidSeqNo = -1;

    if (self->oldestSentASDU == -1)
    { /* if k-Buffer is empty */
        if (seqNo == self->sendCount)
            seqNoIsValid = true;
    }
    else
    {
        /* two cases are required to reflect sequence number overflow */
        int oldestAsduSeqNo = self->sentASDUs[self->oldestSentASDU].seqNo;
        int newestAsduSeqNo = self->sentASDUs[self->newestSentASDU].seqNo;

        if (oldestAsduSeqNo <= newestAsduSeqNo)
        {
            if ((seqNo >= oldestAsduSeqNo) && (seqNo <= newestAsduSeqNo))
                seqNoIsValid = true;
        }
        else
        {
            if ((seqNo >= oldestAsduSeqNo) || (seqNo <= newestAsduSeqNo))
                seqNoIsValid = true;

            counterOverflowDetected = true;
        }

        /* check if confirmed message was already removed from list */
        if (oldestAsduSeqNo == 0)
            oldestValidSeqNo = 32767;
        else
            oldestValidSeqNo = (oldestAsduSeqNo - 1) % 32768;

        if (oldestValidSeqNo == seqNo)
            seqNoIsValid = true;
    }

    if (seqNoIsValid)
    {
        if (self->oldestSentASDU != -1)
        {
            do
            {
                int oldestAsduSeqNo = self->sentASDUs[self->oldestSentASDU].seqNo;

                if (counterOverflowDetected == false)
                {
                    if (seqNo < oldestAsduSeqNo)
                        break;
                }

                if (seqNo == oldestValidSeqNo)
                    break;

                /* remove from server (low-priority) queue if required */
                if (self->sentASDUs[self->oldestSentASDU].queueEntry != NULL)
                {
                    MessageQueue_lock(self->lowPrioQueue);

                    MessageQueue_markAsduAsConfirmed(self->lowPrioQueue,
                                                     self->sentASDUs[self->oldestSentASDU].queueEntry,
                                                     self->sentASDUs[self->oldestSentASDU].entryId);

                    self->sentASDUs[self->oldestSentASDU].queueEntry = NULL;

                    self->sentASDUs[self->oldestSentASDU].seqNo = -1;

                    MessageQueue_unlock(self->lowPrioQueue);
                }

                if (oldestAsduSeqNo == seqNo)
                {
                    /* we arrived at the seq# that has been confirmed */

                    if (self->oldestSentASDU == self->newestSentASDU)
                        self->oldestSentASDU = -1;
                    else
                        self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                    break;
                }

                self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                int checkIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

                if (self->oldestSentASDU == checkIndex)
                {
                    self->oldestSentASDU = -1;
                    break;
                }

            } while (true);
        }
    }
    else
        DEBUG_PRINT("CS104 SLAVE: Received sequence number out of range");

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return seqNoIsValid;
}

static bool
MasterConnection_isRunning(MasterConnection self)
{
    bool retVal;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    retVal = self->isRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return retVal;
}

static bool
MasterConnection_isActive(MasterConnection self)
{
    bool isActive = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    if (self->state == M_CON_STATE_STARTED)
        isActive = true;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return isActive;
}

static void
_resetT3Timeout(MasterConnection self, uint64_t currentTime)
{
    self->nextT3Timeout = currentTime + (uint64_t)(self->slave->conParameters.t3 * 1000);
}

static void
resetT3Timeout(MasterConnection self, uint64_t currentTime)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    _resetT3Timeout(self, currentTime);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif
}

static bool
checkT3Timeout(MasterConnection self, uint64_t currentTime)
{
    bool retVal = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    if (self->waitingForTestFRcon)
        goto exit_function;

    if (self->nextT3Timeout > (currentTime + (uint64_t)(self->slave->conParameters.t3 * 1000)))
    {
        /* timeout value not plausible (maybe system time changed) */
        _resetT3Timeout(self, currentTime);
    }

    if (currentTime > self->nextT3Timeout)
        retVal = true;

exit_function:

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    return retVal;
}

static void
resetTestFRConTimeout(MasterConnection self, uint64_t currentTime)
{
    self->nextTestFRConTimeout = currentTime + (uint64_t)(self->slave->conParameters.t1 * 1000);
}

static bool
checkTestFRConTimeout(MasterConnection self, uint64_t currentTime)
{
    if (self->nextTestFRConTimeout > (currentTime + (uint64_t)(self->slave->conParameters.t1 * 1000)))
    {
        /* timeout value not plausible (maybe system time changed) */
        resetTestFRConTimeout(self, currentTime);
    }

    if (currentTime > self->nextTestFRConTimeout)
        return true;
    else
        return false;
}

/* unprotected version of sendSMessage */
static void
_sendSMessage(MasterConnection self)
{
    uint8_t msg[6];

    msg[0] = 0x68;
    msg[1] = 0x04;
    msg[2] = 0x01;
    msg[3] = 0;
    msg[4] = (uint8_t)((self->receiveCount % 128) * 2);
    msg[5] = (uint8_t)(self->receiveCount / 128);

    if (writeToSocket(self, msg, 6) < 0)
        self->isRunning = false;
}

static void
sendSMessage(MasterConnection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    _sendSMessage(self);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif
}

static bool
handleMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    uint64_t currentTime = Hal_getMonotonicTimeInMs();

    if (msgSize >= 3)
    {
        if (buffer[0] != 0x68)
        {
            DEBUG_PRINT("CS104 SLAVE: Invalid START character!");
            return false;
        }

        uint8_t lengthOfApdu = buffer[1];

        if (lengthOfApdu != msgSize - 2)
        {
            DEBUG_PRINT("CS104 SLAVE: Invalid length of APDU");
            return false;
        }

        if ((buffer[2] & 1) == 0) /* I message */
        {
            if (msgSize < 7)
            {
                DEBUG_PRINT("CS104 SLAVE: Received I msg too small!");
                return false;
            }

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif

            if (self->state != M_CON_STATE_STARTED)
            {
                DEBUG_PRINT("CS104 SLAVE: Received I message while connection not active -> close connection");

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_post(self->stateLock);
#endif

                return false;
            }

            if (self->timeoutT2Triggered == false)
            {
                self->timeoutT2Triggered = true;
                self->lastConfirmationTime = currentTime; /* start timeout T2 */
            }
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            int frameSendSequenceNumber = ((buffer[3] * 0x100) + (buffer[2] & 0xfe)) / 2;
            int frameRecvSequenceNumber = ((buffer[5] * 0x100) + (buffer[4] & 0xfe)) / 2;

            DEBUG_PRINT("CS104 SLAVE: Received I frame: N(S) = %i N(R) = %i\n", frameSendSequenceNumber,
                        frameRecvSequenceNumber);

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif
            if (frameSendSequenceNumber != self->receiveCount)
            {

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_post(self->stateLock);
#endif

                DEBUG_PRINT("CS104 SLAVE: Sequence error - close connection");
                return false;
            }
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            if (checkSequenceNumber(self, frameRecvSequenceNumber) == false)
            {
                DEBUG_PRINT("CS104 SLAVE: Sequence number check failed - close connection");
                return false;
            }

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif
            self->receiveCount = (self->receiveCount + 1) % 32768;
            self->unconfirmedReceivedIMessages++;
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            if (MasterConnection_isActive(self))
            {
                struct sCS101_ASDU _asdu;

                CS101_ASDU asdu =
                    CS101_ASDU_createFromBufferEx(&_asdu, &(self->slave->alParameters), buffer + 6, msgSize - 6);

                if (asdu)
                {
                    bool validAsdu = handleASDU(self, asdu);

                    if (validAsdu == false)
                    {
                        DEBUG_PRINT("CS104 SLAVE: ASDU corrupted");
                        return false;
                    }
                }
                else
                {
                    DEBUG_PRINT("CS104 SLAVE: Invalid ASDU");
                    return false;
                }
            }
            else
            {
                DEBUG_PRINT("CS104 SLAVE: Received I message while connection not activate -> close connection");
                return false;
            }
        }

        /* Check for TESTFR_ACT message */
        else if ((buffer[2] & 0x43) == 0x43)
        {
            DEBUG_PRINT("CS104 SLAVE: Send TESTFR_CON\n");

            if (writeToSocket(self, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE) < 0)
                return false;
        }

        /* Check for STARTDT_ACT message */
        else if ((buffer[2] & 0x07) == 0x07)
        {
            CS104_Slave_activate(self->slave, self);

            HighPriorityASDUQueue_resetConnectionQueue(self->highPrioQueue);

            DEBUG_PRINT("CS104 SLAVE: Send STARTDT_CON\n");

            if (writeToSocket(self, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE) < 0)
                return false;
        }

        /* Check for STOPDT_ACT message */
        else if ((buffer[2] & 0x13) == 0x13)
        {
            DEBUG_PRINT("CS104 SLAVE: Received STARTDT_ACT\n");

            MasterConnection_deactivate(self);

            /* Send S-Message to confirm all outstanding messages */

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif

            if (self->unconfirmedReceivedIMessages > 0)
            {
                self->lastConfirmationTime = Hal_getMonotonicTimeInMs();

                self->unconfirmedReceivedIMessages = 0;

                self->timeoutT2Triggered = false;

                _sendSMessage(self);
            }

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            if (MasterConnection_hasUnconfirmedMessages(self))
            {
                DEBUG_PRINT(
                    "CS104 SLAVE: Unconfirmed messages after STOPDT_ACT -> pending unconfirmed stopped state\n");
            }
            else
            {
                DEBUG_PRINT("CS104 SLAVE: Send STOPDT_CON\n");

                self->state = M_CON_STATE_STOPPED;

                if (writeToSocket(self, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE) < 0)
                {
#if (CONFIG_USE_SEMAPHORES == 1)
                    Semaphore_post(self->stateLock);
#endif

                    return false;
                }
            }

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif
        }

        /* Check for TESTFR_CON message */
        else if ((buffer[2] & 0x83) == 0x83)
        {
            DEBUG_PRINT("CS104 SLAVE: Recv TESTFR_CON\n");

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif
            self->waitingForTestFRcon = false;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif
        }

        else if (buffer[2] == 0x01) /* S-message */
        {
            int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

            DEBUG_PRINT("CS104 SLAVE: Rcvd S(%i) (own sendcounter = %i)\n", seqNo, self->sendCount);

            if (checkSequenceNumber(self, seqNo) == false)
            {
                DEBUG_PRINT("CS104 SLAVE: S message - sequence number mismatch");
                return false;
            }

            if (self->state == M_CON_STATE_UNCONFIRMED_STOPPED)
            {
                if (MasterConnection_hasUnconfirmedMessages(self) == false)
                {
                    self->state = M_CON_STATE_STOPPED;

                    DEBUG_PRINT("CS104 SLAVE: Send STOPDT_CON\n");

                    if (writeToSocket(self, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE) < 0)
                        return false;
                }
            }
            else if (self->state == M_CON_STATE_STOPPED)
            {
                DEBUG_PRINT("CS104 SLAVE: S message in stopped state -> active close\n");
                /* actively close connection */
                return false;
            }
        }

        else
        {
            DEBUG_PRINT("CS104 SLAVE: unknown message - IGNORE\n");
            return true;
        }

        resetT3Timeout(self, currentTime);

        return true;
    }
    else
    {
        DEBUG_PRINT("CS104 SLAVE: Invalid message (too small)");
        return false;
    }
}

static void
MasterConnection_deinit(MasterConnection self)
{
    if (self)
    {
#if (CONFIG_CS104_SUPPORT_TLS == 1)
        if (self->tlsSocket != NULL)
            TLSSocket_close(self->tlsSocket);
#endif

        if (self->socket)
        {
            Socket_destroy(self->socket);
            self->socket = NULL;
        }

        self->state = M_CON_STATE_STOPPED;
    }
}

static void
MasterConnection_destroy(MasterConnection self)
{
    if (self)
    {

        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_destroy(self->sentASDUsLock);
        Semaphore_destroy(self->stateLock);
#endif

        Handleset_destroy(self->handleSet);

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
        if (self->slave->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
        {
            MessageQueue_destroy(self->lowPrioQueue);
            HighPriorityASDUQueue_destroy(self->highPrioQueue);
        }
#endif

        GLOBAL_FREEMEM(self);
    }
}

static void
sendNextLowPriorityASDU(MasterConnection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    uint8_t* asduBuffer;

    if (isSentBufferFull(self))
        goto exit_function;

    MessageQueue_lock(self->lowPrioQueue);

    uint64_t entryId;
    uint8_t* queueEntry;
    int msgSize;

    asduBuffer = MessageQueue_getNextWaitingASDU(self->lowPrioQueue, &entryId, &queueEntry, &msgSize);

    if (asduBuffer)
    {
        memcpy(self->sendBuffer + IEC60870_5_104_APCI_LENGTH, asduBuffer, msgSize);

        msgSize += IEC60870_5_104_APCI_LENGTH;

        sendASDU(self, self->sendBuffer, msgSize, entryId, queueEntry);
    }

    MessageQueue_unlock(self->lowPrioQueue);

exit_function:

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return;
}

static bool
sendNextHighPriorityASDU(MasterConnection self)
{
    bool retVal = false;
    uint8_t* buffer = NULL;
    int msgSize = 0;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (isSentBufferFull(self))
        goto exit_function;

    HighPriorityASDUQueue_lock(self->highPrioQueue);

    buffer = HighPriorityASDUQueue_getNextASDU(self->highPrioQueue, &msgSize);

    if (buffer)
    {
        memcpy(self->sendBuffer + IEC60870_5_104_APCI_LENGTH, buffer, msgSize);

        msgSize += IEC60870_5_104_APCI_LENGTH;

        sendASDU(self, self->sendBuffer, msgSize, 0, NULL);

        retVal = true;
    }

    HighPriorityASDUQueue_unlock(self->highPrioQueue);

exit_function:
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return retVal;
}

/**
 * Send all high-priority ASDUs and the last waiting ASDU from the low-priority queue.
 * Returns true if ASDUs are still waiting. This can happen when there are more ASDUs
 * in the event (low-priority) buffer, or the connection is unavailable to send the high-priority
 * ASDUs (congestion or connection lost).
 */
static bool
sendWaitingASDUs(MasterConnection self)
{
    /* send all available high priority ASDUs first */
    while (HighPriorityASDUQueue_isAsduAvailable(self->highPrioQueue))
    {

        if (sendNextHighPriorityASDU(self) == false)
            return true;

        if (MasterConnection_isRunning(self) == false)
            return true;
    }

    /* send messages from low-priority queue */
    sendNextLowPriorityASDU(self);

    if (MessageQueue_isAsduAvailable(self->lowPrioQueue))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool
handleTimeouts(MasterConnection self)
{
    uint64_t currentTime = Hal_getMonotonicTimeInMs();

    bool timeoutsOk = true;

    /* check T3 timeout */
    if (checkT3Timeout(self, currentTime))
    {
        if (writeToSocket(self, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE) < 0)
        {

            DEBUG_PRINT("CS104 SLAVE: Failed to write TESTFR ACT message\n");
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif
            self->isRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->stateLock);
#endif
        self->waitingForTestFRcon = true;
        resetTestFRConTimeout(self, currentTime);

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->stateLock);
#endif
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    /* Check for TEST FR con timeout */
    if (self->waitingForTestFRcon)
    {
        if (checkTestFRConTimeout(self, currentTime))
        {
            DEBUG_PRINT("CS104 SLAVE: Timeout for TESTFR CON message\n");

            /* close connection */
            timeoutsOk = false;
        }
    }

    /* check timeout for others station I messages */
    if (self->unconfirmedReceivedIMessages > 0)
    {

        /* Check validity of last confirmation time */
        if (self->lastConfirmationTime != UINT64_MAX && self->lastConfirmationTime > currentTime)
        {
            /* last confirmation time is in the future (maybe caused by system time change) */
            self->lastConfirmationTime = currentTime;
        }

        if (currentTime > self->lastConfirmationTime)
        {
            if ((currentTime - self->lastConfirmationTime) >= (uint64_t)(self->slave->conParameters.t2 * 1000))
            {
                self->lastConfirmationTime = currentTime;
                self->unconfirmedReceivedIMessages = 0;
                self->timeoutT2Triggered = false;
                _sendSMessage(self);
            }
        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    /* check if counterpart confirmed I message */
    if (self->oldestSentASDU != -1)
    {

        /* check validity of sent time */

        if (self->sentASDUs[self->oldestSentASDU].sentTime > currentTime)
        {
            /* sent time is in the future (maybe caused by system time change) */
            self->sentASDUs[self->oldestSentASDU].sentTime = currentTime;
        }

        if (currentTime > self->sentASDUs[self->oldestSentASDU].sentTime)
        {

            if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >=
                (uint64_t)(self->slave->conParameters.t1 * 1000))
            {
                timeoutsOk = false;

                printSendBuffer(self);

                DEBUG_PRINT("CS104 SLAVE: I message timeout for %i seqNo: %i\n", self->oldestSentASDU,
                            self->sentASDUs[self->oldestSentASDU].seqNo);
            }
        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return timeoutsOk;
}

static void
CS104_Slave_closeAllConnections(CS104_Slave self)
{
#if (CONFIG_USE_SEMAPHORES)
    Semaphore_wait(self->openConnectionsLock);
#endif

    int i;

    for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
    {
        if (self->masterConnections[i])
        {
            if (self->masterConnections[i]->isUsed)
            {
                self->masterConnections[i]->isUsed = false;
                MasterConnection_deinit(self->masterConnections[i]);
            }
        }
    }

    self->openConnections = 0;

#if (CONFIG_USE_SEMAPHORES)
    Semaphore_post(self->openConnectionsLock);
#endif
}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection)parameter;

    resetT3Timeout(self, Hal_getMonotonicTimeInMs());

    bool isAsduWaiting = false;

    if (self->slave->connectionEventHandler)
    {
        self->slave->connectionEventHandler(self->slave->connectionEventHandlerParameter, &(self->iMasterConnection),
                                            CS104_CON_EVENT_CONNECTION_OPENED);
    }

    while (MasterConnection_isRunning(self))
    {
        Handleset_reset(self->handleSet);
        Handleset_addSocket(self->handleSet, self->socket);

        int socketTimeout;

        /*
         * When an ASDU is waiting only have a short look to see if a client request
         * was received. Otherwise wait to save CPU time.
         */
        if (isAsduWaiting)
            socketTimeout = 0;
        else
            socketTimeout = 100;

        if (Handleset_waitReady(self->handleSet, socketTimeout))
        {
            int bytesRec = receiveMessage(self);

            if (bytesRec == -1)
            {
                DEBUG_PRINT("CS104 SLAVE: Error reading from socket\n");
                break;
            }

            if (bytesRec > 0)
            {
                DEBUG_PRINT("CS104 SLAVE: Connection: rcvd msg(%i bytes)\n", bytesRec);

                if (self->slave->rawMessageHandler)
                    self->slave->rawMessageHandler(self->slave->rawMessageHandlerParameter, &(self->iMasterConnection),
                                                   self->recvBuffer, bytesRec, false);

                if (handleMessage(self, self->recvBuffer, bytesRec) == false)
                {
#if (CONFIG_USE_SEMAPHORES == 1)
                    Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                    self->isRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
                    Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                }

                if (self->unconfirmedReceivedIMessages >= self->slave->conParameters.w)
                {
                    self->lastConfirmationTime = Hal_getMonotonicTimeInMs();

                    self->unconfirmedReceivedIMessages = 0;

                    self->timeoutT2Triggered = false;

                    sendSMessage(self);
                }
            }
        }

        if (handleTimeouts(self) == false)
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

            self->isRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
        }

        if (MasterConnection_isRunning(self))
        {
            if (MasterConnection_isActive(self))
            {
                isAsduWaiting = sendWaitingASDUs(self);
            }
        }

        /* call plugins */
        if (self->slave->plugins)
        {
            LinkedList pluginElem = LinkedList_getNext(self->slave->plugins);

            while (pluginElem)
            {
                CS101_SlavePlugin plugin = (CS101_SlavePlugin)LinkedList_getData(pluginElem);

                plugin->runTask(plugin->parameter, &(self->iMasterConnection));

                pluginElem = LinkedList_getNext(pluginElem);
            }
        }
    }

    if (self->slave->connectionEventHandler)
    {
        self->slave->connectionEventHandler(self->slave->connectionEventHandlerParameter, &(self->iMasterConnection),
                                            CS104_CON_EVENT_CONNECTION_CLOSED);
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    self->isRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    MessageQueue_setWaitingForTransmissionWhenNotConfirmed(self->lowPrioQueue);

    return NULL;
}

/********************************************
 * IMasterConnection
 *******************************************/

static bool
_IMasterConnection_isReady(IMasterConnection self)
{
    MasterConnection con = (MasterConnection)self->object;

    if (MasterConnection_isActive(con))
    {
        if (isSentBufferFull(con) == false)
            return true;

        if (HighPriorityASDUQueue_isFull(con->highPrioQueue))
            return false;

        return true;
    }
    else
        return false;
}

static bool
_IMasterConnection_sendASDU(IMasterConnection self, CS101_ASDU asdu)
{
    MasterConnection con = (MasterConnection)self->object;

    return sendASDUInternal(con, asdu);
}

static bool
_IMasterConnection_sendACT_CON(IMasterConnection self, CS101_ASDU asdu, bool negative)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
    CS101_ASDU_setNegative(asdu, negative);

    return _IMasterConnection_sendASDU(self, asdu);
}

static bool
_IMasterConnection_sendACT_TERM(IMasterConnection self, CS101_ASDU asdu)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_TERMINATION);
    CS101_ASDU_setNegative(asdu, false);

    return _IMasterConnection_sendASDU(self, asdu);
}

static void
_IMasterConnection_close(IMasterConnection self)
{
    MasterConnection con = (MasterConnection)self->object;

    MasterConnection_close(con);
}

static int
_IMasterConnection_getPeerAddress(IMasterConnection self, char* addrBuf, int addrBufSize)
{
    MasterConnection con = (MasterConnection)self->object;

    char buf[54];

    if (con->socket == NULL)
    {
        return 0;
    }

    char* addrStr = Socket_getPeerAddressStatic(con->socket, buf);

    if (addrStr == NULL)
        return 0;

    int len = (int)strlen(buf);

    if (len < addrBufSize)
    {
        strcpy(addrBuf, buf);
        return len;
    }
    else
        return 0;
}

static CS101_AppLayerParameters
_IMasterConnection_getApplicationLayerParameters(IMasterConnection self)
{
    MasterConnection con = (MasterConnection)self->object;

    return &(con->slave->alParameters);
}

/********************************************
 * END IMasterConnection
 *******************************************/

static MasterConnection
MasterConnection_create(CS104_Slave slave)
{
    MasterConnection self = (MasterConnection)GLOBAL_CALLOC(1, sizeof(struct sMasterConnection));

    if (self != NULL)
    {
        self->state = M_CON_STATE_STOPPED;
        self->isUsed = false;
        self->slave = slave;
        self->maxSentASDUs = 0;
        self->sentASDUs = NULL;
        self->iMasterConnection.object = self;
        self->iMasterConnection.getApplicationLayerParameters = _IMasterConnection_getApplicationLayerParameters;
        self->iMasterConnection.isReady = _IMasterConnection_isReady;
        self->iMasterConnection.sendASDU = _IMasterConnection_sendASDU;
        self->iMasterConnection.sendACT_CON = _IMasterConnection_sendACT_CON;
        self->iMasterConnection.sendACT_TERM = _IMasterConnection_sendACT_TERM;
        self->iMasterConnection.close = _IMasterConnection_close;
        self->iMasterConnection.getPeerAddress = _IMasterConnection_getPeerAddress;

#if (CONFIG_USE_THREADS == 1)
        self->connectionThread = NULL;
#endif

#if (CONFIG_USE_SEMAPHORES == 1)
        self->sentASDUsLock = Semaphore_create(1);
        self->stateLock = Semaphore_create(1);
#endif
        self->handleSet = Handleset_new();

        /* initialize pointers with NULL to avoid segmentation fault on destroy call */
        self->socket = NULL;
#if (CONFIG_CS104_SUPPORT_TLS == 1)
        self->tlsSocket = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
        self->redundancyGroup = NULL;
#endif
        self->lowPrioQueue = NULL;
        self->highPrioQueue = NULL;
    }

    return self;
}

static bool
MasterConnection_init(MasterConnection self, Socket skt, MessageQueue lowPrioQueue, HighPriorityASDUQueue highPrioQueue)
{
    if (self)
    {
        self->socket = skt;
        self->isRunning = false;
        self->receiveCount = 0;
        self->sendCount = 0;
        self->recvBufPos = 0;

        if (self->maxSentASDUs != self->slave->conParameters.k)
        {
            if (self->sentASDUs)
            {
                GLOBAL_FREEMEM(self->sentASDUs);
                self->sentASDUs = NULL;
            }
        }

        if (self->sentASDUs == NULL)
        {
            self->maxSentASDUs = self->slave->conParameters.k;
            self->sentASDUs = (SentASDUSlave*)GLOBAL_CALLOC(self->maxSentASDUs, sizeof(SentASDUSlave));

            if (self->sentASDUs == NULL)
            {
                DEBUG_PRINT("CS104 SLAVE: Failed to allocate memory for sent ASDU buffer\n");
                return false;
            }
        }

        self->unconfirmedReceivedIMessages = 0;
        self->lastConfirmationTime = UINT64_MAX;

        self->timeoutT2Triggered = false;

        self->oldestSentASDU = -1;
        self->newestSentASDU = -1;

        resetT3Timeout(self, Hal_getMonotonicTimeInMs());

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        if (self->slave->tlsConfig != NULL)
        {
            self->tlsSocket = TLSSocket_create(skt, self->slave->tlsConfig, false);

            if (self->tlsSocket == NULL)
            {
                DEBUG_PRINT("CS104 SLAVE: Failed to create TLS context. Close connection\n");

                self->isUsed = false;
                return false;
            }
        }
        else
            self->tlsSocket = NULL;
#endif

        /* for the mode CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP we use the connection specific queues */
        if (lowPrioQueue)
            self->lowPrioQueue = lowPrioQueue;
        else
        {
            MessageQueue_releaseAllQueuedASDUs(self->lowPrioQueue);
        }

        if (highPrioQueue)
            self->highPrioQueue = highPrioQueue;

        HighPriorityASDUQueue_resetConnectionQueue(self->highPrioQueue);

        self->waitingForTestFRcon = false;

        return true;
    }
    else
    {
        return false;
    }
}

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
static bool
MasterConnection_initEx(MasterConnection self, Socket skt, CS104_RedundancyGroup redGroup)
{
    bool retVal = false;

    if (self)
    {
        retVal = MasterConnection_init(self, skt, redGroup->asduQueue, redGroup->connectionAsduQueue);

        if (retVal)
            self->redundancyGroup = redGroup;
    }

    return retVal;
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

#if (CONFIG_USE_THREADS == 1)
static void
MasterConnection_start(MasterConnection self)
{
    if (self->connectionThread)
    {
        Thread_destroy(self->connectionThread);
        self->connectionThread = NULL;
    }

    self->isRunning = true;
    self->state = M_CON_STATE_STOPPED;

    self->connectionThread = Thread_create((ThreadExecutionFunction)connectionHandlingThread, (void*)self, false);

    Thread_start(self->connectionThread);
}
#endif /* (CONFIG_USE_THREADS == 1) */

static void
MasterConnection_close(MasterConnection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    self->isRunning = false;
    self->state = M_CON_STATE_STOPPED;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

static bool
MasterConnection_hasUnconfirmedMessages(MasterConnection self)
{
    bool retVal = false;

    if (self->lowPrioQueue)
    {
        if (MessageQueue_hasUnconfirmedIMessages(self->lowPrioQueue))
            return true;

        if (HighPriorityASDUQueue_hasUnconfirmedIMessages(self->highPrioQueue))
            return true;
    }

    return retVal;
}

static void
MasterConnection_deactivate(MasterConnection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    if (self->isUsed)
    {
        if (self->state == M_CON_STATE_STARTED)
        {
            if (self->slave->connectionEventHandler)
            {
                self->slave->connectionEventHandler(self->slave->connectionEventHandlerParameter,
                                                    &(self->iMasterConnection), CS104_CON_EVENT_DEACTIVATED);
            }
        }
    }

    self->state = M_CON_STATE_UNCONFIRMED_STOPPED;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

static void
MasterConnection_activate(MasterConnection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    if (self->state != M_CON_STATE_STARTED)
    {
        if (self->slave->connectionEventHandler)
        {
            self->slave->connectionEventHandler(self->slave->connectionEventHandlerParameter,
                                                &(self->iMasterConnection), CS104_CON_EVENT_ACTIVATED);
        }
    }

    self->state = M_CON_STATE_STARTED;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

static void
MasterConnection_handleTcpConnection(MasterConnection self)
{
    int bytesRec = receiveMessage(self);

    if (bytesRec < 0)
    {
        DEBUG_PRINT("CS104 SLAVE: Error reading from socket\n");
        self->isRunning = false;
    }

    if ((bytesRec > 0) && (self->isRunning))
    {
        if (self->slave->rawMessageHandler)
            self->slave->rawMessageHandler(self->slave->rawMessageHandlerParameter, &(self->iMasterConnection),
                                           self->recvBuffer, bytesRec, false);

        if (handleMessage(self, self->recvBuffer, bytesRec) == false)
            self->isRunning = false;

        if (self->unconfirmedReceivedIMessages >= self->slave->conParameters.w)
        {
            self->lastConfirmationTime = Hal_getMonotonicTimeInMs();

            self->unconfirmedReceivedIMessages = 0;

            self->timeoutT2Triggered = false;

            sendSMessage(self);
        }
    }
}

static void
MasterConnection_executePeriodicTasks(MasterConnection self)
{
    if (self->state == M_CON_STATE_STARTED)
    {
        sendWaitingASDUs(self);
    }

    if (handleTimeouts(self) == false)
    {
        self->isRunning = false;
    }
}

static void
handleClientConnections(CS104_Slave self)
{
    HandleSet handleset = NULL;

    if (self->openConnections > 0)
    {
        int i;

        bool first = true;

        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            MasterConnection con = self->masterConnections[i];

            if (con && con->isUsed)
            {
                if (con->isRunning)
                {
                    if (first)
                    {
                        handleset = con->handleSet;
                        Handleset_reset(handleset);

                        first = false;
                    }

                    Handleset_addSocket(handleset, con->socket);
                }
                else
                {
                    if (self->connectionEventHandler)
                    {
                        self->connectionEventHandler(self->connectionEventHandlerParameter, &(con->iMasterConnection),
                                                     CS104_CON_EVENT_CONNECTION_CLOSED);
                    }

                    DEBUG_PRINT("CS104 SLAVE: Connection closed\n");

                    self->masterConnections[i]->isUsed = false;

                    MessageQueue_setWaitingForTransmissionWhenNotConfirmed(self->masterConnections[i]->lowPrioQueue);

                    self->openConnections--;

                    MasterConnection_deinit(con);
                }
            }
        }

        /* handle incoming messages when available */
        if (handleset && Handleset_waitReady(handleset, 0))
        {
            for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
            {
                MasterConnection con = self->masterConnections[i];

                if (con && con->isUsed)
                    MasterConnection_handleTcpConnection(con);
            }
        }

        /* handle periodic tasks for running connections */
        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            MasterConnection con = self->masterConnections[i];

            if (con && con->isUsed && con->isRunning)
            {
                MasterConnection_executePeriodicTasks(con);

                /* call plugins */
                if (self->plugins)
                {
                    LinkedList pluginElem = LinkedList_getNext(self->plugins);

                    while (pluginElem)
                    {
                        CS101_SlavePlugin plugin = (CS101_SlavePlugin)LinkedList_getData(pluginElem);

                        plugin->runTask(plugin->parameter, &(con->iMasterConnection));

                        pluginElem = LinkedList_getNext(pluginElem);
                    }
                }
            }
        }
    }
}

static char*
getPeerAddress(Socket socket, char* ipAddress)
{
    char* ipAddrStr = NULL;

    if (Socket_getPeerAddressStatic(socket, ipAddress))
    {
        /* remove TCP port part */
        if (ipAddress[0] == '[')
        {
            /* IPV6 address */
            ipAddrStr = ipAddress + 1;

            char* separator = strchr(ipAddrStr, ']');

            if (separator != NULL)
                *separator = 0;
        }
        else
        {
            /* IPV4 address */
            ipAddrStr = ipAddress;

            char* separator = strchr(ipAddrStr, ':');

            if (separator != NULL)
                *separator = 0;
        }
    }

    return ipAddrStr;
}

static bool
callConnectionRequestHandler(CS104_Slave self, Socket newSocket)
{
    char ipAddress[60];

    char* ipAddrStr = getPeerAddress(newSocket, ipAddress);

    if (ipAddrStr == NULL)
        return false;

    if (self->connectionRequestHandler)
        return self->connectionRequestHandler(self->connectionRequestHandlerParameter, ipAddrStr);
    else
        return true;
}

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
static CS104_RedundancyGroup
getMatchingRedundancyGroup(CS104_Slave self, char* ipAddrStr)
{
    struct sCS104_IPAddress ipAddress;
    memset(&ipAddress, 0, sizeof(struct sCS104_IPAddress));

    CS104_IPAddress_setFromString(&ipAddress, ipAddrStr);

    CS104_RedundancyGroup catchAllGroup = NULL;
    CS104_RedundancyGroup matchingGroup = NULL;

    LinkedList element = LinkedList_getNext(self->redundancyGroups);

    while (element)
    {
        CS104_RedundancyGroup redGroup = (CS104_RedundancyGroup)LinkedList_getData(element);

        if (CS104_RedundancyGroup_matches(redGroup, &ipAddress))
        {
            matchingGroup = redGroup;
            break;
        }

        if (CS104_RedundancyGroup_isCatchAll(redGroup))
            catchAllGroup = redGroup;

        element = LinkedList_getNext(element);
    }

    if (matchingGroup == NULL)
        matchingGroup = catchAllGroup;

    return matchingGroup;
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

/* handle TCP connections in non-threaded mode */
static void
handleConnectionsThreadless(CS104_Slave self)
{
    if ((self->maxOpenConnections < 1) || (self->openConnections < self->maxOpenConnections))
    {
        Socket newSocket = ServerSocket_accept(self->serverSocket);

        if (newSocket)
        {
            bool acceptConnection = true;

            if (acceptConnection)
                acceptConnection = callConnectionRequestHandler(self, newSocket);

            if (acceptConnection)
            {
                MessageQueue lowPrioQueue = NULL;
                HighPriorityASDUQueue highPrioQueue = NULL;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
                if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
                {
                    lowPrioQueue = self->asduQueue;
                    highPrioQueue = self->connectionAsduQueue;
                }
#endif

                MasterConnection connection = NULL;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
                if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
                {
                    char ipAddress[60];

                    char* ipAddrStr = getPeerAddress(newSocket, ipAddress);

                    if (ipAddrStr)
                    {
                        CS104_RedundancyGroup matchingGroup = getMatchingRedundancyGroup(self, ipAddrStr);

                        if (matchingGroup)
                        {
#if (CONFIG_USE_SEMAPHORES)
                            Semaphore_wait(self->openConnectionsLock);
#endif

                            connection = getFreeConnection(self);

                            if (connection)
                            {
                                if (MasterConnection_initEx(connection, newSocket, matchingGroup))
                                {
                                    self->openConnections++;

                                    if (matchingGroup->name)
                                    {
                                        DEBUG_PRINT("CS104 SLAVE: Add connection to group: %s\n", matchingGroup->name);
                                    }
                                }
                                else
                                {
                                    connection->isUsed = false;
                                    connection = NULL;
                                }
                            }

#if (CONFIG_USE_SEMAPHORES)
                            Semaphore_post(self->openConnectionsLock);
#endif
                        }
                        else
                        {
                            DEBUG_PRINT("CS104 SLAVE: Found no matching redundancy group -> close connection\n");
                        }
                    }
                    else
                    {
                        DEBUG_PRINT("CS104 SLAVE: cannot determine peer IP address -> close connection\n");
                    }
                }
                else
#endif /* CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS */
                {
#if (CONFIG_USE_SEMAPHORES)
                    Semaphore_wait(self->openConnectionsLock);
#endif
                    connection = getFreeConnection(self);

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
                    if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
                    {
                        lowPrioQueue = connection->lowPrioQueue;
                        MessageQueue_initialize(lowPrioQueue);

                        highPrioQueue = connection->highPrioQueue;
                        HighPriorityASDUQueue_initialize(highPrioQueue);
                    }
#endif /* CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS */

                    if (connection)
                    {
                        if (MasterConnection_init(connection, newSocket, lowPrioQueue, highPrioQueue))
                        {
                            self->openConnections++;
                        }
                        else
                        {
                            connection->isUsed = false;
                            connection = NULL;
                        }
                    }

#if (CONFIG_USE_SEMAPHORES)
                    Semaphore_post(self->openConnectionsLock);
#endif
                }

                if (connection)
                {
                    connection->isRunning = true;

                    if (self->connectionEventHandler)
                    {
                        self->connectionEventHandler(self->connectionEventHandlerParameter,
                                                     &(connection->iMasterConnection),
                                                     CS104_CON_EVENT_CONNECTION_OPENED);
                    }
                }
                else
                {
                    Socket_destroy(newSocket);
                    DEBUG_PRINT("CS104 SLAVE: Connection attempt failed!\n");
                }
            }
            else
            {
                Socket_destroy(newSocket);
            }
        }
    }

    handleClientConnections(self);
}

#if (CONFIG_USE_THREADS == 1)

static void*
serverThread(void* parameter)
{
    CS104_Slave self = (CS104_Slave)parameter;

    if (self->localAddress)
        self->serverSocket = TcpServerSocket_create(self->localAddress, self->tcpPort);
    else
        self->serverSocket = TcpServerSocket_create("0.0.0.0", self->tcpPort);

    if (self->serverSocket == NULL)
    {
        DEBUG_PRINT("CS104 SLAVE: Cannot create server socket\n");

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->stateLock);
#endif
        self->isStarting = false;

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->stateLock);
#endif

        goto exit_function;
    }

    ServerSocket_listen(self->serverSocket);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    self->isRunning = true;
    self->isStarting = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

    while (isStopRunningSet(self) == false)
    {
        Socket newSocket = ServerSocket_accept(self->serverSocket);

        if (newSocket)
        {
            bool acceptConnection = true;

            /* check if maximum number of open connections is reached */
            if (self->maxOpenConnections > 0)
            {
                if (CS104_Slave_getOpenConnections(self) >= self->maxOpenConnections)
                    acceptConnection = false;
            }

            if (acceptConnection)
                acceptConnection = callConnectionRequestHandler(self, newSocket);

            if (acceptConnection)
            {
                MessageQueue lowPrioQueue = NULL;
                HighPriorityASDUQueue highPrioQueue = NULL;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
                if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
                {
                    lowPrioQueue = self->asduQueue;
                    highPrioQueue = self->connectionAsduQueue;
                }
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
                if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
                {
                    lowPrioQueue = NULL;
                    highPrioQueue = NULL;
                }
#endif

                MasterConnection connection = NULL;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
                if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
                {
                    char ipAddress[60];

                    char* ipAddrStr = getPeerAddress(newSocket, ipAddress);

                    if (ipAddrStr)
                    {
                        CS104_RedundancyGroup matchingGroup = getMatchingRedundancyGroup(self, ipAddrStr);

                        if (matchingGroup)
                        {
#if (CONFIG_USE_SEMAPHORES)
                            Semaphore_wait(self->openConnectionsLock);
#endif

                            connection = getFreeConnection(self);

                            if (connection)
                            {
                                if (MasterConnection_initEx(connection, newSocket, matchingGroup))
                                {
                                    self->openConnections++;

                                    if (matchingGroup->name)
                                    {
                                        DEBUG_PRINT("CS104 SLAVE: Add connection to group: %s\n", matchingGroup->name);
                                    }
                                }
                                else
                                {
                                    connection->isUsed = false;
                                    connection = NULL;
                                }
                            }

#if (CONFIG_USE_SEMAPHORES)
                            Semaphore_post(self->openConnectionsLock);
#endif
                        }
                        else
                        {
                            DEBUG_PRINT("CS104 SLAVE: Found no matching redundancy group -> close connection\n");
                        }
                    }
                    else
                    {
                        DEBUG_PRINT("CS104 SLAVE: cannot determine peer IP address -> close connection\n");
                    }
                }
                else
                {
#if (CONFIG_USE_SEMAPHORES)
                    Semaphore_wait(self->openConnectionsLock);
#endif

                    connection = getFreeConnection(self);

                    if (connection)
                    {
                        if (MasterConnection_init(connection, newSocket, lowPrioQueue, highPrioQueue))
                        {
                            self->openConnections++;
                        }
                        else
                        {
                            connection->isUsed = false;
                            connection = NULL;
                        }
                    }

#if (CONFIG_USE_SEMAPHORES)
                    Semaphore_post(self->openConnectionsLock);
#endif
                }
#else

#if (CONFIG_USE_SEMAPHORES)
                Semaphore_wait(self->openConnectionsLock);
#endif
                connection = getFreeConnection(self);

                if (connection)
                {
                    if (MasterConnection_init(connection, newSocket, lowPrioQueue, highPrioQueue))
                    {
                        self->openConnections++;
                    }
                    else
                    {
                        connection->isUsed = false;
                        connection = NULL;
                    }
                }

#if (CONFIG_USE_SEMAPHORES)
                Semaphore_post(self->openConnectionsLock);
#endif

#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

                if (connection)
                {
                    /* now start the connection handling (thread) */
                    MasterConnection_start(connection);
                }
                else
                {
                    Socket_destroy(newSocket);

                    DEBUG_PRINT("CS104 SLAVE: Connection attempt failed!\n");
                }
            }
            else
            {
                Socket_destroy(newSocket);
            }
        }
        else
            Thread_sleep(10);

            /* check if there are connections to close */
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif

        int i;

        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            if (self->masterConnections[i])
            {
                MasterConnection connection = self->masterConnections[i];

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_wait(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                bool isConnectionUsed = connection->isUsed;

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_post(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                if (isConnectionUsed)
                {
                    if (MasterConnection_isRunning(connection) == false)
                    {
                        if (connection->connectionThread)
                        {
                            Thread_destroy(connection->connectionThread);

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_wait(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                            connection->connectionThread = NULL;

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_post(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                        }

                        MasterConnection_deinit(connection);

                        self->openConnections--;

#if (CONFIG_USE_SEMAPHORES == 1)
                        Semaphore_wait(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                        connection->isUsed = false;

#if (CONFIG_USE_SEMAPHORES == 1)
                        Semaphore_post(connection->stateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                    }
                }
            }
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->openConnectionsLock);
#endif
    }

    if (self->serverSocket)
        Socket_destroy((Socket)self->serverSocket);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->stateLock);
#endif

    self->isRunning = false;
    self->stopRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->stateLock);
#endif

exit_function:
    return NULL;
}

#endif /* (CONFIG_USE_THREADS == 1) */

void
CS104_Slave_enqueueASDU(CS104_Slave self, CS101_ASDU asdu)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
        MessageQueue_enqueueASDU(self->asduQueue, asdu);
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)

    if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
    {
        /************************************************
         * Dispatch event to all redundancy groups
         ************************************************/

        LinkedList element = LinkedList_getNext(self->redundancyGroups);

        while (element)
        {
            CS104_RedundancyGroup group = (CS104_RedundancyGroup)LinkedList_getData(element);

            MessageQueue_enqueueASDU(group->asduQueue, asdu);

            element = LinkedList_getNext(element);
        }
    }

#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
    {
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif

        /************************************************
         * Dispatch event to all open client connections
         ************************************************/

        int i;

        for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
        {
            MasterConnection con = self->masterConnections[i];

            if (con)
                MessageQueue_enqueueASDU(con->lowPrioQueue, asdu);
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->openConnectionsLock);
#endif
    }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */
}

void
CS104_Slave_addRedundancyGroup(CS104_Slave self, CS104_RedundancyGroup redundancyGroup)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
    if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
    {
        if (self->redundancyGroups == NULL)
            self->redundancyGroups = LinkedList_create();

        LinkedList_add(self->redundancyGroups, redundancyGroup);
    }

#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */
}

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
static void
initializeRedundancyGroups(CS104_Slave self, int lowPrioMaxQueueSize, int highPrioMaxQueueSize)
{
    if (self->redundancyGroups == NULL)
    {
        CS104_RedundancyGroup redGroup = CS104_RedundancyGroup_create(NULL);
        CS104_Slave_addRedundancyGroup(self, redGroup);
    }

    LinkedList element = LinkedList_getNext(self->redundancyGroups);

    while (element)
    {
        CS104_RedundancyGroup redGroup = (CS104_RedundancyGroup)LinkedList_getData(element);

        if (redGroup->asduQueue == NULL)
            CS104_RedundancyGroup_initializeMessageQueues(redGroup, lowPrioMaxQueueSize, highPrioMaxQueueSize);

        element = LinkedList_getNext(element);
    }
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

void
CS104_Slave_start(CS104_Slave self)
{
#if ((CONFIG_USE_THREADS == 1) && (CONFIG_USE_SEMAPHORES == 1))
    if (isRunning(self) == false)
    {
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->stateLock);
#endif

        self->isStarting = true;
        self->stopRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->stateLock);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
            initializeMessageQueues(self, self->maxLowPrioQueueSize, self->maxHighPrioQueueSize);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
        if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
            initializeRedundancyGroups(self, self->maxLowPrioQueueSize, self->maxHighPrioQueueSize);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
            initializeConnectionSpecificQueues(self);
#endif

        self->listeningThread = Thread_create(serverThread, (void*)self, false);

        Thread_start(self->listeningThread);

        while (isStarting(self))
            Thread_sleep(1);
    }
#else
    DEBUG_PRINT("CS104 SLAVE: ERROR: CS104_Slave_start not supported when CONFIG_USE_TREADS = 0 or "
                "CONFIG_USE_SEMAPHORES = 0!\n");
#endif
}

int
CS104_Slave_getNumberOfQueueEntries(CS104_Slave self, CS104_RedundancyGroup redGroup)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
    {
        return MessageQueue_getEntryCount(self->asduQueue);
    }
#endif
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
    if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
    {
        if (redGroup)
        {
            return MessageQueue_getEntryCount(redGroup->asduQueue);
        }

        DEBUG_PRINT("CS104_SLAVE: redundancy group not found\n");
    }
#endif

    /* mode CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP not supported! */

    return 0;
}

void
CS104_Slave_startThreadless(CS104_Slave self)
{
    if (isRunning(self) == false)
    {
#if (CONFIG_USE_THREADS == 1)
        self->isThreadlessMode = true;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
            initializeMessageQueues(self, self->maxLowPrioQueueSize, self->maxHighPrioQueueSize);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)
        if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
            initializeRedundancyGroups(self, self->maxLowPrioQueueSize, self->maxHighPrioQueueSize);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
            initializeConnectionSpecificQueues(self);
#endif

        if (self->localAddress)
            self->serverSocket = TcpServerSocket_create(self->localAddress, self->tcpPort);
        else
            self->serverSocket = TcpServerSocket_create("0.0.0.0", self->tcpPort);

        if (self->serverSocket == NULL)
        {
            DEBUG_PRINT("CS104 SLAVE: Cannot create server socket\n");

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif

            self->isStarting = false;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            goto exit_function;
        }

        ServerSocket_listen(self->serverSocket);

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->stateLock);
#endif

        self->isRunning = true;

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->stateLock);
#endif
    }

exit_function:
    return;
}

void
CS104_Slave_stopThreadless(CS104_Slave self)
{
    self->isRunning = false;

    if (self->serverSocket)
    {
        ServerSocket_destroy(self->serverSocket);
        self->serverSocket = NULL;
    }

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP)
    {
        deleteConnectionSpecificQueues(self);
    }
#endif

    CS104_Slave_closeAllConnections(self);
}

void
CS104_Slave_tick(CS104_Slave self)
{
    handleConnectionsThreadless(self);
}

bool
CS104_Slave_isRunning(CS104_Slave self)
{
    return isRunning(self);
}

void
CS104_Slave_stop(CS104_Slave self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->isThreadlessMode)
    {
#endif
        CS104_Slave_stopThreadless(self);
#if (CONFIG_USE_THREADS == 1)
    }
    else
    {
        if (isRunning(self))
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->stateLock);
#endif
            self->stopRunning = true;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->stateLock);
#endif

            while (isRunning(self))
                Thread_sleep(1);
        }

        if (self->listeningThread)
        {
            Thread_destroy(self->listeningThread);
        }

        /*
         * Stop all connections
         * */
        {
            int i;

            for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
            {

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_wait(self->openConnectionsLock);
#endif

                MasterConnection connection = self->masterConnections[i];

                if (connection)
                {
#if (CONFIG_USE_SEMAPHORES == 1)
                    Semaphore_wait(connection->stateLock);
#endif

                    bool isUsed = connection->isUsed;

#if (CONFIG_USE_SEMAPHORES == 1)
                    Semaphore_post(connection->stateLock);
#endif

                    if (isUsed)
                    {
                        MasterConnection_close(connection);

#if (CONFIG_USE_THREADS == 1)
                        if (connection->connectionThread)
                        {
#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_post(self->openConnectionsLock);
#endif

                            Thread_destroy(connection->connectionThread);

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_wait(self->openConnectionsLock);
#endif

                            MasterConnection_deinit(connection);

                            connection->connectionThread = NULL;
                        }
#endif /* (CONFIG_USE_THREADS == 1) */

                        self->openConnections--;
                    }
                }

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_post(self->openConnectionsLock);
#endif
            }
        }

        self->listeningThread = NULL;
    }
#endif
}

void
CS104_Slave_destroy(CS104_Slave self)
{
    if (self)
    {
        CS104_Slave_stop(self);

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
        {
            if (self->asduQueue)
                MessageQueue_releaseAllQueuedASDUs(self->asduQueue);
        }
#endif

        if (self->localAddress != NULL)
            GLOBAL_FREEMEM(self->localAddress);

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_destroy(self->openConnectionsLock);
        Semaphore_destroy(self->stateLock);
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
        {
            MessageQueue_destroy(self->asduQueue);
            HighPriorityASDUQueue_destroy(self->connectionAsduQueue);
        }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1)

        if (self->serverMode == CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS)
        {
            if (self->redundancyGroups)
                LinkedList_destroyDeep(self->redundancyGroups,
                                       (LinkedListValueDeleteFunction)CS104_RedundancyGroup_destroy);
        }

#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_MULTIPLE_REDUNDANCY_GROUPS == 1) */

        {
            int i;

            for (i = 0; i < CONFIG_CS104_MAX_CLIENT_CONNECTIONS; i++)
            {
                if (self->masterConnections[i])
                {
                    MasterConnection_destroy(self->masterConnections[i]);
                    self->masterConnections[i] = NULL;
                }
            }
        }

        if (self->plugins)
        {
            LinkedList_destroyStatic(self->plugins);
        }

        GLOBAL_FREEMEM(self);
    }
}
