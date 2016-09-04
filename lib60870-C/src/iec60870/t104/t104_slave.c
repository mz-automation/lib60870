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

#include "iec60870_slave.h"
#include "frame.h"
#include "t104_frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"

#include "lib60870_config.h"

#include "apl_types_internal.h"

#define T104_DEFAULT_PORT 2404

//TODO refactor: move to separate file/class
static struct sT104ConnectionParameters defaultConnectionParameters = {
        .k = 12,
        .w = 8,
        .t0 = 10,
        .t1 = 15,
        .t2 = 10,
        .t3 = 20,
        .sizeOfTypeId = 1,
        .sizeOfVSQ = 1,
        .sizeOfCOT = 2,
        .originatorAddress = 0,
        .sizeOfCA = 2,
        .sizeOfIOA = 3
};


struct sMessageQueue {
    int size;
    int entryCounter;
    int lastMsgIndex;
    int firstMsgIndex;

#if (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 1)
    ASDU asdus[CONFIG_SLAVE_MESSAGE_QUEUE_SIZE];
#else
    ASDU* asdus;
#endif

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore queueLock;
#endif
};

struct sSlave {
    InterrogationHandler interrogationHandler;
    void* interrogationHandlerParameter;

    ReadHandler readHandler;
    void* readHandlerParameter;

    ClockSynchronizationHandler clockSyncHandler;
    void* clockSyncHandlerParameter;

    ASDUHandler asduHandler;
    void* asduHandlerParameter;

    struct sMessageQueue messageQueue;

    ConnectionParameters parameters;

    bool isRunning;
    bool stopRunning;

    int openConnections;
    int tcpPort;
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

    self->messageQueue.asdus = GLOBAL_CALLOC(maxQueueSize, sizeof(ASDU));
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
            self->parameters = parameters;
        else
            self->parameters = (ConnectionParameters) &defaultConnectionParameters;

        self->asduHandler = NULL;
        self->interrogationHandler = NULL;
        self->readHandler = NULL;
        self->clockSyncHandler = NULL;

        self->isRunning = false;
        self->stopRunning = false;

        self->tcpPort = T104_DEFAULT_PORT;
        self->openConnections = 0;
    }

    return self;
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
    return self->parameters;
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
        printf("add entry (nextIndex:%i)\n", nextIndex);
        self->messageQueue.asdus[nextIndex] = asdu;
        self->messageQueue.lastMsgIndex = nextIndex;
        self->messageQueue.entryCounter++;
    }
    else
    {
        printf("add entry (nextIndex:%i) -> remove oldest\n", nextIndex);

        /* remove oldest entry */
        ASDU_destroy(self->messageQueue.asdus[nextIndex]);
        self->messageQueue.asdus[nextIndex] = asdu;
        self->messageQueue.lastMsgIndex = nextIndex;

        int firstIndex = nextIndex + 1;

        if (firstIndex == self->messageQueue.size)
            firstIndex = 0;

        self->messageQueue.firstMsgIndex = firstIndex;
    }

    printf("ASDUs in FIFO: %i (first: %i, last: %i)\n", self->messageQueue.entryCounter,
            self->messageQueue.firstMsgIndex, self->messageQueue.lastMsgIndex);

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->messageQueue.queueLock);
#endif

    //TODO trigger active connection to send message
}


ASDU
Slave_dequeueASDU(Slave self)
{
    ASDU asdu = NULL;

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_wait(self->messageQueue.queueLock);
#endif

    if (self->messageQueue.entryCounter != 0) {
        int firstMsgIndex = self->messageQueue.firstMsgIndex;

        printf("remove entry (%i)\n", firstMsgIndex);

        asdu = self->messageQueue.asdus[firstMsgIndex];

        firstMsgIndex++;

        if (firstMsgIndex == self->messageQueue.size)
            firstMsgIndex = 0;

        self->messageQueue.firstMsgIndex = firstMsgIndex;
        self->messageQueue.entryCounter--;

        if (self->messageQueue.entryCounter == 0)
            self->messageQueue.lastMsgIndex = firstMsgIndex;

        printf("-->ASDUs in FIFO: %i (first: %i, last: %i)\n", self->messageQueue.entryCounter,
                self->messageQueue.firstMsgIndex, self->messageQueue.lastMsgIndex);

    }

#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_post(self->messageQueue.queueLock);
#endif

    return asdu;
}

struct sMasterConnection {
    Socket socket;
    Slave slave;
    bool isActive;
    bool isRunning;

    int sendCount;
    int receiveCount;

