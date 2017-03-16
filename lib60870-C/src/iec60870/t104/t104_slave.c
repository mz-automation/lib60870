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

#include "iec60870_slave.h"
#include "frame.h"
#include "t104_frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"
#include "buffer_frame.h"

#include "lib60870_config.h"
#include "lib60870_internal.h"

#include "apl_types_internal.h"

#define T104_DEFAULT_PORT 2404

//TODO refactor: move to separate file/class
static struct sT104ConnectionParameters defaultConnectionParameters = {
	/* .sizeOfTypeId =  */ 1,
	/* .sizeOfVSQ = */ 1,
	/* .sizeOfCOT = */ 2,
	/* .originatorAddress = */ 0,
	/* .sizeOfCA = */ 2,
	/* .sizeOfIOA = */ 3,

	/* .k = */ 12,
	/* .w = */ 8,
	/* .t0 = */ 10,
	/* .t1 = */ 15,
	/* .t2 = */ 10,
	/* .t3 = */ 20
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

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore queueLock;
#endif
};


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

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore queueLock;
#endif
};


struct sSlave {
    InterrogationHandler interrogationHandler;
    void* interrogationHandlerParameter;

    CounterInterrogationHandler counterInterrogationHandler;
    void* counterInterrogationHandlerParameter;

    ReadHandler readHandler;
    void* readHandlerParameter;

    ClockSynchronizationHandler clockSyncHandler;
    void* clockSyncHandlerParameter;

    ResetProcessHandler resetProcessHandler;
    void* resetProcessHandlerParameter;

    DelayAcquisitionHandler delayAcquisitionHandler;
    void* delayAcquisitionHandlerParameter;

    ASDUHandler asduHandler;
    void* asduHandlerParameter;

    struct sMessageQueue asduQueue; /* low priority ASDU queue and buffer */

    struct sHighPriorityASDUQueue connectionAsduQueue; /* high priority ASDU queue */

    struct sT104ConnectionParameters parameters;

    bool isStarting;
    bool isRunning;
    bool stopRunning;

    int openConnections;
    int tcpPort;

    char* localAddress;
    Thread listeningThread;
};

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)

static struct sSlave singleStaticSlaveInstance;

#endif

