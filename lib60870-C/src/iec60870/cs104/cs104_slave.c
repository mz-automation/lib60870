/*
 *  Copyright 2016, 2017 MZ Automation GmbH
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
#include <stdio.h>
#include <string.h>

#include "cs104_slave.h"

#include "cs104_frame.h"
#include "frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"
#include "linked_list.h"
#include "buffer_frame.h"

#include "lib60870_config.h"
#include "lib60870_internal.h"
#include "iec60870_slave.h"

#include "apl_types_internal.h"
#include "cs101_asdu_internal.h"

#if (CONFIG_CS104_SUPPORT_TLS == 1)
#include "tls_api.h"
#endif

#if ((CONFIG_CS104_SUPPORT_SERVER_MODE_CONNECTION_IS_REDUNDANCY_GROUP != 1) && (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP != 1))
#error Illegal configuration: Define either CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP or CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP
#endif

#define CS104_DEFAULT_PORT 2404

static struct sCS104_APCIParameters defaultConnectionParameters = {
	/* .k = */ 12,
	/* .w = */ 8,
	/* .t0 = */ 10,
	/* .t1 = */ 15,
	/* .t2 = */ 10,
	/* .t3 = */ 20
};

static struct sCS101_AppLayerParameters defaultAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249
};

typedef struct {
    uint8_t msg[256];
    int msgSize;
} FrameBuffer;

typedef enum  {
    QUEUE_ENTRY_STATE_NOT_USED,
    QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION,
    QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED
} QueueEntryState;

struct sASDUQueueEntry {
    uint64_t entryTimestamp;
    FrameBuffer asdu;
    QueueEntryState state;
};

typedef struct sASDUQueueEntry* ASDUQueueEntry;


/***************************************************
 * MessageQueue
 ***************************************************/

struct sMessageQueue {
    int size;
    int entryCounter;
    int lastMsgIndex;
    int firstMsgIndex;

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    struct sASDUQueueEntry asdus [CONFIG_SLAVE_ASDU_QUEUE_SIZE];
#else
    ASDUQueueEntry asdus;
#endif

#if (CONFIG_USE_THREADS == 1)
    Semaphore queueLock;
#endif
};

typedef struct sMessageQueue* MessageQueue;

static void
MessageQueue_initialize(MessageQueue self, int maxQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
#else
    self->asdus = (ASDUQueueEntry) GLOBAL_CALLOC(maxQueueSize, sizeof(struct sASDUQueueEntry));
#endif

    self->entryCounter = 0;
    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    self->size = maxQueueSize;

#if (CONFIG_USE_THREADS == 1)
    self->queueLock = Semaphore_create(1);
#endif
}

static MessageQueue
MessageQueue_create(int maxQueueSize)
{
    MessageQueue self = (MessageQueue) GLOBAL_MALLOC(sizeof(struct sMessageQueue));

    if (self != NULL)
        MessageQueue_initialize(self, maxQueueSize);

    return self;
}

static void
MessageQueue_destroy(MessageQueue self)
{
    if (self != NULL) {
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE != 1)
        GLOBAL_FREEMEM(self->asdus);
#endif

#if (CONFIG_USE_THREADS == 1)
        Semaphore_destroy(self->queueLock);
#endif

        GLOBAL_FREEMEM(self);
    }
}

static void
MessageQueue_lock(MessageQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif
}

static void
MessageQueue_unlock(MessageQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}


/**
 * Add an ASDU to the queue. When queue is full, override oldest entry.
 */
static void
MessageQueue_enqueueASDU(MessageQueue self, CS101_ASDU asdu)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

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

    struct sBufferFrame bufferFrame;

    Frame frame = BufferFrame_initialize(&bufferFrame, self->asdus[nextIndex].asdu.msg,
                                            IEC60870_5_104_APCI_LENGTH);

    CS101_ASDU_encode(asdu, frame);

    self->asdus[nextIndex].asdu.msgSize = Frame_getMsgSize(frame);
    self->asdus[nextIndex].entryTimestamp = Hal_getTimeInMs();
    self->asdus[nextIndex].state = QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION;

    DEBUG_PRINT("ASDUs in FIFO: %i (first: %i, last: %i)\n", self->entryCounter,
            self->firstMsgIndex, self->lastMsgIndex);

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}

static bool
MessageQueue_isAsduAvailable(MessageQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    bool retVal;

    if (self->entryCounter > 0)
        retVal = true;
    else
        retVal = false;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif

    return retVal;
}