    int unconfirmedMessages;
    uint64_t lastConfirmationTime;
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
    self->unconfirmedMessages++;

    if (self->unconfirmedMessages == 1)
        self->lastConfirmationTime = Hal_getTimeInMs();

}

static void
sendIMessage(MasterConnection self, T104Frame frame)
{
    T104Frame_prepareToSend(frame, self->sendCount, self->receiveCount);
    Socket_write(self->socket, T104Frame_getBuffer(frame), T104Frame_getMsgSize(frame));
    self->sendCount++;
}

static void
sendASDU(MasterConnection self, ASDU asdu)
{
    Frame frame = (Frame) T104Frame_create();

    ASDU_encode(asdu, frame);

    sendIMessage(self, (T104Frame) frame);

    T104Frame_destroy((T104Frame) frame);
}

static void
handleASDU(MasterConnection self, ASDU asdu)
{
    bool messageHandled = false;

    Slave master = self->slave;

    uint8_t cot = ASDU_getCOT(asdu);

    switch (ASDU_getTypeID(asdu)) {

    case C_IC_NA_1: /* 100 - interrogation command */

        printf("Rcvd interrogation command C_IC_NA_1\n");

        if ((cot == ACTIVATION) || (cot == DEACTIVATION)) {
            if (master->interrogationHandler != NULL) {

                InterrogationCommand irc = (InterrogationCommand) ASDU_getElement(asdu, 0);

                if (master->interrogationHandler(master->interrogationHandlerParameter,
                        self, asdu, InterrogationCommand_getQOI(irc)))
                    messageHandled = true;
            }
        }
        else {
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
            sendASDU(self, asdu);
        }


        break;

    case C_CI_NA_1: /* 101 - counter interrogation command */

        printf("Rcvd counter interrogation command C_CI_NA_1\n");

        if ((cot == ACTIVATION) || (cot == DEACTIVATION)) {

#if 0
            if (slave->counterIn != NULL) {

                CounterInterrogationCommand irc = (CounterInterrogationCommand) ASDU_getElement(asdu, 0);


                if (slave->interrogationHandler(slave->interrogationHandlerParameter,
                        self, asdu, InterrogationCommand_getQOI(irc)))
                    messageHandled = true;
            }
#endif
        }
        else {
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
            sendASDU(self, asdu);
        }


        break;

    case C_RD_NA_1: /* 102 - read command */

        printf("Rcvd read command C_RD_NA_1\n");

        if (cot == REQUEST) {
            if (master->readHandler != NULL) {
                ReadCommand rc = (ReadCommand) ASDU_getElement(asdu, 0);

                if (master->readHandler(master->readHandlerParameter,
                        self, asdu, InformationObject_getObjectAddress((InformationObject) rc)))
                    messageHandled = true;
            }
        }
        else {
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
            sendASDU(self, asdu);
        }


        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        printf("Rcvd clock sync command C_CS_NA_1\n");

        if (cot == ACTIVATION) {

            if (master->clockSyncHandler != NULL) {

                ClockSynchronizationCommand csc = (ClockSynchronizationCommand) ASDU_getElement(asdu, 0);

                if (master->clockSyncHandler(master->clockSyncHandlerParameter,
                        self, asdu, ClockSynchronizationCommand_getTime(csc)))
                    messageHandled = true;
            }
        }
        else {
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
            sendASDU(self, asdu);
        }


        break;

    case C_TS_NA_1: /* 104 - test command */

        printf("Rcvd test command C_TS_NA_1\n");

        if (cot != ACTIVATION)
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
        else
            ASDU_setCOT(asdu, ACTIVATION_CON);

        sendASDU(self, asdu);

        messageHandled = true;

        break;

    default: /* no special handler available -> use default handler */
        break;
    }

    if ((messageHandled == false) && (master->asduHandler != NULL))
        if (master->asduHandler(master->asduHandlerParameter, self, asdu))
            messageHandled = true;

    if (messageHandled == false) {
        /* send error response */
        ASDU_setCOT(asdu, UNKNOWN_TYPE_ID);
        sendASDU(self, asdu);
    }
}