static uint8_t STARTDT_CON_MSG[] = { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

#define STARTDT_CON_MSG_SIZE 6

static uint8_t STOPDT_CON_MSG[] = { 0x68, 0x04, 0x23, 0x00, 0x00, 0x00 };

#define STOPDT_CON_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

#define TESTFR_CON_MSG_SIZE 6

static uint8_t TESTFR_ACT_MSG[] = { 0x68, 0x04, 0x43, 0x00, 0x00, 0x00 };

#define TESTFR_ACT_MSG_SIZE 6


static void
initializeMessageQueues(Slave self, int lowPrioMaxQueueSize, int highPrioMaxQueueSize)
{

    /* initialized low priority queue */

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    lowPrioMaxQueueSize = CONFIG_SLAVE_ASDU_QUEUE_SIZE;
#else
    if (lowPrioMaxQueueSize < 1)
        lowPrioMaxQueueSize = CONFIG_SLAVE_ASDU_QUEUE_SIZE;

    self->asduQueue.asdus = GLOBAL_CALLOC(lowPrioMaxQueueSize, sizeof(struct sASDUQueueEntry));
#endif

    self->asduQueue.entryCounter = 0;
    self->asduQueue.firstMsgIndex = 0;
    self->asduQueue.lastMsgIndex = 0;
    self->asduQueue.size = lowPrioMaxQueueSize;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    self->asduQueue.queueLock = Semaphore_create(1);
#endif

    /* initialize high priority queue */

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    highPrioMaxQueueSize = CONFIG_SLAVE_CONNECTION_ASDU_QUEUE_SIZE;
#else
    if (highPrioMaxQueueSize < 1)
        highPrioMaxQueueSize = CONFIG_SLAVE_CONNECTION_ASDU_QUEUE_SIZE;

    self->connectionAsduQueue.asdus = (FrameBuffer*) GLOBAL_CALLOC(highPrioMaxQueueSize, sizeof(FrameBuffer));
#endif

    self->connectionAsduQueue.entryCounter = 0;
    self->connectionAsduQueue.firstMsgIndex = 0;
    self->connectionAsduQueue.lastMsgIndex = 0;
    self->connectionAsduQueue.size = highPrioMaxQueueSize;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    self->connectionAsduQueue.queueLock = Semaphore_create(1);
#endif
}

Slave
T104Slave_create(ConnectionParameters parameters, int maxQueueSize, int maxHighPrioQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    Slave self = &(singleStaticSlaveInstance);
#else
    Slave self = (Slave) GLOBAL_MALLOC(sizeof(struct sSlave));
#endif

    if (self != NULL) {

        initializeMessageQueues(self, maxQueueSize, maxHighPrioQueueSize);

        if (parameters != NULL)
            self->parameters = *((T104ConnectionParameters) parameters);
        else
            self->parameters = defaultConnectionParameters;

        self->asduHandler = NULL;
        self->interrogationHandler = NULL;
        self->counterInterrogationHandler = NULL;
        self->readHandler = NULL;
        self->clockSyncHandler = NULL;
        self->resetProcessHandler = NULL;
        self->delayAcquisitionHandler = NULL;

        self->isRunning = false;
        self->stopRunning = false;

        self->localAddress = NULL;
        self->tcpPort = T104_DEFAULT_PORT;
        self->openConnections = 0;
        self->listeningThread = NULL;
    }

    return self;
}

void
T104Slave_setLocalAddress(Slave self, const char* ipAddress)
{
    if (self->localAddress)
        GLOBAL_FREEMEM(self->localAddress);

    self->localAddress = (char*) GLOBAL_MALLOC(strlen(ipAddress) + 1);

    if (self->localAddress)
        strcpy(self->localAddress, ipAddress);
}

void
T104Slave_setLocalPort(Slave self, int tcpPort)
{
    self->tcpPort = tcpPort;
}

int
T104Slave_getOpenConnections(Slave self)
{
    return self->openConnections;
}

void
Slave_setInterrogationHandler(Slave self, InterrogationHandler handler, void*  parameter)
{
    self->interrogationHandler = handler;
    self->interrogationHandlerParameter = parameter;
}

void
Slave_setCounterInterrogationHandler(Slave self, CounterInterrogationHandler handler, void*  parameter)
{
    self->counterInterrogationHandler = handler;
    self->counterInterrogationHandlerParameter = parameter;
}

void
Slave_setReadHandler(Slave self, ReadHandler handler, void* parameter)
{
    self->readHandler = handler;
    self->readHandlerParameter = parameter;
}

void
Slave_setASDUHandler(Slave self, ASDUHandler handler, void* parameter)
{
    self->asduHandler = handler;
    self->asduHandlerParameter = parameter;
}

void
Slave_setClockSyncHandler(Slave self, ClockSynchronizationHandler handler, void* parameter)
{
    self->clockSyncHandler = handler;
    self->clockSyncHandlerParameter = parameter;
}

ConnectionParameters
Slave_getConnectionParameters(Slave self)
{
    return (ConnectionParameters) &(self->parameters);
}

void
Slave_enqueueASDU(Slave self, ASDU asdu)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->asduQueue.queueLock);
#endif

    int nextIndex;
    bool removeEntry = false;

    if (self->asduQueue.entryCounter == 0) {
        self->asduQueue.firstMsgIndex = 0;
        nextIndex = 0;
    }
    else
        nextIndex = self->asduQueue.lastMsgIndex + 1;

    if (nextIndex == self->asduQueue.size)
        nextIndex = 0;

    if (self->asduQueue.entryCounter == self->asduQueue.size)
        removeEntry = true;

    if (removeEntry == false) {
        DEBUG_PRINT("add entry (nextIndex:%i)\n", nextIndex);
        self->asduQueue.lastMsgIndex = nextIndex;
        self->asduQueue.entryCounter++;
    }
    else {
        DEBUG_PRINT("add entry (nextIndex:%i) -> remove oldest\n", nextIndex);

        /* remove oldest entry */
        self->asduQueue.lastMsgIndex = nextIndex;

        int firstIndex = nextIndex + 1;

        if (firstIndex == self->asduQueue.size)
            firstIndex = 0;

        self->asduQueue.firstMsgIndex = firstIndex;
    }

    struct sBufferFrame bufferFrame;

    Frame frame = BufferFrame_initialize(&bufferFrame, self->asduQueue.asdus[nextIndex].asdu.msg,
                                            IEC60870_5_104_APCI_LENGTH);

    ASDU_encode(asdu, frame);

    self->asduQueue.asdus[nextIndex].asdu.msgSize = Frame_getMsgSize(frame);
    self->asduQueue.asdus[nextIndex].entryTimestamp = Hal_getTimeInMs();
    self->asduQueue.asdus[nextIndex].state = QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION;

    ASDU_destroy(asdu);

    DEBUG_PRINT("ASDUs in FIFO: %i (first: %i, last: %i)\n", self->asduQueue.entryCounter,
            self->asduQueue.firstMsgIndex, self->asduQueue.lastMsgIndex);

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->asduQueue.queueLock);
#endif

    //TODO trigger active connection to send message
}