static FrameBuffer*
MessageQueue_getNextWaitingASDU(MessageQueue self, uint64_t* timestamp, int* queueIndex)
{
    FrameBuffer* buffer = NULL;

    if (self->entryCounter != 0) {
        int currentIndex = self->firstMsgIndex;

        while (self->asdus[currentIndex].state != QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION) {

            if (self->asdus[currentIndex].state == QUEUE_ENTRY_STATE_NOT_USED)
                break;

            currentIndex = (currentIndex + 1) % self->size;

            /* break when we reached the oldest entry again */
            if (currentIndex == self->firstMsgIndex)
                break;
        }

        if (self->asdus[currentIndex].state == QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION) {

            self->asdus[currentIndex].state = QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED;
            *timestamp = self->asdus[currentIndex].entryTimestamp;
            *queueIndex = currentIndex;

            buffer = &(self->asdus[currentIndex].asdu);
        }
    }

    return buffer;
}

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
static void
MessageQueue_releaseAllQueuedASDUs(MessageQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    self->entryCounter = 0;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

static void
MessageQueue_markAsduAsConfirmed(MessageQueue self, int queueIndex, uint64_t timestamp)
{
    if ((queueIndex < 0) || (queueIndex > self->size))
        return;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    if (self->entryCounter > 0) {
        if (self->asdus[queueIndex].state == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED) {

            if (self->asdus[queueIndex].entryTimestamp == timestamp) {
                int currentIndex = queueIndex;

                while (self->asdus[currentIndex].state == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED) {

                    DEBUG_PRINT("Remove from queue with index %i\n", currentIndex);

                    self->asdus[currentIndex].state = QUEUE_ENTRY_STATE_NOT_USED;
                    self->asdus[currentIndex].entryTimestamp = 0;

                    self->entryCounter--;

                    if (self->entryCounter == 0) {
                        self->firstMsgIndex = -1;
                        self->lastMsgIndex = -1;
                        break;
                    }

                    if (currentIndex == self->firstMsgIndex) {
                        self->firstMsgIndex = (queueIndex + 1) % self->size;

                        if (self->entryCounter == 1)
                            self->lastMsgIndex = self->firstMsgIndex;

                        break;
                    }

                    currentIndex--;

                    if (currentIndex < 0)
                        currentIndex = self->size - 1;

                    /* break if we reached the first deleted entry again */
                    if (currentIndex == queueIndex)
                        break;

                    DEBUG_PRINT("queue state: noASDUs: %i oldest: %i latest: %i\n", self->entryCounter,
                            self->firstMsgIndex, self->lastMsgIndex);

                }
            }
        }
    }

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}

/***************************************************
 * HighPriorityASDUQueue
 ***************************************************/

struct sHighPriorityASDUQueue {
    int size;
    int entryCounter;
    int lastMsgIndex;
    int firstMsgIndex;

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    FrameBuffer asdus[CONFIG_SLAVE_CONNECTION_ASDU_QUEUE_SIZE];
#else
    FrameBuffer* asdus;
#endif

#if (CONFIG_USE_THREADS == 1)
    Semaphore queueLock;
#endif
};

typedef struct sHighPriorityASDUQueue* HighPriorityASDUQueue;


static void
HighPriorityASDUQueue_initialize(HighPriorityASDUQueue self, int maxQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
#else
    self->asdus = (FrameBuffer*) GLOBAL_CALLOC(maxQueueSize, sizeof(FrameBuffer));
#endif

    self->entryCounter = 0;
    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    self->size = maxQueueSize;

#if (CONFIG_USE_THREADS == 1)
    self->queueLock = Semaphore_create(1);
#endif
}

static HighPriorityASDUQueue
HighPriorityASDUQueue_create(int maxQueueSize)
{
    HighPriorityASDUQueue self = (HighPriorityASDUQueue) GLOBAL_MALLOC(sizeof(struct sHighPriorityASDUQueue));

    if (self != NULL)
        HighPriorityASDUQueue_initialize(self, maxQueueSize);

    return self;
}

static void
HighPriorityASDUQueue_destroy(HighPriorityASDUQueue self)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
#else
    GLOBAL_FREEMEM(self->asdus);
#endif

#if (CONFIG_USE_THREADS == 1)
    Semaphore_destroy(self->queueLock);
#endif

    GLOBAL_FREEMEM(self);
}

static void
HighPriorityASDUQueue_lock(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif
}

static void
HighPriorityASDUQueue_unlock(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}

static bool
HighPriorityASDUQueue_isAsduAvailable(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    bool retVal;

    if (self->entryCounter > 0)
        retVal = true;
    else
        retVal = false;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif

    return retVal;
}

static FrameBuffer*
HighPriorityASDUQueue_getNextASDU(HighPriorityASDUQueue self)
{
    FrameBuffer* buffer = NULL;

    if (self->entryCounter != 0)  {
        int currentIndex = self->firstMsgIndex;

        if (self->entryCounter == 1) {
            self->entryCounter = 0;
            self->firstMsgIndex = -1;
            self->lastMsgIndex = -1;
        }
        else {
            self->firstMsgIndex = (self->firstMsgIndex + 1) % (self->size);

            self->entryCounter--;
        }

        buffer = &(self->asdus[currentIndex]);
    }

    return buffer;
}

static bool
HighPriorityASDUQueue_enqueue(HighPriorityASDUQueue self, CS101_ASDU asdu)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    Frame frame;

    bool enqueued = false;

    if (self->entryCounter == self->size)
        goto exit_function;

    int nextIndex;

    if (self->entryCounter == 0) {
        self->firstMsgIndex = 0;
        nextIndex = 0;
    }
    else
        nextIndex = self->lastMsgIndex + 1;

    if (nextIndex == self->size)
        nextIndex = 0;

    DEBUG_PRINT("HighPrio AsduQueue: add entry (nextIndex:%i)\n", nextIndex);
    self->lastMsgIndex = nextIndex;
    self->entryCounter++;

    struct sBufferFrame bufferFrame;

    frame = BufferFrame_initialize(&bufferFrame, self->asdus[nextIndex].msg,
                                            IEC60870_5_104_APCI_LENGTH);

    CS101_ASDU_encode(asdu, frame);

    self->asdus[nextIndex].msgSize = Frame_getMsgSize(frame);

    DEBUG_PRINT("ASDUs in HighPrio FIFO: %i (first: %i, last: %i)\n", self->entryCounter,
            self->firstMsgIndex, self->lastMsgIndex);

    enqueued = true;

exit_function:

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif

    return enqueued;
}

static void
HighPriorityASDUQueue_resetConnectionQueue(HighPriorityASDUQueue self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->queueLock);
#endif

    self->firstMsgIndex = 0;
    self->lastMsgIndex = 0;
    self->entryCounter = 0;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->queueLock);
#endif
}



/***************************************************
 * Slave
 ***************************************************/

struct sCS104_Slave {
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

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    TLSConfiguration tlsConfig;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP)
    MessageQueue asduQueue; /**< low priority ASDU queue and buffer */
    HighPriorityASDUQueue connectionAsduQueue; /**< high priority ASDU queue */
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP)
    int maxLowPrioQueueSize;
    int maxHighPrioQueueSize;
#endif

    int openConnections; /**< number of connected clients */
    LinkedList masterConnections; /**< references to all MasterConnection objects */
#if (CONFIG_USE_THREADS == 1)
    Semaphore openConnectionsLock;
#endif

    int maxOpenConnections; /**< maximum accepted open client connections */

    /* TODO if configured for fixed number of connections and connection_is_redundancy_group,
     *  add connection queues here */

    struct sCS104_APCIParameters conParameters;

    struct sCS101_AppLayerParameters alParameters;

    bool isStarting;
    bool isRunning;
    bool stopRunning;

    int tcpPort;

    CS104_ServerMode serverMode;

    char* localAddress;
    Thread listeningThread;
};

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)