static bool
handleMessage(MasterConnection self, uint8_t* buffer, int msgSize)
{
    if ((buffer[2] & 1) == 0) {

        printf("Received I frame\n");

        if (msgSize < 7) {
            printf("I msg too small!\n");
            return false;
        }

        increaseReceivedMessageCounters(self);

        if (self->isActive) {

            ASDU asdu = ASDU_createFromBuffer(self->slave->parameters, buffer + 6, msgSize - 6);

            handleASDU(self, asdu);
            //ASDU_destroy(asdu);
        }
    }

    /* Check for TESTFR_ACT message */
    else if ((buffer[2] & 0x43) == 0x43) {
        printf("Send TESTFR_CON\n");

        Socket_write(self->socket, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
    }

    /* Check for STARTDT_ACT message */
    else if ((buffer [2] & 0x07) == 0x07) {
        printf("Send STARTDT_CON\n");

        self->isActive = true;

        Socket_write(self->socket, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
    }

    /* Check for STOPDT_ACT message */
    else if ((buffer [2] & 0x13) == 0x13) {
        printf("Send STOPDT_CON\n");

        self->isActive = false;

        Socket_write(self->socket, STOPDT_CON_MSG, STOPDT_CON_MSG_SIZE);
    }
    else if ((buffer [2] == 0x01)) /* S-message */ {
        int messageCount = (buffer[4] + buffer[5] * 0x100) / 2;

        printf("Rcvd S(%i) (own sendcounter = %i)\n", messageCount, self->sendCount);
    }

    else {
        printf("unknown message - IGNORE\n");
        return true;
    }

    return true;
}

static void
checkServerQueue(MasterConnection self)
{
    ASDU asdu = Slave_dequeueASDU(self->slave);

    if (asdu != NULL) {
        sendASDU(self, asdu);

        ASDU_destroy(asdu);
    }
}

#if 0
        private void sendSMessage() {
            byte[] msg = new byte[6];

            msg [0] = 0x68;
            msg [1] = 0x04;
            msg [2] = 0x01;
            msg [3] = 0;
            msg [4] = (byte) ((receiveCount % 128) * 2);
            msg [5] = (byte) (receiveCount / 128);

            socket.Send (msg);
        }
#endif

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
    T104ConnectionParameters parameters = self->slave->parameters;

    if ((currentTime - self->lastConfirmationTime) >= (parameters->t2 * 1000))
        return true;
    else
        return false;
}

static void
sendSMessageIfRequired(MasterConnection self)
{
    if (self->unconfirmedMessages > 0) {
        uint64_t currentTime = Hal_getTimeInMs();

        T104ConnectionParameters parameters = self->slave->parameters;

        if ((self->unconfirmedMessages > parameters->w) || checkConfirmTimeout(self, currentTime)) {

            printf("Send S message\n");

            self->lastConfirmationTime = currentTime;

            self->unconfirmedMessages = 0;

            sendSMessage(self);
        }
    }
}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection) parameter;

    uint8_t buffer[260];

    self->isRunning = true;

    while (self->isRunning) {
        int bytesRec = receiveMessage(self->socket, buffer);

        if (bytesRec == -1) {
            printf("Error reading from socket\n");
            break;
        }

        if (bytesRec > 0) {
            printf("Connection: rcvd msg(%i bytes)\n", bytesRec);

            if (handleMessage(self, buffer, bytesRec) == false)
                self->isRunning = false;
        }

        if (self->isActive)
            checkServerQueue(self);

        sendSMessageIfRequired(self);
    }

    printf("Connection closed\n");

    self->isRunning = false;
    self->slave->openConnections--;

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

        self->unconfirmedMessages = 0;
        self->lastConfirmationTime = UINT64_MAX;

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
    Frame frame = (Frame) T104Frame_create();
    ASDU_encode(asdu, frame);

    if (ASDU_isStackCreated(asdu) == false)
        ASDU_destroy(asdu);

    sendIMessage(self, (T104Frame) frame);

    Frame_destroy(frame);
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

    ServerSocket serverSocket = TcpServerSocket_create("127.0.0.1", self->tcpPort);

    ServerSocket_listen(serverSocket);

    self->isRunning = true;

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

    self->isRunning = false;
    self->stopRunning = false;

    return NULL;
}

void
Slave_start(Slave self)
{
    if (self->isRunning == false) {
        self->stopRunning = false;

        Thread server = Thread_create(serverThread, (void*) self, true);

        Thread_start(server);
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
    self->stopRunning = true;
}


void
Slave_destroy(Slave self)
{
#if (CONFIG_SLAVE_USING_THREADS == 1)
    Semaphore_destroy(self->messageQueue.queueLock);
#endif

#if  (CONFIG_SLAVE_WITH_STATIC_MESSAGE_QUEUE == 0)
    GLOBAL_FREEMEM(self->messageQueue.frames);
    GLOBAL_FREEMEM(self);
#endif
}