static bool
Slave_isHighPrioASDUAvailable(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->connectionAsduQueue.queueLock);
#endif

    bool retVal;

    if (self->connectionAsduQueue.entryCounter > 0)
        retVal = true;
    else
        retVal = false;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->connectionAsduQueue.queueLock);
#endif

    return retVal;
}

static bool
Slave_enqueueHighPrioASDU(Slave self, ASDU asdu)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->connectionAsduQueue.queueLock);
#endif

    bool enqueued = false;

    if (self->connectionAsduQueue.entryCounter == self->connectionAsduQueue.size)
        goto exit_function;

    int nextIndex;

    if (self->connectionAsduQueue.entryCounter == 0) {
        self->connectionAsduQueue.firstMsgIndex = 0;
        nextIndex = 0;
    }
    else
        nextIndex = self->connectionAsduQueue.lastMsgIndex + 1;

    if (nextIndex == self->connectionAsduQueue.size)
        nextIndex = 0;

    DEBUG_PRINT("HighPrio queue: add entry (nextIndex:%i)\n", nextIndex);
    self->connectionAsduQueue.lastMsgIndex = nextIndex;
    self->connectionAsduQueue.entryCounter++;

    struct sBufferFrame bufferFrame;

    Frame frame = BufferFrame_initialize(&bufferFrame, self->connectionAsduQueue.asdus[nextIndex].msg,
                                            IEC60870_5_104_APCI_LENGTH);

    ASDU_encode(asdu, frame);

    self->connectionAsduQueue.asdus[nextIndex].msgSize = Frame_getMsgSize(frame);

    DEBUG_PRINT("ASDUs in HighPrio FIFO: %i (first: %i, last: %i)\n", self->connectionAsduQueue.entryCounter,
            self->connectionAsduQueue.firstMsgIndex, self->connectionAsduQueue.lastMsgIndex);

    enqueued = true;

exit_function:

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->connectionAsduQueue.queueLock);
#endif

    return enqueued;
}




static void
releaseAllQueuedASDUs(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->asduQueue.queueLock);
#endif

    self->asduQueue.firstMsgIndex = 0;
    self->asduQueue.lastMsgIndex = 0;
    self->asduQueue.entryCounter = 0;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->asduQueue.queueLock);
#endif
}

static void
Slave_resetConnectionQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->connectionAsduQueue.queueLock);
#endif

    self->connectionAsduQueue.firstMsgIndex = 0;
    self->connectionAsduQueue.lastMsgIndex = 0;
    self->connectionAsduQueue.entryCounter = 0;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->connectionAsduQueue.queueLock);