static struct sCS104_Slave singleStaticSlaveInstance;

#endif

static uint8_t STARTDT_CON_MSG[] = { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

#define STARTDT_CON_MSG_SIZE 6

static uint8_t STOPDT_CON_MSG[] = { 0x68, 0x04, 0x23, 0x00, 0x00, 0x00 };

#define STOPDT_CON_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

#define TESTFR_CON_MSG_SIZE 6

static uint8_t TESTFR_ACT_MSG[] = { 0x68, 0x04, 0x43, 0x00, 0x00, 0x00 };

#define TESTFR_ACT_MSG_SIZE 6

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
static void
initializeMessageQueues(CS104_Slave self, int lowPrioMaxQueueSize, int highPrioMaxQueueSize)
{
    /* initialized low priority queue */

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    lowPrioMaxQueueSize = CONFIG_SLAVE_ASDU_QUEUE_SIZE;
#else
    if (lowPrioMaxQueueSize < 1)
        lowPrioMaxQueueSize = CONFIG_SLAVE_ASDU_QUEUE_SIZE;
#endif

    self->asduQueue = MessageQueue_create(lowPrioMaxQueueSize);
    //TODO support static memory allocation mode

    /* initialize high priority queue */

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    highPrioMaxQueueSize = CONFIG_SLAVE_CONNECTION_ASDU_QUEUE_SIZE;
#else
    if (highPrioMaxQueueSize < 1)
        highPrioMaxQueueSize = CONFIG_SLAVE_CONNECTION_ASDU_QUEUE_SIZE;
#endif

    self->connectionAsduQueue = HighPriorityASDUQueue_create(highPrioMaxQueueSize);
    //TODO support static memory allocation mode
}
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */


static CS104_Slave
createSlave(int maxLowPrioQueueSize, int maxHighPrioQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    CS104_Slave self = &(singleStaticSlaveInstance);
#else
    CS104_Slave self = (CS104_Slave) GLOBAL_MALLOC(sizeof(struct sCS104_Slave));
#endif

    if (self != NULL) {

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

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        self->maxLowPrioQueueSize = maxLowPrioQueueSize;
        self->maxHighPrioQueueSize = maxHighPrioQueueSize;
#endif

        self->masterConnections = LinkedList_create();
        self->maxOpenConnections = CONFIG_CS104_MAX_CLIENT_CONNECTIONS;
#if (CONFIG_USE_THREADS == 1)
        self->openConnectionsLock = Semaphore_create(1);
#endif

        self->isRunning = false;
        self->stopRunning = false;

        self->localAddress = NULL;
        self->tcpPort = CS104_DEFAULT_PORT;
        self->openConnections = 0;
        self->listeningThread = NULL;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        self->tlsConfig = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        self->serverMode = CS104_MODE_SINGLE_REDUNDANCY_GROUP;
#else
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
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

    if (self != NULL) {
        self->tcpPort = 19998;
        self->tlsConfig = tlsConfig;
    }

    return self;
}
#endif /* (CONFIG_CS104_SUPPORT_TLS == 1) */

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

    self->localAddress = (char*) GLOBAL_MALLOC(strlen(ipAddress) + 1);

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

#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->openConnectionsLock);
#endif

    openConnections = self->openConnections;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->openConnectionsLock);
#endif

    return openConnections;
}

void
CS104_Slave_setMaxOpenConnections(CS104_Slave self, int maxOpenConnections)
{
    if (CONFIG_CS104_MAX_CLIENT_CONNECTIONS > 0) {
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

/**
 * Activate connection and deactivate existing active connections if required
 */
static void
CS104_Slave_activate(CS104_Slave self, MasterConnection connectionToActivate)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP) {

        /* Deactivate all other connections */
#if (CONFIG_USE_THREADS == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif

        LinkedList element;

        for (element = LinkedList_getNext(self->masterConnections);
             element != NULL;
             element = LinkedList_getNext(element))
        {
            MasterConnection connection = (MasterConnection) LinkedList_getData(element);

            if (connection != connectionToActivate)
                MasterConnection_deactivate(connection);
        }

#if (CONFIG_USE_THREADS == 1)
        Semaphore_post(self->openConnectionsLock);
#endif

    }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */
}

void
CS104_Slave_setInterrogationHandler(CS104_Slave self, CS101_InterrogationHandler handler, void*  parameter)
{
    self->interrogationHandler = handler;
    self->interrogationHandlerParameter = parameter;
}

void
CS104_Slave_setCounterInterrogationHandler(CS104_Slave self, CS101_CounterInterrogationHandler handler, void*  parameter)
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

typedef struct {
    /* required to identify message in server (low-priority) queue */
    uint64_t entryTime;
    int queueIndex; /* -1 if ASDU is not from low-priority queue */

    /* required for T1 timeout */
    uint64_t sentTime;
    int seqNo;
} SentASDUSlave;

struct sMasterConnection {

    Socket socket;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    TLSSocket tlsSocket;
#endif

    struct sIMasterConnection iMasterConnection;

    CS104_Slave slave;
    bool isActive;
    bool isRunning;

    int sendCount;     /* sent messages - sequence counter */
    int receiveCount;  /* received messages - sequence counter */

    int unconfirmedReceivedIMessages; /* number of unconfirmed messages received */
    uint64_t lastConfirmationTime; /* timestamp when the last confirmation message (for I messages) was sent */

    uint64_t nextT3Timeout;
    int outstandingTestFRConMessages;

    int maxSentASDUs;
    int oldestSentASDU;
    int newestSentASDU;
    SentASDUSlave* sentASDUs;
#if (CONFIG_USE_THREADS == 1)
    Semaphore sentASDUsLock;
#endif

    MessageQueue lowPrioQueue;
    HighPriorityASDUQueue highPrioQueue;

    bool firstIMessageReceived;
};


