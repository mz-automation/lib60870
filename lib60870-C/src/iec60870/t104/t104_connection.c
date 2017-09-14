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
#include "lib60870_internal.h"

struct sT104ConnectionParameters defaultConnectionParameters = {
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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

typedef struct {
    uint64_t sentTime; /* required for T1 timeout */
    int seqNo;
} SentASDU;


struct sT104Connection {
    char hostname[HOST_NAME_MAX + 1];
    int tcpPort;
    struct sT104ConnectionParameters parameters;
    int connectTimeoutInMs;
    uint8_t sMessage[6];

    SentASDU* sentASDUs; /* the k-buffer */
    int maxSentASDUs;    /* maximum number of ASDU to be sent without confirmation - parameter k */
    int oldestSentASDU;  /* index of oldest entry in k-buffer */
    int newestSentASDU;  /* index of newest entry in k-buffer */
#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore sentASDUsLock;
    Thread connectionHandlingThread;
#endif

    int receiveCount;
    int sendCount;

    bool firstIMessageReceived;

    int unconfirmedReceivedIMessages;
    uint64_t lastConfirmationTime;

    uint64_t nextT3Timeout;
    int outstandingTestFCConMessages;

    uint64_t uMessageTimeout;

    Socket socket;
    bool running;
    bool failure;
    bool close;

    ASDUReceivedHandler receivedHandler;
    void* receivedHandlerParameter;

    ConnectionHandler connectionHandler;
    void* connectionHandlerParameter;
};


static uint8_t STARTDT_ACT_MSG[] = { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };
#define STARTDT_ACT_MSG_SIZE 6

static uint8_t TESTFR_ACT_MSG[] = { 0x68, 0x04, 0x43, 0x00, 0x00, 0x00 };
#define TESTFR_ACT_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };
#define TESTFR_CON_MSG_SIZE 6

static uint8_t STOPDT_ACT_MSG[] = { 0x68, 0x04, 0x13, 0x00, 0x00, 0x00 };
#define STOPDT_ACT_MSG_SIZE 6

static uint8_t STARTDT_CON_MSG[] = { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };
#define STARTDT_CON_MSG_SIZE 6



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

static int
sendIMessage(T104Connection self, Frame frame)
{
    T104Frame_prepareToSend((T104Frame) frame, self->sendCount, self->receiveCount);

    Socket_write(self->socket, T104Frame_getBuffer(frame), T104Frame_getMsgSize(frame));

    self->sendCount = (self->sendCount + 1) % 32768;

    return self->sendCount;
}

T104Connection
T104Connection_create(const char* hostname, int tcpPort)
{
    T104Connection self = (T104Connection) GLOBAL_MALLOC(sizeof(struct sT104Connection));

    if (self != NULL) {
        strncpy(self->hostname, hostname, HOST_NAME_MAX);
        self->tcpPort = tcpPort;
        self->parameters = defaultConnectionParameters;

        self->receivedHandler = NULL;
        self->receivedHandlerParameter = NULL;

        self->connectionHandler = NULL;
        self->connectionHandlerParameter = NULL;

#if (CONFIG_MASTER_USING_THREADS == 1)
        self->sentASDUsLock = Semaphore_create(1);
        self->connectionHandlingThread = NULL;
#endif

        self->sentASDUs = NULL;

        prepareSMessage(self->sMessage);
    }

    return self;
}


static void
resetConnection(T104Connection self)
{
    self->connectTimeoutInMs = self->parameters.t0 * 1000;

    self->running = false;
    self->failure = false;
    self->close = false;

    self->receiveCount = 0;
    self->sendCount = 0;

    self->unconfirmedReceivedIMessages = 0;
    self->lastConfirmationTime = 0xffffffffffffffff;
    self->firstIMessageReceived = false;

    self->oldestSentASDU = -1;
    self->newestSentASDU = -1;

    if (self->sentASDUs == NULL) {
        self->maxSentASDUs = self->parameters.k;
        self->sentASDUs = (SentASDU*) GLOBAL_MALLOC(sizeof(SentASDU) * self->maxSentASDUs);
    }

    self->outstandingTestFCConMessages = 0;
    self->uMessageTimeout = 0;
}

static void
resetT3Timeout(T104Connection self) {
    self->nextT3Timeout = Hal_getTimeInMs() + (self->parameters.t3 * 1000);
}

static bool
checkSequenceNumber(T104Connection self, int seqNo)
{
#if (CONFIG_MASTER_USING_THREADS == 1)
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
        /* Two cases are required to reflect sequence number overflow */
        if (self->sentASDUs[self->oldestSentASDU].seqNo <= self->sentASDUs[self->newestSentASDU].seqNo) {
            if ((seqNo >= self->sentASDUs[self->oldestSentASDU].seqNo) &&
                (seqNo <= self->sentASDUs[self->newestSentASDU].seqNo))
                seqNoIsValid = true;
        }
        else {
            if ((seqNo >= self->sentASDUs[self->oldestSentASDU].seqNo) ||
                (seqNo <= self->sentASDUs[self->newestSentASDU].seqNo))
                seqNoIsValid = true;

            counterOverflowDetected = true;
        }

        int latestValidSeqNo = (self->sentASDUs[self->oldestSentASDU].seqNo - 1) % 32768;

        if (latestValidSeqNo == seqNo)
            seqNoIsValid = true;
    }

