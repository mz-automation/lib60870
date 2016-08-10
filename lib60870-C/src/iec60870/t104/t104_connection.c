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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iec60870_common.h"
#include "t104_frame.h"
#include "hal_thread.h"
#include "hal_socket.h"
#include "hal_time.h"
#include "lib_memory.h"

#include "t104_connection.h"
#include "apl_types_internal.h"
#include "information_objects_internal.h"

struct sT104ConnectionParameters defaultConnectionParameters = {
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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

struct sT104Connection {
    char hostname[HOST_NAME_MAX + 1];
    int tcpPort;
    struct sT104ConnectionParameters parameters;
    uint8_t sMessage[6];

    int receiveCount;
    int sendCount;

    int unconfirmedMessages;
    uint64_t lastConfirmationTime;

    Socket socket;
    bool running;
    bool failure;
    bool close;

    ASDUReceivedHandler receivedHandler;
    void* receivedHandlerParameter;
};

static uint8_t STARTDT_ACT_MSG[] = { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };
#define STARTDT_ACT_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };
#define TESTFR_CON_MSG_SIZE 6

static void
prepareSMessage(uint8_t* msg)
{
    msg[0] = 0x68;
    msg[1] = 0x04;
    msg[2] = 0x01;
    msg[3] = 0x00;
}

static void
sendSMessage(T104Connection self)
{
    uint8_t* msg = self->sMessage;

    msg [4] = (uint8_t) ((self->receiveCount % 128) * 2);
    msg [5] = (uint8_t) (self->receiveCount / 128);

    Socket_write(self->socket, msg, 6);
}

static void
sendIMessage(T104Connection self, T104Frame frame)
{
    T104Frame_prepareToSend(frame, self->sendCount, self->receiveCount);

    Socket_write(self->socket, T104Frame_getBuffer(frame), T104Frame_getMsgSize(frame));

    self->sendCount++;
}

T104Connection
T104Connection_create(const char* hostname, int tcpPort)
{
    T104Connection self = (T104Connection) GLOBAL_MALLOC(sizeof(struct sT104Connection));

    if (self != NULL) {
        strncpy(self->hostname, hostname, HOST_NAME_MAX);
        self->tcpPort = tcpPort;
        self->parameters = defaultConnectionParameters;
        self->running = false;
        self->failure = false;

        self->receiveCount = 0;
        self->sendCount = 0;
        self->unconfirmedMessages = 0;
        self->lastConfirmationTime = 0xffffffffffffffff;

        self->receivedHandler = NULL;
        self->receivedHandlerParameter = NULL;

        prepareSMessage(self->sMessage);
    }

    return self;
}

static void
T104Connection_close(T104Connection self)
{
    if (self->running) {

        self->close = true;

        /* wait unit connection thread terminates */
        while (self->running)
            Thread_sleep(1);
    }
}

void
T104Connection_destroy(T104Connection self)
{
    T104Connection_close(self);

    GLOBAL_FREEMEM(self);
}

void
T104Connection_setConnectionParameters(T104Connection self, T104ConnectionParameters parameters)
{
    self->parameters = *parameters;
}

void
T104Connection_setConnectTimeout(T104Connection self, int millies)
{
    //TODO implement me
}

T104ConnectionParameters
T104Connection_getConnectionParameters(T104Connection self)
{
    return &(self->parameters);
}

static int
receiveMessage(Socket socket, uint8_t* buffer)
{

    if (Socket_read(socket, buffer, 1) != 1)
        return 0;

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

static bool
checkConfirmTimeout(T104Connection self, long currentTime)
{
    if ((currentTime - self->lastConfirmationTime) >= (uint) (self->parameters.t2 * 1000))
        return true;
    else
        return false;
}

static bool
checkMessage(T104Connection self, uint8_t* buffer, int msgSize)
{
    if ((buffer[2] & 1) == 0) {

        printf("Received I frame\n");

        if (msgSize < 7) {
            printf("I msg too small!\n");
            return false;
        }

        self->receiveCount++;
        self->unconfirmedMessages++;

        uint64_t currentTime = Hal_getTimeInMs();

        if ((self->unconfirmedMessages > self->parameters.w) || checkConfirmTimeout(self, currentTime)) {

            self->lastConfirmationTime = currentTime;

            self->unconfirmedMessages = 0;
            sendSMessage(self);
        }

        //TODO check T104 specific header

        ASDU asdu = ASDU_createFromBuffer((ConnectionParameters)&(self->parameters), buffer + 6, msgSize - 6);

        if (asdu != NULL) {

            if (self->receivedHandler != NULL)
                self->receivedHandler(self->receivedHandlerParameter, asdu);

        }
        else
            return false;

    }
    else if (buffer[2] == 0x43) { /* Check for TESTFR_ACT message */

        printf("Send TESTFR_CON\n");

        Socket_write(self->socket, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
    }

    return true;
}

static void*
handleConnection(void* parameter)
{
    T104Connection self = (T104Connection) parameter;

    self->close = false;

    Socket socket = TcpSocket_create();

    //TODO set connect timeout

    if (Socket_connect(socket, self->hostname, self->tcpPort)) {

        self->running = true;
        self->socket = socket;

        uint8_t buffer[300];

        while (self->running) {

            Thread_sleep(1); //TODO use select!

            int bytesRec = receiveMessage(socket, buffer);

            if (bytesRec == -1) {
                self->running = false;
                self->failure = true;
            }

            if (bytesRec > 0) {
                //TODO call raw message handler if available

                if (checkMessage(self, buffer, bytesRec) == false) {
                    /* close connection on error */
                    self->running = false;
                    self->failure = true;
                }
            }

            if (self->close)
                self->running = false;
        }

        Socket_destroy(socket);
    }
    else {
        self->failure = true;
        self->running = false;
    }

    return NULL;
}

void
T104Connection_connect(T104Connection self)
{
    Thread workerThread = Thread_create(handleConnection, self, true);

    if (workerThread)
        Thread_start(workerThread);
}

bool
T104Connection_connectBlocking(T104Connection self)
{
    Thread workerThread = Thread_create(handleConnection, self, true);

    if (workerThread)
        Thread_start(workerThread);

    while ((self->running == false) && (self->failure == false))
        Thread_sleep(1);

    return self->running;
}

void
T104Connection_setASDUReceivedHandler(T104Connection self, ASDUReceivedHandler handler, void* parameter)
{
    self->receivedHandler = handler;
    self->receivedHandlerParameter = parameter;
}

static void
encodeIdentificationField(T104Connection self, T104Frame frame, TypeID typeId,
        int vsq, CauseOfTransmission cot, int ca)
{
    T104Frame_setNextByte(frame, typeId);
    T104Frame_setNextByte(frame, (uint8_t) vsq);

    /* encode COT */
    T104Frame_setNextByte(frame, (uint8_t) cot);
    if (self->parameters.sizeOfCOT == 2)
        T104Frame_setNextByte(frame, (uint8_t) self->parameters.originatorAddress);

    /* encode CA */
    T104Frame_setNextByte(frame, (uint8_t)(ca & 0xff));
    if (self->parameters.sizeOfCA == 2)
        T104Frame_setNextByte(frame, (uint8_t) ((ca & 0xff00) >> 8));
}

static void
encodeIOA(T104Connection self, T104Frame frame, int ioa)
{
    T104Frame_setNextByte(frame, (uint8_t) (ioa & 0xff));

    if (self->parameters.sizeOfIOA > 1)
        T104Frame_setNextByte(frame, (uint8_t) ((ioa / 0x100) & 0xff));

    if (self->parameters.sizeOfIOA > 1)
        T104Frame_setNextByte(frame, (uint8_t) ((ioa / 0x10000) & 0xff));
}

void
T104Connection_sendStartDT(T104Connection self)
{
    Socket_write(self->socket, STARTDT_ACT_MSG, STARTDT_ACT_MSG_SIZE);
}

void
T104Connection_sendInterrogationCommand(T104Connection self, CauseOfTransmission cot, int ca, uint8_t qoi)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField(self, frame, C_IC_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode COI (7.2.6.21) */
    T104Frame_setNextByte(frame, qoi); /* 20 = station interrogation */

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

void
T104Connection_sendCounterInterrogationCommand(T104Connection self, CauseOfTransmission cot, int ca, uint8_t qcc)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField(self, frame, C_CI_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode QCC */
    T104Frame_setNextByte(frame, qcc);

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

void
T104Connection_sendReadCommend(T104Connection self, int ca, int ioa)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField(self, frame, C_RD_NA_1, 1, REQUEST, ca);

    encodeIOA(self, frame, 0);

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

void
T104Connection_sendClockSyncCommand(T104Connection self, int ca, CP56Time2a time)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField(self, frame, C_CS_NA_1, 1, ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_appendBytes(frame, CP56Time2a_getEncodedValue(time), 7);

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

void
T104Connection_sendTestCommand(T104Connection self, int ca)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField(self, frame, C_TS_NA_1, 1, ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_setNextByte(frame, 0xcc);
    T104Frame_setNextByte(frame, 0x55);

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}

void
T104Connection_sendControlCommand(T104Connection self, TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc)
{
    T104Frame frame = T104Frame_create();

    encodeIdentificationField (self, frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

    InformationObject_encode(sc, (Frame) frame, (ConnectionParameters) &(self->parameters));

    sendIMessage(self, frame);

    T104Frame_destroy(frame);
}
