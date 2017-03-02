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

struct sMessageQueue {
    int size;
    int entryCounter;
    int lastMsgIndex;
    int firstMsgIndex;

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    FrameBuffer asdus[CONFIG_SLAVE_MESSAGE_QUEUE_SIZE];
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

    struct sMessageQueue messageQueue;

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


static void
initializeMessageQueue(Slave self, int maxQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    maxQueueSize = CONFIG_SLAVE_MESSAGE_QUEUE_SIZE;
#else
    if (maxQueueSize < 1)
        maxQueueSize = CONFIG_SLAVE_MESSAGE_QUEUE_SIZE;

    self->messageQueue.asdus = GLOBAL_CALLOC(maxQueueSize, sizeof(FrameBuffer));
#endif

    self->messageQueue.entryCounter = 0;
    self->messageQueue.firstMsgIndex = 0;
    self->messageQueue.lastMsgIndex = 0;
    self->messageQueue.size = maxQueueSize;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    self->messageQueue.queueLock = Semaphore_create(1);
#endif
}

Slave
T104Slave_create(ConnectionParameters parameters, int maxQueueSize)
{
#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    Slave self = &(singleStaticSlaveInstance);
#else
    Slave self = (Slave) GLOBAL_MALLOC(sizeof(struct sSlave));
#endif

    if (self != NULL) {

        initializeMessageQueue(self, maxQueueSize);

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
    Semaphore_wait(self->messageQueue.queueLock);
#endif

    int nextIndex;
    bool removeEntry = false;

    if (self->messageQueue.entryCounter == 0)
        nextIndex = self->messageQueue.lastMsgIndex;
    else
        nextIndex = self->messageQueue.lastMsgIndex + 1;

    if (nextIndex == self->messageQueue.size)
        nextIndex = 0;

    if (self->messageQueue.entryCounter == self->messageQueue.size)
        removeEntry = true;

    if (removeEntry == false) {
        DEBUG_PRINT("add entry (nextIndex:%i)\n", nextIndex);
        self->messageQueue.lastMsgIndex = nextIndex;
        self->messageQueue.entryCounter++;
    }
    else
    {
        DEBUG_PRINT("add entry (nextIndex:%i) -> remove oldest\n", nextIndex);

        /* remove oldest entry */
        self->messageQueue.lastMsgIndex = nextIndex;

        int firstIndex = nextIndex + 1;

        if (firstIndex == self->messageQueue.size)
            firstIndex = 0;

        self->messageQueue.firstMsgIndex = firstIndex;
    }

    struct sBufferFrame bufferFrame;

    Frame frame = BufferFrame_initialize(&bufferFrame, self->messageQueue.asdus[nextIndex].msg,
                                            IEC60870_5_104_APCI_LENGTH);

    ASDU_encode(asdu, frame);

    self->messageQueue.asdus[nextIndex].msgSize = Frame_getMsgSize(frame);

    ASDU_destroy(asdu);

    DEBUG_PRINT("ASDUs in FIFO: %i (first: %i, last: %i)\n", self->messageQueue.entryCounter,
            self->messageQueue.firstMsgIndex, self->messageQueue.lastMsgIndex);

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->messageQueue.queueLock);
#endif

    //TODO trigger active connection to send message
}

static void
releaseAllQueuedASDUs(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->messageQueue.queueLock);
#endif

    self->messageQueue.firstMsgIndex = 0;
    self->messageQueue.lastMsgIndex = 0;
    self->messageQueue.entryCounter = 0;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->messageQueue.queueLock);
#endif
}

static void
Slave_lockQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->messageQueue.queueLock);
#endif
}

static void
Slave_unlockQueue(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->messageQueue.queueLock);
#endif
}

static uint8_t*
Slave_dequeueASDU(Slave self, int* msgSize)
{
    uint8_t* buffer = NULL;

    if (self->messageQueue.entryCounter != 0) {
        int firstMsgIndex = self->messageQueue.firstMsgIndex;

        DEBUG_PRINT("remove entry (%i)\n", firstMsgIndex);

        buffer = self->messageQueue.asdus[firstMsgIndex].msg;
        *msgSize = self->messageQueue.asdus[firstMsgIndex].msgSize;

        firstMsgIndex++;

        if (firstMsgIndex == self->messageQueue.size)
            firstMsgIndex = 0;

        self->messageQueue.firstMsgIndex = firstMsgIndex;
        self->messageQueue.entryCounter--;

        if (self->messageQueue.entryCounter == 0)
            self->messageQueue.lastMsgIndex = firstMsgIndex;

        DEBUG_PRINT("-->ASDUs in FIFO: %i (first: %i, last: %i)\n", self->messageQueue.entryCounter,
                self->messageQueue.firstMsgIndex, self->messageQueue.lastMsgIndex);

    }

    return buffer;
}

typedef struct {
    // required to identify message in server (low-priority) queue
    uint64_t entryTime;
    int queueIndex; /* -1 if ASDU is not from low-priority queue */

    // required for T1 timeout
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

    bool firstIMessageReceived;
};



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

static void
increaseReceivedMessageCounters(MasterConnection self)
{
    self->receiveCount++;
    self->unconfirmedReceivedIMessages++;

    if (self->unconfirmedReceivedIMessages == 1)
        self->lastConfirmationTime = Hal_getTimeInMs();

}

static void
sendIMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    //TODO lock socket

    buffer[0] = (uint8_t) 0x68;
    buffer[1] = (uint8_t) (msgSize - 2);

    buffer[2] = (uint8_t) ((self->sendCount % 128) * 2);
    buffer[3] = (uint8_t) (self->sendCount / 128);

    buffer[4] = (uint8_t) ((self->receiveCount % 128) * 2);
    buffer[5] = (uint8_t) (self->receiveCount / 128);

    Socket_write(self->socket, buffer, msgSize);

    self->sendCount = (self->sendCount + 1) % 32768;

    self->unconfirmedReceivedIMessages = 0;
}