    if (seqNoIsValid) {

        if (self->oldestSentASDU != -1) {

            do {
                if (counterOverflowDetected == false) {
                    if (seqNo < self->sentASDUs [self->oldestSentASDU].seqNo)
                        break;
                }
                else {
                    if (seqNo == ((self->sentASDUs [self->oldestSentASDU].seqNo - 1) % 32768))
                        break;
                }

                self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                int checkIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

                if (self->oldestSentASDU == checkIndex) {
                    self->oldestSentASDU = -1;
                    break;
                }

                if (self->sentASDUs [self->oldestSentASDU].seqNo == seqNo)
                    break;
            } while (true);

        }
    }

#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return seqNoIsValid;
}


static bool
isSentBufferFull(T104Connection self)
{
    if (self->oldestSentASDU == -1)
        return false;

    int newIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

    if (newIndex == self->oldestSentASDU)
        return true;
    else
        return false;
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

#if (CONFIG_MASTER_USING_THREADS == 1)
    Thread_destroy(self->connectionHandlingThread);
#endif
}

void
T104Connection_destroy(T104Connection self)
{
    T104Connection_close(self);

    if (self->sentASDUs != NULL)
        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_destroy(self->sentASDUsLock);
#endif

    GLOBAL_FREEMEM(self);
}

void
T104Connection_setConnectionParameters(T104Connection self, T104ConnectionParameters parameters)
{
    self->parameters = *parameters;

    self->connectTimeoutInMs = self->parameters.t0 * 1000;
}

void
T104Connection_setConnectTimeout(T104Connection self, int millies)
{
    self->connectTimeoutInMs = millies;
}