static void
printSendBuffer(MasterConnection self)
{
    if (self->oldestSentASDU != -1) {
        int currentIndex = self->oldestSentASDU;

        int nextIndex = 0;

        DEBUG_PRINT ("------k-buffer------\n");

        do {
            DEBUG_PRINT("%02i : SeqNo=%i time=%llu : queueIdx=%i\n", currentIndex,
                    self->sentASDUs[currentIndex].seqNo,
                    self->sentASDUs[currentIndex].sentTime,
                    self->sentASDUs[currentIndex].queueIndex);

            if (currentIndex == self->newestSentASDU)
                nextIndex = -1;
            else
                currentIndex = (currentIndex + 1) % self->maxSentASDUs;

        } while (nextIndex != -1);

        DEBUG_PRINT ("--------------------\n");
    }
    else
        DEBUG_PRINT("k-buffer is empty\n");
}

#if (CONFIG_CS104_SUPPORT_TLS == 1)
static inline int
receiveMessageTlsSocket(TLSSocket socket, uint8_t* buffer)
{
    int readFirst = TLSSocket_read(socket, buffer, 1);

    if (readFirst < 1)
        return readFirst;

    if (buffer[0] != 0x68)
        return -1; /* message error */

    if (TLSSocket_read(socket, buffer + 1, 1) != 1)
        return -1;

    int length = buffer[1];

    /* read remaining frame */
    if (TLSSocket_read(socket, buffer + 2, length) != length)
        return -1;

    return length + 2;
}
#endif /*  (CONFIG_CS104_SUPPORT_TLS == 1) */

static inline int
receiveMessageSocket(Socket socket, uint8_t* buffer)
{
    int readFirst = Socket_read(socket, buffer, 1);

    if (readFirst < 1)
        return readFirst;

    if (buffer[0] != 0x68)
        return -1; /* message error */

    if (Socket_read(socket, buffer + 1, 1) != 1)
        return -1;

    int length = buffer[1];

    /* read remaining frame */
    if (Socket_read(socket, buffer + 2, length) != length)
        return -1;

    return length + 2;
}

static int
receiveMessage(MasterConnection self, uint8_t* buffer)
{
#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket != NULL)
        return receiveMessageTlsSocket(self->tlsSocket, buffer);
    else
        return receiveMessageSocket(self->socket, buffer);
#else
    return receiveMessageSocket(self->socket, buffer);
#endif
}