static bool
isSentBufferFull(MasterConnection self)
{
    //TODO lock k-buffer
    if (self->oldestSentASDU == -1)
        return false;

    int newIndex = (self->newestSentASDU + 1) % (self->maxSentASDUs);

    if (newIndex == self->oldestSentASDU)
        return true;
    else
        return false;
}

static void
sendASDU(MasterConnection self, uint8_t* buffer, int msgSize)
{
    sendIMessage(self, buffer, msgSize);
}

static void
sendASDUInternal(MasterConnection self, ASDU asdu)
{
    struct sBufferFrame bufferFrame;

    uint8_t buffer[IEC60870_5_104_MAX_ASDU_LENGTH + IEC60870_5_104_APCI_LENGTH];

    Frame frame = BufferFrame_initialize(&bufferFrame, buffer,
                                            IEC60870_5_104_APCI_LENGTH);

    ASDU_encode(asdu, frame);

    sendASDU(self, buffer, Frame_getMsgSize(frame));
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

static bool
handleMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    if ((buffer[2] & 1) == 0) {

        DEBUG_PRINT("Received I frame\n");

        if (msgSize < 7) {
            DEBUG_PRINT("I msg too small!\n");
            return false;
        }

        increaseReceivedMessageCounters(self);

        if (self->isActive) {

            ASDU asdu = ASDU_createFromBuffer((ConnectionParameters)&(self->slave->parameters), buffer + 6, msgSize - 6);

            handleASDU(self, asdu);

            ASDU_destroy(asdu);
        }
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

        Socket_write(self->socket, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
    }

    /* Check for STOPDT_ACT message */
    else if ((buffer [2] & 0x13) == 0x13) {
        DEBUG_PRINT("Send STOPDT_CON\n");

        self->isActive = false;

        Socket_write(self->socket, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE);
    }
    else if ((buffer [2] == 0x01)) /* S-message */ {
        int messageCount = (buffer[4] + buffer[5] * 0x100) / 2;

        DEBUG_PRINT("Rcvd S(%i) (own sendcounter = %i)\n", messageCount, self->sendCount);
    }

    else {
        DEBUG_PRINT("unknown message - IGNORE\n");
        return true;
    }

    return true;
}

static void
checkServerQueue(MasterConnection self)
{
    Slave_lockQueue(self->slave);

    int msgSize;

    uint8_t* buffer = Slave_dequeueASDU(self->slave, &msgSize);

    if (buffer != NULL)
        sendASDU(self, buffer, msgSize);

    Slave_unlockQueue(self->slave);
}

static void sendSMessage(MasterConnection self)
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


static bool
checkConfirmTimeout(MasterConnection self, uint64_t currentTime)
{
	T104ConnectionParameters parameters = (T104ConnectionParameters) &(self->slave->parameters);

    if ((currentTime - self->lastConfirmationTime) >= ((uint64_t) parameters->t2 * 1000))
        return true;
    else
        return false;
}

static void
sendSMessageIfRequired(MasterConnection self)
{
    if (self->unconfirmedReceivedIMessages > 0) {
        uint64_t currentTime = Hal_getTimeInMs();

		T104ConnectionParameters parameters = (T104ConnectionParameters) &(self->slave->parameters);

        if ((self->unconfirmedReceivedIMessages > parameters->w) || checkConfirmTimeout(self, currentTime)) {

            DEBUG_PRINT("Send S message\n");

            self->lastConfirmationTime = currentTime;

            self->unconfirmedReceivedIMessages = 0;

            sendSMessage(self);
        }
    }
}

static void
MasterConnection_destroy(MasterConnection self)
{
    if (self) {
        Socket_destroy(self->socket);

        GLOBAL_FREEMEM(self->sentASDUs);

        GLOBAL_FREEMEM(self);
    }
}

static void
resetT3Timeout(MasterConnection self)
{
    self->nextT3Timeout = Hal_getTimeInMs() + (uint64_t) (self->slave->parameters.t3 * 1000);
}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection) parameter;

    uint8_t buffer[260];

    self->isRunning = true;

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
        }
        else
            Thread_sleep(10);

        if (self->isActive)
            checkServerQueue(self);

        sendSMessageIfRequired(self);
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

        self->outstandingTestFRConMessages = 0;

        Thread newThread =
               Thread_create((ThreadExecutionFunction) connectionHandlingThread,
                       (void*) self, true);

        Thread_start(newThread);
    }

    return self;
}


void
MasterConnection_sendASDU(MasterConnection self, ASDU asdu)
{
    sendASDUInternal(self, asdu);

    if (ASDU_isStackCreated(asdu) == false)
        ASDU_destroy(asdu);
}

void
MasterConnection_sendACT_CON(MasterConnection self, ASDU asdu, bool negative)
{
    ASDU_setCOT(asdu, ACTIVATION_CON);
    ASDU_setNegative(asdu, negative);

    MasterConnection_sendASDU(self, asdu);
}

void
MasterConnection_sendACT_TERM(MasterConnection self, ASDU asdu)
{
    ASDU_setCOT(asdu, ACTIVATION_TERMINATION);
    ASDU_setNegative(asdu, false);

    MasterConnection_sendASDU(self, asdu);
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
    Semaphore_destroy(self->messageQueue.queueLock);
#endif

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 0)
    GLOBAL_FREEMEM(self->messageQueue.asdus);
    GLOBAL_FREEMEM(self);
#endif
}