#endif
}

static void
Slave_lockQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->asduQueue.queueLock);
#endif
}

static void
Slave_unlockQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->asduQueue.queueLock);
#endif
}

static FrameBuffer*
T104Slave_getNextWaitingASDU(Slave self, uint64_t* timestamp, int* index)
{
    FrameBuffer* buffer = NULL;

    if (self->asduQueue.entryCounter != 0) {
        int currentIndex = self->asduQueue.firstMsgIndex;

        while (self->asduQueue.asdus[currentIndex].state != QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION) {

            if (self->asduQueue.asdus[currentIndex].state == QUEUE_ENTRY_STATE_NOT_USED)
                break;

            currentIndex = (currentIndex + 1) % self->asduQueue.size;

            /* break when we reached the oldest entry again */
            if (currentIndex == self->asduQueue.firstMsgIndex)
                break;
        }

        if (self->asduQueue.asdus[currentIndex].state == QUEUE_ENTRY_STATE_WAITING_FOR_TRANSMISSION) {

            self->asduQueue.asdus[currentIndex].state = QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED;
            *timestamp = self->asduQueue.asdus[currentIndex].entryTimestamp;
            *index = currentIndex;

            buffer = &(self->asduQueue.asdus[currentIndex].asdu);
        }
    }

    return buffer;
}

static void
Slave_lockConnectionQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->connectionAsduQueue.queueLock);
#endif
}

static void
Slave_unlockConnectionQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->connectionAsduQueue.queueLock);
#endif
}

static FrameBuffer*
Slave_getNextHighPrioASDU(Slave self)
{
    FrameBuffer* buffer = NULL;

    if (self->connectionAsduQueue.entryCounter != 0)  {
        int currentIndex = self->connectionAsduQueue.firstMsgIndex;

        if (self->connectionAsduQueue.entryCounter == 1) {
            self->connectionAsduQueue.entryCounter = 0;
            self->connectionAsduQueue.firstMsgIndex = -1;
            self->connectionAsduQueue.lastMsgIndex = -1;
        }
        else {
            self->connectionAsduQueue.firstMsgIndex = (self->connectionAsduQueue.firstMsgIndex + 1)
                    % (self->connectionAsduQueue.size);

            self->connectionAsduQueue.entryCounter--;
        }

        buffer = &(self->connectionAsduQueue.asdus[currentIndex]);
    }

    return buffer;
}

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
    Slave slave;
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
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore sentASDUsLock;
#endif

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


