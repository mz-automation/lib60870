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

#include "iec60870_slave.h"
#include "frame.h"
#include "t104_frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"

#include <stdio.h>

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

struct sMaster {
    InterrogationHandler interrogationHandler;
    void* interrogationHandlerParameter;

    ReadHandler readHandler;
    void* readHandlerParameter;

    ClockSynchronizationHandler clockSyncHandler;
    void* clockSyncHandlerParameter;

    ASDUHandler asduHandler;
    void* asduHandlerParameter;



    ConnectionParameters parameters;

    bool isRunning;
    bool stopRunning;

    int openConnections;
    int tcpPort;
};

static uint8_t STARTDT_CON_MSG[] = { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

#define STARTDT_CON_MSG_SIZE 6

static uint8_t STOPDT_CON_MSG[] = { 0x68, 0x04, 0x23, 0x00, 0x00, 0x00 };

#define STOPDT_CON_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

#define TESTFR_CON_MSG_SIZE 6

Master
T104Master_create(ConnectionParameters parameters)
{
    Master self = (Master) GLOBAL_MALLOC(sizeof(struct sMaster));

    if (self != NULL) {

        if (parameters != NULL)
            self->parameters = parameters;
        else
            self->parameters = &defaultConnectionParameters;

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
T104Master_getOpenConnections(Master self)
{
    return self->openConnections;
}

void
Master_setInterrogationHandler(Master self, InterrogationHandler handler, void*  parameter)
{
    self->interrogationHandler = handler;
    self->interrogationHandlerParameter = parameter;
}

void
Master_setReadHandler(Master self, ReadHandler handler, void* parameter)
{
    self->readHandler = handler;
    self->readHandlerParameter = parameter;
}

void
Master_setASDUHandler(Master self, ASDUHandler handler, void* parameter)
{
    self->asduHandler = handler;
    self->asduHandlerParameter = parameter;
}

struct sMasterConnection {
    Socket socket;
    Master master;
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
    T104Frame frame = (Frame) T104Frame_create();

    ASDU_encode(asdu, frame);

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

static void
handleASDU(MasterConnection self, ASDU asdu)
{
    bool messageHandled = false;

    Master master = self->master;

    uint8_t cot = ASDU_getCOT(asdu);

    switch (ASDU_getTypeID(asdu)) {

    case C_IC_NA_1: /* 100 - interrogation command */

        printf("Rcvd interrogation command C_IC_NA_1");

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

        printf("Rcvd counter interrogation command C_CI_NA_1");

        if ((cot == ACTIVATION) || (cot == DEACTIVATION)) {

#if 0
            if (master->counterIn != NULL) {

                CounterInterrogationCommand irc = (CounterInterrogationCommand) ASDU_getElement(asdu, 0);


                if (master->interrogationHandler(master->interrogationHandlerParameter,
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

        printf("Rcvd read command C_RD_NA_1");

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

        printf("Rcvd clock sync command C_CS_NA_1");

        if (cot == ACTIVATION) {
            ClockSynchronizationCommand csc = (ClockSynchronizationCommand) ASDU_getElement(asdu, 0);

            if (master->clockSyncHandler(master->clockSyncHandlerParameter,
                    self, asdu, ClockSynchronizationCommand_getTime(csc)))
                messageHandled = true;
        }
        else {
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
            sendASDU(self, asdu);
        }


        break;

    case C_TS_NA_1: /* 104 - test command */

        printf("Rcvd test command C_TS_NA_1");

        if (cot != ACTIVATION)
            ASDU_setCOT(asdu, UNKNOWN_CAUSE_OF_TRANSMISSION);
        else
            ASDU_setCOT(asdu, ACTIVATION_CON);

        sendASDU(self, asdu);

        messageHandled = true;

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

            ASDU asdu = ASDU_createFromBuffer(self->master->parameters, buffer + 6, msgSize - 6);

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
    else
        return false;

    return true;
}

static void*
connectionHandlingThread(void* parameter)
{
    MasterConnection self = (MasterConnection) parameter;

    uint8_t buffer[260];

    self->isRunning = true;

    while (self->isRunning) {
        int bytesRec = receiveMessage(self->socket, buffer);

        if (bytesRec == -1)
            break;

        if (bytesRec > 0) {
            printf("Connection: rcvd msg(%i bytes)\n", bytesRec);

            if (handleMessage(self, buffer, bytesRec) == false)
                self->isRunning = false;
        }
    }

    printf("Connection closed\n");

    self->isRunning = false;
    self->master->openConnections--;

    return NULL;
}

MasterConnection
MasterConnection_create(Master master, Socket socket)
{
    MasterConnection self = (MasterConnection) GLOBAL_MALLOC(sizeof(struct sMasterConnection));

    if (self != NULL) {

        self->master = master;
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

void*
serverThread (void* parameter)
{
    Master self = (Master) parameter;

    ServerSocket serverSocket = TcpServerSocket_create("127.0.0.1", self->tcpPort);

    ServerSocket_listen(serverSocket);

    self->isRunning = true;

    while (self->stopRunning == false) {
        Socket newSocket = ServerSocket_accept(serverSocket);

        if (newSocket != NULL) {

            MasterConnection connection = MasterConnection_create(self, newSocket);

        }
        else
            Thread_sleep(10);
    }

    self->isRunning = false;
    self->stopRunning = false;

    return NULL;
}

void
Master_start(Master self)
{
    if (self->isRunning == false) {
        self->stopRunning = false;

        Thread server = Thread_create(serverThread, (void*) self, true);

        Thread_start(server);
    }
}

bool
Master_isRunning(Master self)
{
    return self->isRunning;
}

void
Master_stop(Master self)
{
    self->stopRunning = true;
}

void
Master_enqueueASDU(Master self, ASDU asdu)
{

}

void
Master_destroy(Master self)
{

}