T104ConnectionParameters
T104Connection_getConnectionParameters(T104Connection self)
{
    return &(self->parameters);
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

static bool
checkConfirmTimeout(T104Connection self, long currentTime)
{
    if ((currentTime - self->lastConfirmationTime) >= (uint32_t) (self->parameters.t2 * 1000))
        return true;
    else
        return false;
}

static bool
checkMessage(T104Connection self, uint8_t* buffer, int msgSize)
{
    if ((buffer[2] & 1) == 0) { /* I format frame */

        if (self->firstIMessageReceived == false) {
            self->firstIMessageReceived = true;
            self->lastConfirmationTime = Hal_getTimeInMs(); /* start timeout T2 */
        }

        if (msgSize < 7) {
            DEBUG_PRINT("I msg too small!\n");
            return false;
        }

        int frameSendSequenceNumber = ((buffer [3] * 0x100) + (buffer [2] & 0xfe)) / 2;
        int frameRecvSequenceNumber = ((buffer [5] * 0x100) + (buffer [4] & 0xfe)) / 2;

        DEBUG_PRINT("Received I frame: N(S) = %i N(R) = %i\n", frameSendSequenceNumber, frameRecvSequenceNumber);

        /* check the receive sequence number N(R) - connection will be closed on an unexpected value */
        if (frameSendSequenceNumber != self->receiveCount) {
            DEBUG_PRINT("Sequence error: Close connection!");
            return false;
        }

        if (checkSequenceNumber(self, frameRecvSequenceNumber) == false)
            return false;

        self->receiveCount = (self->receiveCount + 1) % 32768;
        self->unconfirmedReceivedIMessages++;

        ASDU asdu = ASDU_createFromBuffer((ConnectionParameters)&(self->parameters), buffer + 6, msgSize - 6);

        if (asdu != NULL) {
            if (self->receivedHandler != NULL)
                self->receivedHandler(self->receivedHandlerParameter, asdu);

            ASDU_destroy(asdu);
        }
        else
            return false;

    }
    else if ((buffer[2] & 0x03) == 0x03) { /* U format frame */

        DEBUG_PRINT("Received U frame\n");

        self->uMessageTimeout = 0;

        if (buffer[2] == 0x43) { /* Check for TESTFR_ACT message */
            DEBUG_PRINT("Send TESTFR_CON\n");
            Socket_write(self->socket, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
        }
        else if (buffer[2] == 0x83) { /* TESTFR_CON */
            DEBUG_PRINT("Rcvd TESTFR_CON\n");
            self->outstandingTestFCConMessages = 0;
        }
        else if (buffer[2] == 0x07) { /* STARTDT_ACT */
            DEBUG_PRINT("Send STARTDT_CON\n");
            Socket_write(self->socket, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
        }
        else if (buffer[2] == 0x0b) { /* STARTDT_CON */
            DEBUG_PRINT("Received STARTDT_CON\n");

            if (self->connectionHandler != NULL)
                self->connectionHandler(self->connectionHandlerParameter, self, IEC60870_CONNECTION_STARTDT_CON_RECEIVED);
        }
        else if (buffer[2] == 0x23) { /* STOPDT_CON */
            DEBUG_PRINT("Received STOPDT_CON\n");

            if (self->connectionHandler != NULL)
                self->connectionHandler(self->connectionHandlerParameter, self, IEC60870_CONNECTION_STOPDT_CON_RECEIVED);
        }

    }
    else if (buffer [2] == 0x01) { /* S-message */
        int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

        DEBUG_PRINT("Rcvd S(%i) (own sendcounter = %i)\n", seqNo, self->sendCount);

        if (checkSequenceNumber(self, seqNo) == false)
            return false;
    }


    resetT3Timeout(self);

    return true;
}

static bool
handleTimeouts(T104Connection self)
{
    bool retVal = true;

    uint64_t currentTime = Hal_getTimeInMs();

    if (currentTime > self->nextT3Timeout) {

        if (self->outstandingTestFCConMessages > 2) {
            DEBUG_PRINT("Timeout for TESTFR_CON message\n");

            /* close connection */
            retVal = false;
            goto exit_function;
        }
        else {
            DEBUG_PRINT("U message T3 timeout\n");

            Socket_write(self->socket, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE);
            self->uMessageTimeout = currentTime + (self->parameters.t1 * 1000);
            self->outstandingTestFCConMessages++;

            resetT3Timeout(self);
        }
    }

    if (self->unconfirmedReceivedIMessages > 0) {

        if (checkConfirmTimeout(self, currentTime)) {

            self->lastConfirmationTime = currentTime;
            self->unconfirmedReceivedIMessages = 0;

            sendSMessage(self); /* send confirmation message */
        }
    }

    if (self->uMessageTimeout != 0) {
        if (currentTime > self->uMessageTimeout) {
            DEBUG_PRINT("U message T1 timeout\n");
            retVal = false;
            goto exit_function;
        }
    }

    /* check if counterpart confirmed I messages */
#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif
    if (self->oldestSentASDU != -1) {
        if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >= (uint64_t) (self->parameters.t1 * 1000)) {
            DEBUG_PRINT("I message timeout\n");
            retVal = false;
        }
    }
#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif


exit_function:

    return retVal;
}

static void*
handleConnection(void* parameter)
{
    T104Connection self = (T104Connection) parameter;

    resetConnection(self);

    self->socket = TcpSocket_create();

    Socket_setConnectTimeout(self->socket, self->connectTimeoutInMs);

    if (Socket_connect(self->socket, self->hostname, self->tcpPort)) {

        self->running = true;

        /* Call connection handler */
        if (self->connectionHandler != NULL)
            self->connectionHandler(self->connectionHandlerParameter, self, IEC60870_CONNECTION_OPENED);

        HandleSet handleSet = Handleset_new();

        bool loopRunning = true;

        while (loopRunning) {

            uint8_t buffer[300];

            Handleset_reset(handleSet);
            Handleset_addSocket(handleSet, self->socket);

            if (Handleset_waitReady(handleSet, 100)) {
                int bytesRec = receiveMessage(self->socket, buffer);

                if (bytesRec == -1) {
                    loopRunning = false;
                    self->failure = true;
                }

                if (bytesRec > 0) {
                    //TODO call raw message handler if available

                    if (checkMessage(self, buffer, bytesRec) == false) {
                        /* close connection on error */
                        loopRunning= false;
                        self->failure = true;
                    }
                }

                if (self->unconfirmedReceivedIMessages >= self->parameters.w) {
                    self->lastConfirmationTime = Hal_getTimeInMs();
                    self->unconfirmedReceivedIMessages = 0;
                    sendSMessage(self);
                }
            }

            if (handleTimeouts(self) == false)
                loopRunning = false;

            if (self->close)
                loopRunning = false;
        }

        Handleset_destroy(handleSet);

        /* Call connection handler */
        if (self->connectionHandler != NULL)
            self->connectionHandler(self->connectionHandlerParameter, self, IEC60870_CONNECTION_CLOSED);
    }
    else {
        self->failure = true;
    }

    Socket_destroy(self->socket);

    DEBUG_PRINT("EXIT CONNECTION HANDLING THREAD\n");

    self->running = false;

    return NULL;
}

void
T104Connection_connectAsync(T104Connection self)
{
    resetConnection(self);

    resetT3Timeout(self);

    self->connectionHandlingThread = Thread_create(handleConnection, (void*) self, false);

    if (self->connectionHandlingThread)
        Thread_start(self->connectionHandlingThread);
}

bool
T104Connection_connect(T104Connection self)
{
    T104Connection_connectAsync(self);

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

void
T104Connection_setConnectionHandler(T104Connection self, ConnectionHandler handler, void* parameter)
{
    self->connectionHandler = handler;
    self->connectionHandlerParameter = parameter;
}

static void
encodeIdentificationField(T104Connection self, Frame frame, TypeID typeId,
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
encodeIOA(T104Connection self, Frame frame, int ioa)
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
T104Connection_sendStopDT(T104Connection self)
{
    Socket_write(self->socket, STOPDT_ACT_MSG, STOPDT_ACT_MSG_SIZE);
}

static void
sendIMessageAndUpdateSentASDUs(T104Connection self, Frame frame)
{
#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    int currentIndex = 0;

    if (self->oldestSentASDU == -1) {
        self->oldestSentASDU = 0;
        self->newestSentASDU = 0;

    } else {
        currentIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;
    }

    self->sentASDUs [currentIndex].seqNo = sendIMessage (self, frame);
    self->sentASDUs [currentIndex].sentTime = Hal_getTimeInMs();

    self->newestSentASDU = currentIndex;

#if (CONFIG_MASTER_USING_THREADS == 1)
    Semaphore_post(self->sentASDUsLock);
#endif
}

static bool
sendASDUInternal(T104Connection self, Frame frame)
{
    bool retVal = false;

    if (self->running) {
        if (isSentBufferFull(self) == false) {
            sendIMessageAndUpdateSentASDUs(self, frame);
            retVal = true;
        }
    }

    T104Frame_destroy(frame);

    return retVal;
}

bool
T104Connection_sendInterrogationCommand(T104Connection self, CauseOfTransmission cot, int ca, QualifierOfInterrogation qoi)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_IC_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode QOI (7.2.6.22) */
    T104Frame_setNextByte(frame, qoi); /* 20 = station interrogation */

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendCounterInterrogationCommand(T104Connection self, CauseOfTransmission cot, int ca, uint8_t qcc)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_CI_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode QCC */
    T104Frame_setNextByte(frame, qcc);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendReadCommand(T104Connection self, int ca, int ioa)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_RD_NA_1, 1, REQUEST, ca);

    encodeIOA(self, frame, ioa);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendClockSyncCommand(T104Connection self, int ca, CP56Time2a time)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_CS_NA_1, 1, ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_appendBytes(frame, CP56Time2a_getEncodedValue(time), 7);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendTestCommand(T104Connection self, int ca)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_TS_NA_1, 1, ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_setNextByte(frame, 0xcc);
    T104Frame_setNextByte(frame, 0x55);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendControlCommand(T104Connection self, TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField (self, frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

    InformationObject_encode(sc, frame, (ConnectionParameters) &(self->parameters), false);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_sendASDU(T104Connection self, ASDU asdu)
{
    Frame frame = (Frame) T104Frame_create();

    ASDU_encode(asdu, frame);

    return sendASDUInternal(self, frame);
}

bool
T104Connection_isTransmitBufferFull(T104Connection self)
{
    return isSentBufferFull(self);
}