static inline int
writeToSocket(MasterConnection self, uint8_t* buf, int size)
{
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
    buffer[0] = (uint8_t) 0x68;
    buffer[1] = (uint8_t) (msgSize - 2);

    buffer[2] = (uint8_t) ((self->sendCount % 128) * 2);
    buffer[3] = (uint8_t) (self->sendCount / 128);

    buffer[4] = (uint8_t) ((self->receiveCount % 128) * 2);
    buffer[5] = (uint8_t) (self->receiveCount / 128);

    if (writeToSocket(self, buffer, msgSize) != -1) {
        DEBUG_PRINT("SEND I (size = %i) N(S) = %i N(R) = %i\n", msgSize, self->sendCount, self->receiveCount);
        self->sendCount = (self->sendCount + 1) % 32768;
        self->unconfirmedReceivedIMessages = 0;
    }
    else
        self->isRunning = false;

    self->unconfirmedReceivedIMessages = 0;

    return self->sendCount;
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
sendASDU(MasterConnection self, FrameBuffer* asdu, uint64_t timestamp, int index)
{
    int currentIndex = 0;

    if (self->oldestSentASDU == -1) {
        self->oldestSentASDU = 0;
        self->newestSentASDU = 0;
    }
    else {
        currentIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;
    }

    self->sentASDUs[currentIndex].entryTime = timestamp;
    self->sentASDUs[currentIndex].queueIndex = index;
    self->sentASDUs[currentIndex].seqNo = sendIMessage(self, asdu->msg, asdu->msgSize);
    self->sentASDUs[currentIndex].sentTime = Hal_getTimeInMs();

    self->newestSentASDU = currentIndex;

    printSendBuffer(self);
}


static bool
sendASDUInternal(MasterConnection self, CS101_ASDU asdu)
{
    bool asduSent;

    if (self->isActive) {

#if (CONFIG_USE_THREADS == 1)
        Semaphore_wait(self->sentASDUsLock);
#endif

        if (isSentBufferFull(self) == false) {

            FrameBuffer frameBuffer;

            struct sBufferFrame bufferFrame;

            Frame frame = BufferFrame_initialize(&bufferFrame, frameBuffer.msg, IEC60870_5_104_APCI_LENGTH);
            CS101_ASDU_encode(asdu, frame);

            frameBuffer.msgSize = Frame_getMsgSize(frame);

            sendASDU(self, &frameBuffer, 0, -1);

#if (CONFIG_USE_THREADS == 1)
            Semaphore_post(self->sentASDUsLock);
#endif

            asduSent = true;
        }
        else {
#if (CONFIG_USE_THREADS == 1)
            Semaphore_post(self->sentASDUsLock);
#endif
            asduSent = HighPriorityASDUQueue_enqueue(self->highPrioQueue, asdu);
        }

    }
    else
        asduSent = false;

    if (asduSent == false)
        DEBUG_PRINT("unable to send response (isActive=%i)\n", self->isActive);

    return asduSent;
}


static void responseCOTUnknown(CS101_ASDU asdu, MasterConnection self)
{
    DEBUG_PRINT("  with unknown COT\n");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
    sendASDUInternal(self, asdu);
}

/*
 * Handle received ASDUs
 *
 * Call the appropriate callbacks according to ASDU type and CoT
 */
static void
handleASDU(MasterConnection self, CS101_ASDU asdu)
{
    bool messageHandled = false;

    CS104_Slave slave = self->slave;

    uint8_t cot = CS101_ASDU_getCOT(asdu);

    switch (CS101_ASDU_getTypeID(asdu)) {

    case C_IC_NA_1: /* 100 - interrogation command */

        DEBUG_PRINT("Rcvd interrogation command C_IC_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION)) {
            if (slave->interrogationHandler != NULL) {

                InterrogationCommand irc = (InterrogationCommand) CS101_ASDU_getElement(asdu, 0);

                if (slave->interrogationHandler(slave->interrogationHandlerParameter,
                        &(self->iMasterConnection), asdu, InterrogationCommand_getQOI(irc)))
                    messageHandled = true;

                InterrogationCommand_destroy(irc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CI_NA_1: /* 101 - counter interrogation command */

        DEBUG_PRINT("Rcvd counter interrogation command C_CI_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION)) {

            if (slave->counterInterrogationHandler != NULL) {

                CounterInterrogationCommand cic = (CounterInterrogationCommand) CS101_ASDU_getElement(asdu, 0);


                if (slave->counterInterrogationHandler(slave->counterInterrogationHandlerParameter,
                        &(self->iMasterConnection), asdu, CounterInterrogationCommand_getQCC(cic)))
                    messageHandled = true;

                CounterInterrogationCommand_destroy(cic);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_RD_NA_1: /* 102 - read command */

        DEBUG_PRINT("Rcvd read command C_RD_NA_1\n");

        if (cot == CS101_COT_REQUEST) {
            if (slave->readHandler != NULL) {
                ReadCommand rc = (ReadCommand) CS101_ASDU_getElement(asdu, 0);

                if (slave->readHandler(slave->readHandlerParameter,
                        &(self->iMasterConnection), asdu, InformationObject_getObjectAddress((InformationObject) rc)))
                    messageHandled = true;

                ReadCommand_destroy(rc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        DEBUG_PRINT("Rcvd clock sync command C_CS_NA_1\n");

        if (cot == CS101_COT_ACTIVATION) {

            if (slave->clockSyncHandler != NULL) {

                ClockSynchronizationCommand csc = (ClockSynchronizationCommand) CS101_ASDU_getElement(asdu, 0);

                if (slave->clockSyncHandler(slave->clockSyncHandlerParameter,
                        &(self->iMasterConnection), asdu, ClockSynchronizationCommand_getTime(csc)))
                    messageHandled = true;

                ClockSynchronizationCommand_destroy(csc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_TS_NA_1: /* 104 - test command */

        DEBUG_PRINT("Rcvd test command C_TS_NA_1\n");

        if (cot != CS101_COT_ACTIVATION)
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
        else
            CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);

        sendASDUInternal(self, asdu);

        messageHandled = true;

        break;

    case C_RP_NA_1: /* 105 - Reset process command */

        DEBUG_PRINT("Rcvd reset process command C_RP_NA_1\n");

        if (cot == CS101_COT_ACTIVATION) {

            if (slave->resetProcessHandler != NULL) {
                ResetProcessCommand rpc = (ResetProcessCommand) CS101_ASDU_getElement(asdu, 0);

                if (slave->resetProcessHandler(slave->resetProcessHandlerParameter,
                        &(self->iMasterConnection), asdu, ResetProcessCommand_getQRP(rpc)))
                    messageHandled = true;

                ResetProcessCommand_destroy(rpc);
            }

        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CD_NA_1: /* 106 - Delay acquisition command */

        DEBUG_PRINT("Rcvd delay acquisition command C_CD_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_SPONTANEOUS)) {

            if (slave->delayAcquisitionHandler != NULL) {
                DelayAcquisitionCommand dac = (DelayAcquisitionCommand) CS101_ASDU_getElement(asdu, 0);

                if (slave->delayAcquisitionHandler(slave->delayAcquisitionHandlerParameter,
                        &(self->iMasterConnection), asdu, DelayAcquisitionCommand_getDelay(dac)))
                    messageHandled = true;

                DelayAcquisitionCommand_destroy(dac);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;


    default: /* no special handler available -> use default handler */
        break;
    }

    if ((messageHandled == false) && (slave->asduHandler != NULL))
        if (slave->asduHandler(slave->asduHandlerParameter, &(self->iMasterConnection), asdu))
            messageHandled = true;

    if (messageHandled == false) {
        /* send error response */
        CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
        sendASDUInternal(self, asdu);
    }
}



static bool
checkSequenceNumber(MasterConnection self, int seqNo)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    /* check if received sequence number is valid */

    bool seqNoIsValid = false;
    bool counterOverflowDetected = false;

    if (self->oldestSentASDU == -1) { /* if k-Buffer is empty */
        if (seqNo == self->sendCount)
            seqNoIsValid = true;
    }
    else {
        /* two cases are required to reflect sequence number overflow */
        int oldestAsduSeqNo = self->sentASDUs[self->oldestSentASDU].seqNo;
        int newestAsduSeqNo = self->sentASDUs[self->newestSentASDU].seqNo;

        if (oldestAsduSeqNo <= newestAsduSeqNo) {
            if ((seqNo >= oldestAsduSeqNo) && (seqNo <= newestAsduSeqNo))
                seqNoIsValid = true;
        }
        else {
            if ((seqNo >= oldestAsduSeqNo) || (seqNo <= newestAsduSeqNo))
                seqNoIsValid = true;

            counterOverflowDetected = true;
        }

        int latestValidSeqNo = (oldestAsduSeqNo - 1) % 32768;

        if (latestValidSeqNo == seqNo)
            seqNoIsValid = true;
    }

    if (seqNoIsValid) {
        if (self->oldestSentASDU != -1) {

            do {
                int oldestAsduSeqNo = self->sentASDUs[self->oldestSentASDU].seqNo;

                if (counterOverflowDetected == false) {
                    if (seqNo < oldestAsduSeqNo)
                        break;
                }
                else {
                    if (seqNo == ((oldestAsduSeqNo - 1) % 32768))
                        break;
                }

                /* remove from server (low-priority) queue if required */
                if (self->sentASDUs[self->oldestSentASDU].queueIndex != -1) {

                    MessageQueue_markAsduAsConfirmed(self->lowPrioQueue,
                            self->sentASDUs[self->oldestSentASDU].queueIndex,
                            self->sentASDUs[self->oldestSentASDU].entryTime);
                }

                self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                int checkIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

                if (self->oldestSentASDU == checkIndex) {
                    self->oldestSentASDU = -1;
                    break;
                }

                if (self->sentASDUs[self->oldestSentASDU].seqNo == seqNo) {
                    /* we arrived at the seq# that has been confirmed */

                    if (self->oldestSentASDU == self->newestSentASDU)
                        self->oldestSentASDU = -1;
                    else
                        self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                    break;
                }

            } while (true);

        }
    }
    else
        DEBUG_PRINT("Received sequence number out of range");


#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return seqNoIsValid;
}

static void
resetT3Timeout(MasterConnection self)
{
    self->nextT3Timeout = Hal_getTimeInMs() + (uint64_t) (self->slave->conParameters.t3 * 1000);
}


static bool
handleMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    uint64_t currentTime = Hal_getTimeInMs();

    if ((buffer[2] & 1) == 0) {

        if (msgSize < 7) {
            DEBUG_PRINT("Received I msg too small!\n");
            return false;
        }

        if (self->firstIMessageReceived == false) {
            self->firstIMessageReceived = true;
            self->lastConfirmationTime = currentTime; /* start timeout T2 */
        }

        int frameSendSequenceNumber = ((buffer [3] * 0x100) + (buffer [2] & 0xfe)) / 2;
        int frameRecvSequenceNumber = ((buffer [5] * 0x100) + (buffer [4] & 0xfe)) / 2;

        DEBUG_PRINT("Received I frame: N(S) = %i N(R) = %i\n", frameSendSequenceNumber, frameRecvSequenceNumber);

        if (frameSendSequenceNumber != self->receiveCount) {
            DEBUG_PRINT("Sequence error: Close connection!");
            return false;
        }

        if (checkSequenceNumber (self, frameRecvSequenceNumber) == false) {
            DEBUG_PRINT("Sequence number check failed");
            return false;
        }

        self->receiveCount = (self->receiveCount + 1) % 32768;
        self->unconfirmedReceivedIMessages++;

        if (self->isActive) {

            CS101_ASDU asdu = CS101_ASDU_createFromBuffer(&(self->slave->alParameters), buffer + 6, msgSize - 6);

            handleASDU(self, asdu);

            CS101_ASDU_destroy(asdu);
        }
        else
            DEBUG_PRINT("Connection not activated. Skip I message");

    }

    /* Check for TESTFR_ACT message */
    else if ((buffer[2] & 0x43) == 0x43) {
        DEBUG_PRINT("Send TESTFR_CON\n");

        writeToSocket(self, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
    }

    /* Check for STARTDT_ACT message */
    else if ((buffer [2] & 0x07) == 0x07) {
        DEBUG_PRINT("Send STARTDT_CON\n");

        CS104_Slave_activate(self->slave, self);

        self->isActive = true;

        HighPriorityASDUQueue_resetConnectionQueue(self->highPrioQueue);

        writeToSocket(self, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
    }

    /* Check for STOPDT_ACT message */
    else if ((buffer [2] & 0x13) == 0x13) {
        DEBUG_PRINT("Send STOPDT_CON\n");

        self->isActive = false;

        writeToSocket(self, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE);
    }

    /* Check for TESTFR_CON message */
    else if ((buffer[2] & 0x83) == 0x83) {
        DEBUG_PRINT("Recv TESTFR_CON\n");

        self->outstandingTestFRConMessages = 0;
    }

    else if (buffer [2] == 0x01) { /* S-message */
        int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

        DEBUG_PRINT("Rcvd S(%i) (own sendcounter = %i)\n", seqNo, self->sendCount);

        if (checkSequenceNumber(self, seqNo) == false)
            return false;
    }

    else {
        DEBUG_PRINT("unknown message - IGNORE\n");
        return true;
    }

    resetT3Timeout(self);

    return true;
}



static void
sendSMessage(MasterConnection self)
{
    uint8_t msg[6];

    msg[0] = 0x68;
    msg[1] = 0x04;
    msg[2] = 0x01;
    msg[3] = 0;
    msg[4] = (uint8_t) ((self->receiveCount % 128) * 2);
    msg[5] = (uint8_t) (self->receiveCount / 128);

    writeToSocket(self, msg, 6);
}

static void
MasterConnection_destroy(MasterConnection self)
{
    if (self) {

#if (CONFIG_CS104_SUPPORT_TLS == 1)
      if (self->tlsSocket != NULL)
          TLSSocket_close(self->tlsSocket);
#endif

        Socket_destroy(self->socket);

        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_USE_THREADS == 1)
        Semaphore_destroy(self->sentASDUsLock);
#endif

        GLOBAL_FREEMEM(self);
    }
}


static void
sendNextLowPriorityASDU(MasterConnection self)
{
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    FrameBuffer* asdu;

    if (isSentBufferFull(self))
        goto exit_function;

    MessageQueue_lock(self->lowPrioQueue);

    uint64_t timestamp;
    int queueIndex;

    asdu = MessageQueue_getNextWaitingASDU(self->lowPrioQueue, &timestamp, &queueIndex);

    if (asdu != NULL)
        sendASDU(self, asdu, timestamp, queueIndex);

    MessageQueue_unlock(self->lowPrioQueue);

exit_function:
#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif
}

static bool
sendNextHighPriorityASDU(MasterConnection self)
{
    bool retVal = false;

    FrameBuffer* msg;

#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (isSentBufferFull(self))
        goto exit_function;

    HighPriorityASDUQueue_lock(self->highPrioQueue);

    msg = HighPriorityASDUQueue_getNextASDU(self->highPrioQueue);

    if (msg != NULL) {
        sendASDU(self, msg, 0, -1);
        retVal = true;
    }

    HighPriorityASDUQueue_unlock(self->highPrioQueue);

exit_function:
#if (CONFIG_USE_THREADS == 1)
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
    while (HighPriorityASDUQueue_isAsduAvailable(self->highPrioQueue)) {

        if (sendNextHighPriorityASDU(self) == false)
            return true;

        if (self->isRunning == false)
            return true;
    }

    /* send messages from low-priority queue */
    sendNextLowPriorityASDU(self);

    if (MessageQueue_isAsduAvailable(self->lowPrioQueue))
        return true;
    else
        return false;
}

static bool
handleTimeouts(MasterConnection self)
{
    uint64_t currentTime = Hal_getTimeInMs();

    bool timeoutsOk = true;

    /* check T3 timeout */
    if (currentTime > self->nextT3Timeout) {

        if (self->outstandingTestFRConMessages > 2) {
            DEBUG_PRINT("Timeout for TESTFR CON message\n");

            /* close connection */
            timeoutsOk = false;
        }
        else {
            if (writeToSocket(self, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE) == -1)
                self->isRunning = false;

            self->outstandingTestFRConMessages++;
            resetT3Timeout(self);
        }
    }

    /* check timeout for others station I messages */
    if (self->unconfirmedReceivedIMessages > 0) {
        if ((currentTime - self->lastConfirmationTime) >= (uint64_t) (self->slave->conParameters.t2 * 1000)) {
            self->lastConfirmationTime = currentTime;
            self->unconfirmedReceivedIMessages = 0;
            sendSMessage(self);
        }
    }

    /* check if counterpart confirmed I message */
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (self->oldestSentASDU != -1) {
        if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >= (uint64_t) (self->slave->conParameters.t1 * 1000)) {
            timeoutsOk = false;

            printSendBuffer(self);

            DEBUG_PRINT("I message timeout for %i seqNo: %i\n", self->oldestSentASDU,
                    self->sentASDUs[self->oldestSentASDU].seqNo);
        }
    }

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return timeoutsOk;
}

static void
CS104_Slave_removeConnection(CS104_Slave self, MasterConnection connection)
{
#if (CONFIG_USE_THREADS)
    Semaphore_wait(self->openConnectionsLock);
#endif

    self->openConnections--;
    LinkedList_remove(self->masterConnections, (void*) connection);

    MasterConnection_destroy(connection);

#if (CONFIG_USE_THREADS)
    Semaphore_post(self->openConnectionsLock);
#endif


}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection) parameter;

    uint8_t buffer[260];

    self->isRunning = true;

    resetT3Timeout(self);

    HandleSet handleSet = Handleset_new();

    bool isAsduWaiting = false;

    while (self->isRunning) {

        Handleset_reset(handleSet);
        Handleset_addSocket(handleSet, self->socket);

        int socketTimeout;

        /*
         * When an ASDU is waiting only have a short look to see if a client request
         * was received. Otherwise wait to save CPU time.
         */
        if (isAsduWaiting)
            socketTimeout = 1;
        else
            socketTimeout = 100; /* TODO replace by configurable parameter */

        if (Handleset_waitReady(handleSet, socketTimeout)) {

            int bytesRec = receiveMessage(self, buffer);

            if (bytesRec == -1) {
                DEBUG_PRINT("Error reading from socket\n");
                break;
            }

            if (bytesRec > 0) {
                DEBUG_PRINT("Connection: rcvd msg(%i bytes)\n", bytesRec);

                if (handleMessage(self, buffer, bytesRec) == false)
                    self->isRunning = false;

                if (self->unconfirmedReceivedIMessages >= self->slave->conParameters.w) {

                    self->lastConfirmationTime = Hal_getTimeInMs();

                    self->unconfirmedReceivedIMessages = 0;

                    sendSMessage(self);
                }
            }
        }

        if (handleTimeouts(self) == false)
            self->isRunning = false;

        if (self->isRunning)
            if (self->isActive)
                isAsduWaiting = sendWaitingASDUs(self);
    }

    DEBUG_PRINT("Connection closed\n");

    Handleset_destroy(handleSet);

    self->isRunning = false;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->slave->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP) {
        MessageQueue_destroy(self->lowPrioQueue);
        HighPriorityASDUQueue_destroy(self->highPrioQueue);
    }
#endif

    CS104_Slave_removeConnection(self->slave, self);

    return NULL;
}

/********************************************
 * IMasterConnection
 *******************************************/

static void
_sendASDU(IMasterConnection self, CS101_ASDU asdu)
{
    MasterConnection con = (MasterConnection) self->object;

    sendASDUInternal(con, asdu);
}

static void
_sendACT_CON(IMasterConnection self, CS101_ASDU asdu, bool negative)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
    CS101_ASDU_setNegative(asdu, negative);

    _sendASDU(self, asdu);
}

static void
_sendACT_TERM(IMasterConnection self, CS101_ASDU asdu)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_TERMINATION);
    CS101_ASDU_setNegative(asdu, false);

    _sendASDU(self, asdu);
}

static CS101_AppLayerParameters
_getApplicationLayerParameters(IMasterConnection self)
{
    MasterConnection con = (MasterConnection) self->object;

    return &(con->slave->alParameters);
}

/********************************************
 * END IMasterConnection
 *******************************************/

static MasterConnection
MasterConnection_create(CS104_Slave slave, Socket socket, MessageQueue lowPrioQueue, HighPriorityASDUQueue highPrioQueue)
{
    MasterConnection self = (MasterConnection) GLOBAL_MALLOC(sizeof(struct sMasterConnection));

    if (self != NULL) {

        self->slave = slave;
        self->socket = socket;
        self->isActive = false;
        self->isRunning = false;
        self->receiveCount = 0;
        self->sendCount = 0;

        self->unconfirmedReceivedIMessages = 0;
        self->lastConfirmationTime = UINT64_MAX;

        self->firstIMessageReceived = false;

        self->maxSentASDUs = slave->conParameters.k;
        self->oldestSentASDU = -1;
        self->newestSentASDU = -1;
        self->sentASDUs = (SentASDUSlave*) GLOBAL_MALLOC(sizeof(SentASDUSlave) * self->maxSentASDUs);

        self->iMasterConnection.object = self;
        self->iMasterConnection.getApplicationLayerParameters = _getApplicationLayerParameters;
        self->iMasterConnection.sendASDU = _sendASDU;
        self->iMasterConnection.sendACT_CON = _sendACT_CON;
        self->iMasterConnection.sendACT_TERM = _sendACT_TERM;

#if (CONFIG_USE_THREADS == 1)
        self->sentASDUsLock = Semaphore_create(1);
#endif

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        if (slave->tlsConfig != NULL) {
            self->tlsSocket = TLSSocket_create(socket, slave->tlsConfig);

            if (self->tlsSocket == NULL) {
                printf("Close connection\n");

                MasterConnection_destroy(self);
                return NULL;
            }
        }
        else
            self->tlsSocket = NULL;
#endif

        self->lowPrioQueue = lowPrioQueue;
        self->highPrioQueue = highPrioQueue;

        self->outstandingTestFRConMessages = 0;

        Thread newThread =
               Thread_create((ThreadExecutionFunction) connectionHandlingThread,
                       (void*) self, true);

        Thread_start(newThread);
    }

    return self;
}

void
MasterConnection_close(MasterConnection self)
{
    self->isRunning = false;
}

void
MasterConnection_deactivate(MasterConnection self)
{
    self->isActive = false;
}



static void*
serverThread (void* parameter)
{
    CS104_Slave self = (CS104_Slave) parameter;

    ServerSocket serverSocket;

    if (self->localAddress)
        serverSocket = TcpServerSocket_create(self->localAddress, self->tcpPort);
    else
        serverSocket = TcpServerSocket_create("0.0.0.0", self->tcpPort);

    if (serverSocket == NULL) {
        DEBUG_PRINT("Cannot create server socket\n");
        self->isStarting = false;
        goto exit_function;
    }

    ServerSocket_listen(serverSocket);

    self->isRunning = true;
    self->isStarting = false;

    while (self->stopRunning == false) {
        Socket newSocket = ServerSocket_accept(serverSocket);

        if (newSocket != NULL) {

            bool acceptConnection = true;

            /* check if maximum number of open connections is reached */
            if (self->maxOpenConnections > 0) {
                if (self->openConnections >= self->maxOpenConnections)
                    acceptConnection = false;
            }

            if (acceptConnection && (self->connectionRequestHandler != NULL)) {
                char ipAddress[60];

                Socket_getPeerAddressStatic(newSocket, ipAddress);

                /* remove TCP port part */
                char* separator = strchr(ipAddress, ':');
                if (separator != NULL)
                    *separator = 0;

                acceptConnection = self->connectionRequestHandler(self->connectionRequestHandlerParameter,
                        ipAddress);
            }

            if (acceptConnection) {

                MessageQueue lowPrioQueue = NULL;
                HighPriorityASDUQueue highPrioQueue = NULL;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
                if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP) {
                    lowPrioQueue = self->asduQueue;
                    highPrioQueue = self->connectionAsduQueue;
                }
#endif

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
                if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP) {
                    lowPrioQueue = MessageQueue_create(self->maxLowPrioQueueSize);
                    highPrioQueue = HighPriorityASDUQueue_create(self->maxHighPrioQueueSize);
                }
#endif

                MasterConnection connection =
                        MasterConnection_create(self, newSocket, lowPrioQueue, highPrioQueue);

                if (connection) {

#if (CONFIG_USE_THREADS)
                    Semaphore_wait(self->openConnectionsLock);
#endif

                    self->openConnections++;
                    LinkedList_add(self->masterConnections, connection);

#if (CONFIG_USE_THREADS)
                    Semaphore_post(self->openConnectionsLock);
#endif
                }
                else
                    DEBUG_PRINT("Connection attempt failed!");

            }
            else {
                Socket_destroy(newSocket);
            }
        }
        else
            Thread_sleep(10);
    }

    if (serverSocket)
        Socket_destroy((Socket) serverSocket);

    self->isRunning = false;
    self->stopRunning = false;

exit_function:
    return NULL;
}

void
CS104_Slave_enqueueASDU(CS104_Slave self, CS101_ASDU asdu)
{
#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
        MessageQueue_enqueueASDU(self->asduQueue, asdu);
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP) {

#if (CONFIG_USE_THREADS == 1)
        Semaphore_wait(self->openConnectionsLock);
#endif

        /************************************************
         * Dispatch event to all open client connections
         ************************************************/

        LinkedList element;

        for (element = LinkedList_getNext(self->masterConnections);
             element != NULL;
             element = LinkedList_getNext(element))
        {
            MasterConnection connection = (MasterConnection) LinkedList_getData(element);

            MessageQueue_enqueueASDU(connection->lowPrioQueue, asdu);
        }

#if (CONFIG_USE_THREADS == 1)
        Semaphore_post(self->openConnectionsLock);
#endif
    }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */
}

void
CS104_Slave_start(CS104_Slave self)
{
    if (self->isRunning == false) {

        self->isStarting = true;
        self->isRunning = false;
        self->stopRunning = false;

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
        if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
            initializeMessageQueues(self, self->maxLowPrioQueueSize, self->maxHighPrioQueueSize);
#endif
        self->listeningThread = Thread_create(serverThread, (void*) self, false);

        Thread_start(self->listeningThread);

        while (self->isStarting)
            Thread_sleep(1);
    }
}

bool
CS104_Slave_isRunning(CS104_Slave self)
{
    return self->isRunning;
}

void
CS104_Slave_stop(CS104_Slave self)
{
    if (self->isRunning) {
        self->stopRunning = true;

        while (self->isRunning)
            Thread_sleep(1);
    }

    if (self->listeningThread) {
        Thread_destroy(self->listeningThread);
    }

    self->listeningThread = NULL;
}

void
CS104_Slave_destroy(CS104_Slave self)
{
    if (self->isRunning)
        CS104_Slave_stop(self);

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP)
        MessageQueue_releaseAllQueuedASDUs(self->asduQueue);
#endif

    if (self->localAddress != NULL)
        GLOBAL_FREEMEM(self->localAddress);

    /*
     * Stop all connections
     * */
#if (CONFIG_USE_THREADS == 1)
    Semaphore_wait(self->openConnectionsLock);
#endif

    LinkedList element;

    for (element = LinkedList_getNext(self->masterConnections);
         element != NULL;
         element = LinkedList_getNext(element))
    {
        MasterConnection connection = (MasterConnection) LinkedList_getData(element);

        MasterConnection_close(connection);
    }

#if (CONFIG_USE_THREADS == 1)
    Semaphore_post(self->openConnectionsLock);
#endif

    /* Wait until all connections are closed */
    while (CS104_Slave_getOpenConnections(self) > 0)
        Thread_sleep(10);

    LinkedList_destroyStatic(self->masterConnections);

#if (CONFIG_USE_THREADS == 1)
    Semaphore_destroy(self->openConnectionsLock);
#endif

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 0)

#if (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1)
    if (self->serverMode == CS104_MODE_SINGLE_REDUNDANCY_GROUP) {
        MessageQueue_destroy(self->asduQueue);
        HighPriorityASDUQueue_destroy(self->connectionAsduQueue);
    }
#endif /* (CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP == 1) */
    GLOBAL_FREEMEM(self);

#endif /* (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 0) */
}