static int
receiveMessage(Socket socket, uint8_t* buffer)
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
sendIMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    //TODO lock socket

    buffer[0] = (uint8_t) 0x68;
    buffer[1] = (uint8_t) (msgSize - 2);

    buffer[2] = (uint8_t) ((self->sendCount % 128) * 2);
    buffer[3] = (uint8_t) (self->sendCount / 128);

    buffer[4] = (uint8_t) ((self->receiveCount % 128) * 2);
    buffer[5] = (uint8_t) (self->receiveCount / 128);

    if (Socket_write(self->socket, buffer, msgSize) != -1) {
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
sendASDUInternal(MasterConnection self, ASDU asdu)
{
    bool retVal;

    if (self->isActive) {

#if (CONFIG_SLAVE_USING_THREADS == 1)
        Semaphore_wait(self->sentASDUsLock);
#endif

        if (isSentBufferFull(self) == false) {

            FrameBuffer frameBuffer;

            struct sBufferFrame bufferFrame;

            Frame frame = BufferFrame_initialize(&bufferFrame, frameBuffer.msg, IEC60870_5_104_APCI_LENGTH);
            ASDU_encode(asdu, frame);

            frameBuffer.msgSize = Frame_getMsgSize(frame);

            sendASDU(self, &frameBuffer, 0, -1);

#if (CONFIG_SLAVE_USING_THREADS == 1)
            Semaphore_post(self->sentASDUsLock);
#endif

            retVal = true;
        }
        else {
#if (CONFIG_SLAVE_USING_THREADS == 1)
            Semaphore_post(self->sentASDUsLock);
#endif
            retVal = Slave_enqueueHighPrioASDU(self->slave, asdu);
        }

    }
    else
        retVal = false;

    if (retVal == false)
        DEBUG_PRINT("unable to send response (isActive=%i)\n", self->isActive);

    return retVal;
}


static void responseCOTUnknown(ASDU asdu, MasterConnection self)
{
    DEBUG_PRINT("  with unknown COT\n");
    ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
    sendASDUInternal(self, asdu);
}

static void
handleASDU(MasterConnection self, ASDU asdu)
{
    bool messageHandled = false;

    Slave slave = self->slave;

    uint8_t cot = ASDU_getCOT(asdu);

    switch (ASDU_getTypeID(asdu)) {

    case C_IC_NA_1: /* 100 - interrogation command */

        DEBUG_PRINT("Rcvd interrogation command C_IC_NA_1\n");

        if ((cot == ACTIVATION) || (cot == DEACTIVATION)) {
            if (slave->interrogationHandler != NULL) {

                InterrogationCommand irc = (InterrogationCommand) ASDU_getElement(asdu, 0);

                if (slave->interrogationHandler(slave->interrogationHandlerParameter,
                        self, asdu, InterrogationCommand_getQOI(irc)))
                    messageHandled = true;

                InterrogationCommand_destroy(irc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CI_NA_1: /* 101 - counter interrogation command */

        DEBUG_PRINT("Rcvd counter interrogation command C_CI_NA_1\n");

        if ((cot == ACTIVATION) || (cot == DEACTIVATION)) {

            if (slave->counterInterrogationHandler != NULL) {

                CounterInterrogationCommand cic = (CounterInterrogationCommand) ASDU_getElement(asdu, 0);


                if (slave->counterInterrogationHandler(slave->counterInterrogationHandlerParameter,
                        self, asdu, CounterInterrogationCommand_getQCC(cic)))
                    messageHandled = true;

                CounterInterrogationCommand_destroy(cic);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_RD_NA_1: /* 102 - read command */

        DEBUG_PRINT("Rcvd read command C_RD_NA_1\n");

        if (cot == REQUEST) {
            if (slave->readHandler != NULL) {
                ReadCommand rc = (ReadCommand) ASDU_getElement(asdu, 0);

                if (slave->readHandler(slave->readHandlerParameter,
                        self, asdu, InformationObject_getObjectAddress((InformationObject) rc)))
                    messageHandled = true;

                ReadCommand_destroy(rc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        DEBUG_PRINT("Rcvd clock sync command C_CS_NA_1\n");

        if (cot == ACTIVATION) {

            if (slave->clockSyncHandler != NULL) {

                ClockSynchronizationCommand csc = (ClockSynchronizationCommand) ASDU_getElement(asdu, 0);

                if (slave->clockSyncHandler(slave->clockSyncHandlerParameter,
                        self, asdu, ClockSynchronizationCommand_getTime(csc)))
                    messageHandled = true;

                ClockSynchronizationCommand_destroy(csc);
            }
        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_TS_NA_1: /* 104 - test command */

        DEBUG_PRINT("Rcvd test command C_TS_NA_1\n");

        if (cot != ACTIVATION)
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
        else
            ASDU_setCOT(asdu, ACTIVATION_CON);

        sendASDUInternal(self, asdu);

        messageHandled = true;

        break;

    case C_RP_NA_1: /* 105 - Reset process command */

        DEBUG_PRINT("Rcvd reset process command C_RP_NA_1\n");

        if (cot == ACTIVATION) {

            if (slave->resetProcessHandler != NULL) {
                ResetProcessCommand rpc = (ResetProcessCommand) ASDU_getElement(asdu, 0);

                if (slave->resetProcessHandler(slave->resetProcessHandlerParameter,
                        self, asdu, ResetProcessCommand_getQRP(rpc)))
                    messageHandled = true;

                ResetProcessCommand_destroy(rpc);
            }

        }
        else
            responseCOTUnknown(asdu, self);

        break;

    case C_CD_NA_1: /* 106 - Delay acquisition command */

        DEBUG_PRINT("Rcvd delay acquisition command C_CD_NA_1\n");

        if ((cot == ACTIVATION) || (cot == SPONTANEOUS)) {

            if (slave->delayAcquisitionHandler != NULL) {
                DelayAcquisitionCommand dac = (DelayAcquisitionCommand) ASDU_getElement(asdu, 0);

                if (slave->delayAcquisitionHandler(slave->delayAcquisitionHandlerParameter,
                        self, asdu, DelayAcquisitionCommand_getDelay(dac)))
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
        if (slave->asduHandler(slave->asduHandlerParameter, self, asdu))
            messageHandled = true;

    if (messageHandled == false) {
        /* send error response */
        ASDU_setCOT(asdu, UNKNOWN_TYPE_ID);
        sendASDUInternal(self, asdu);
    }
}

static void
Slave_markAsduAsConfirmed(Slave self, int index, uint64_t timestamp)
{
    if ((index < 0) || (index > self->asduQueue.size))
        return;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->asduQueue.queueLock);
#endif

    if (self->asduQueue.entryCounter > 0) {
        if (self->asduQueue.asdus[index].state == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED) {

            if (self->asduQueue.asdus[index].entryTimestamp == timestamp) {
                int currentIndex = index;

                while (self->asduQueue.asdus[currentIndex].state == QUEUE_ENTRY_STATE_SENT_BUT_NOT_CONFIRMED) {

                    DEBUG_PRINT("Remove from queue with index %i\n", currentIndex);

                    self->asduQueue.asdus[currentIndex].state = QUEUE_ENTRY_STATE_NOT_USED;
                    self->asduQueue.asdus[currentIndex].entryTimestamp = 0;

                    self->asduQueue.entryCounter--;

                    if (self->asduQueue.entryCounter == 0) {
                        self->asduQueue.firstMsgIndex = -1;
                        self->asduQueue.lastMsgIndex = -1;
                        break;
                    }

                    if (currentIndex == self->asduQueue.firstMsgIndex) {
                        self->asduQueue.firstMsgIndex = (index + 1) % self->asduQueue.size;

                        if (self->asduQueue.entryCounter == 1)
                            self->asduQueue.lastMsgIndex = self->asduQueue.firstMsgIndex;

                        break;
                    }

                    currentIndex--;

                    if (currentIndex < 0)
                        currentIndex = self->asduQueue.size - 1;

                    /* break if we reached the first deleted entry again */
                    if (currentIndex == index)
                        break;

                    DEBUG_PRINT("queue state: noASDUs: %i oldest: %i latest: %i\n", self->asduQueue.entryCounter,
                            self->asduQueue.firstMsgIndex, self->asduQueue.lastMsgIndex);

                }
            }
        }
    }

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->asduQueue.queueLock);
#endif
}

static bool
checkSequenceNumber(MasterConnection self, int seqNo)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
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
                    Slave_markAsduAsConfirmed (self->slave, self->sentASDUs[self->oldestSentASDU].queueIndex,
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


#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return seqNoIsValid;
}

static void
resetT3Timeout(MasterConnection self)
{
    self->nextT3Timeout = Hal_getTimeInMs() + (uint64_t) (self->slave->parameters.t3 * 1000);
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

            ASDU asdu = ASDU_createFromBuffer((ConnectionParameters)&(self->slave->parameters), buffer + 6, msgSize - 6);

            //TODO add support for separate handler thread
            handleASDU(self, asdu);

            ASDU_destroy(asdu);
        }
        else
            DEBUG_PRINT("Connection not activated. Skip I message");

    }

    /* Check for TESTFR_ACT message */
    else if ((buffer[2] & 0x43) == 0x43) {
        DEBUG_PRINT("Send TESTFR_CON\n");

        Socket_write(self->socket, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
    }

    /* Check for STARTDT_ACT message */
    else if ((buffer [2] & 0x07) == 0x07) {
        DEBUG_PRINT("Send STARTDT_CON\n");

        self->isActive = true;
        Slave_resetConnectionQueue(self->slave);

        Socket_write(self->socket, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
    }

    /* Check for STOPDT_ACT message */
    else if ((buffer [2] & 0x13) == 0x13) {
        DEBUG_PRINT("Send STOPDT_CON\n");

        self->isActive = false;

        Socket_write(self->socket, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE);
    }

    /* Check for TESTFR_CON message */
    else if ((buffer[2] & 0x83) == 0x83) {
        DEBUG_PRINT("Recv TESTFR_CON\n");

        self->outstandingTestFRConMessages = 0;
    }

    else if ((buffer [2] == 0x01)) /* S-message */ {
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

    Socket_write(self->socket, msg, 6);
}

static void
MasterConnection_destroy(MasterConnection self)
{
    if (self) {
        Socket_destroy(self->socket);

        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_SLAVE_USING_THREADS == 1)
        Semaphore_destroy(self->sentASDUsLock);
#endif

        GLOBAL_FREEMEM(self);
    }
}


static void
sendNextAvailableASDU(MasterConnection self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (isSentBufferFull(self))
        goto exit_function;

    Slave_lockQueue(self->slave);

    uint64_t timestamp;
    int index;

    FrameBuffer* asdu = T104Slave_getNextWaitingASDU(self->slave, &timestamp, &index);

    if (asdu != NULL)
        sendASDU(self, asdu, timestamp, index);

    Slave_unlockQueue(self->slave);

exit_function:
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif
}

static bool
sendNextHighPriorityASDU(MasterConnection self)
{
    bool retVal = false;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (isSentBufferFull(self))
        goto exit_function;

    Slave_lockConnectionQueue(self->slave);

    FrameBuffer* msg = Slave_getNextHighPrioASDU(self->slave);

    if (msg != NULL) {
        sendASDU(self, msg, 0, -1);
        retVal = true;
    }

    Slave_unlockConnectionQueue(self->slave);

exit_function:
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return retVal;
}

static void
sendWaitingASDUs(MasterConnection self)
{
    /* send all available high priority ASDUs first */
    while (Slave_isHighPrioASDUAvailable(self->slave)) {

        if (sendNextHighPriorityASDU(self) == false)
            return;

        if (self->isRunning == false)
            return;
    }

    /* send messages from low-priority queue */
    sendNextAvailableASDU(self);
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
            if (Socket_write(self->socket, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE) == -1)
                self->isRunning = false;

            self->outstandingTestFRConMessages++;
            resetT3Timeout(self);
        }
    }

    /* check timeout for others station I messages */
    if (self->unconfirmedReceivedIMessages > 0) {
        if ((currentTime - self->lastConfirmationTime) >= (uint64_t) (self->slave->parameters.t2 * 1000)) {
            self->lastConfirmationTime = currentTime;
            self->unconfirmedReceivedIMessages = 0;
            sendSMessage(self);
        }
    }

    /* check if counterpart confirmed I message */
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    if (self->oldestSentASDU != -1) {
        if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >= (uint64_t) (self->slave->parameters.t1 * 1000)) {
            timeoutsOk = false;

            printSendBuffer(self);

            DEBUG_PRINT("I message timeout for %i seqNo: %i\n", self->oldestSentASDU,
                    self->sentASDUs[self->oldestSentASDU].seqNo);
        }
    }

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return timeoutsOk;
}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection) parameter;

    uint8_t buffer[260];

    self->isRunning = true;

    resetT3Timeout(self);

    while (self->isRunning) {
        //TODO use select
        int bytesRec = receiveMessage(self->socket, buffer);

        if (bytesRec == -1) {
            DEBUG_PRINT("Error reading from socket\n");
            break;
        }

        if (bytesRec > 0) {
            DEBUG_PRINT("Connection: rcvd msg(%i bytes)\n", bytesRec);

            if (handleMessage(self, buffer, bytesRec) == false)
                self->isRunning = false;

            if (self->unconfirmedReceivedIMessages >= self->slave->parameters.w) {

                self->lastConfirmationTime = Hal_getTimeInMs();

                self->unconfirmedReceivedIMessages = 0;

                sendSMessage(self);
            }
        }
        else
            Thread_sleep(10);

        if (handleTimeouts(self) == false)
            self->isRunning = false;

        if (self->isRunning)
            if (self->isActive)
                sendWaitingASDUs(self);
    }

    DEBUG_PRINT("Connection closed\n");

    self->isRunning = false;
    self->slave->openConnections--;

    MasterConnection_destroy(self);

    return NULL;
}

static MasterConnection
MasterConnection_create(Slave slave, Socket socket)
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

        self->maxSentASDUs = slave->parameters.k;
        self->oldestSentASDU = -1;
        self->newestSentASDU = -1;
        self->sentASDUs = (SentASDUSlave*) GLOBAL_MALLOC(sizeof(SentASDUSlave) * self->maxSentASDUs);

#if (CONFIG_SLAVE_USING_THREADS == 1)
        self->sentASDUsLock = Semaphore_create(1);
#endif

        self->outstandingTestFRConMessages = 0;

        Thread newThread =
               Thread_create((ThreadExecutionFunction) connectionHandlingThread,
                       (void*) self, true);

        Thread_start(newThread);
    }

    return self;
}


bool
MasterConnection_sendASDU(MasterConnection self, ASDU asdu)
{
    bool retVal = sendASDUInternal(self, asdu);

    if (ASDU_isStackCreated(asdu) == false)
        ASDU_destroy(asdu);

    return retVal;
}

bool
MasterConnection_sendACT_CON(MasterConnection self, ASDU asdu, bool negative)
{
    ASDU_setCOT(asdu, ACTIVATION_CON);
    ASDU_setNegative(asdu, negative);

    return MasterConnection_sendASDU(self, asdu);
}

bool
MasterConnection_sendACT_TERM(MasterConnection self, ASDU asdu)
{
    ASDU_setCOT(asdu, ACTIVATION_TERMINATION);
    ASDU_setNegative(asdu, false);

    return MasterConnection_sendASDU(self, asdu);
}

static void*
serverThread (void* parameter)
{
    Slave self = (Slave) parameter;

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

            //MasterConnection connection =
            //TODO save connection reference
            MasterConnection_create(self, newSocket);

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
Slave_start(Slave self)
{
    if (self->isRunning == false) {

        self->isStarting = true;
        self->isRunning = false;
        self->stopRunning = false;

        self->listeningThread = Thread_create(serverThread, (void*) self, false);

        Thread_start(self->listeningThread);

        while (self->isStarting)
            Thread_sleep(1);
    }
}

bool
Slave_isRunning(Slave self)
{
    return self->isRunning;
}

void
Slave_stop(Slave self)
{
    if (self->isRunning) {
        self->stopRunning = true;

        while (self->isRunning)
            Thread_sleep(1);
    }

    Thread_destroy(self->listeningThread);

    self->listeningThread = NULL;
}


void
Slave_destroy(Slave self)
{
    if (self->isRunning)
        Slave_stop(self);

    releaseAllQueuedASDUs(self);

    if (self->localAddress != NULL)
        GLOBAL_FREEMEM(self->localAddress);

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_destroy(self->asduQueue.queueLock);
    Semaphore_destroy(self->connectionAsduQueue.queueLock);
#endif

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 0)
    GLOBAL_FREEMEM(self->asduQueue.asdus);
    GLOBAL_FREEMEM(self->connectionAsduQueue.asdus);
    GLOBAL_FREEMEM(self);
#endif
}
