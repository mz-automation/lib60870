/*
 *  Copyright 2016-2018 MZ Automation GmbH
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

#include "cs104_connection.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cs104_frame.h"
#include "hal_thread.h"
#include "hal_socket.h"
#include "hal_time.h"
#include "lib_memory.h"

#include "apl_types_internal.h"
#include "information_objects_internal.h"
#include "lib60870_internal.h"
#include "cs101_asdu_internal.h"

struct sCS104_APCIParameters defaultAPCIParameters = {
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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

typedef struct {
    uint64_t sentTime; /* required for T1 timeout */
    int seqNo;
} SentASDU;


struct sCS104_Connection {
    char hostname[HOST_NAME_MAX + 1];
    int tcpPort;

    struct sCS104_APCIParameters parameters;
    struct sCS101_AppLayerParameters alParameters;

    uint8_t recvBuffer[260];
    int recvBufPos;

    int connectTimeoutInMs;
    uint8_t sMessage[6];

    SentASDU* sentASDUs; /* the k-buffer */
    int maxSentASDUs;    /* maximum number of ASDU to be sent without confirmation - parameter k */
    int oldestSentASDU;  /* index of oldest entry in k-buffer */
    int newestSentASDU;  /* index of newest entry in k-buffer */

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore sentASDUsLock;
#endif

#if (CONFIG_USE_THREADS == 1)
    Thread connectionHandlingThread;
#endif

    int receiveCount;
    int sendCount;

    int unconfirmedReceivedIMessages;

    /* timeout T2 handling */
    bool timeoutT2Trigger;
    uint64_t lastConfirmationTime;

    uint64_t nextT3Timeout;
    int outstandingTestFCConMessages;

    uint64_t uMessageTimeout;

    Socket socket;
    bool running;
    bool failure;
    bool close;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    TLSConfiguration tlsConfig;
    TLSSocket tlsSocket;
#endif

    CS101_ASDUReceivedHandler receivedHandler;
    void* receivedHandlerParameter;

    CS104_ConnectionHandler connectionHandler;
    void* connectionHandlerParameter;

    IEC60870_RawMessageHandler rawMessageHandler;
    void* rawMessageHandlerParameter;
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

static int
writeToSocket(CS104_Connection self, uint8_t* buf, int size)
{
    if (self->rawMessageHandler)
        self->rawMessageHandler(self->rawMessageHandlerParameter, buf, size, true);

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket)
        return TLSSocket_write(self->tlsSocket, buf, size);
    else
        return Socket_write(self->socket, buf, size);
#else
    return Socket_write(self->socket, buf, size);
#endif
}

static void
prepareSMessage(uint8_t* msg)
{
    msg[0] = 0x68;
    msg[1] = 0x04;
    msg[2] = 0x01;
    msg[3] = 0x00;
}

static void
sendSMessage(CS104_Connection self)
{
    uint8_t* msg = self->sMessage;

    msg [4] = (uint8_t) ((self->receiveCount % 128) * 2);
    msg [5] = (uint8_t) (self->receiveCount / 128);

    writeToSocket(self, msg, 6);
}

static int
sendIMessage(CS104_Connection self, Frame frame)
{
    T104Frame_prepareToSend((T104Frame) frame, self->sendCount, self->receiveCount);

    writeToSocket(self, T104Frame_getBuffer(frame), T104Frame_getMsgSize(frame));

    self->sendCount = (self->sendCount + 1) % 32768;

    self->unconfirmedReceivedIMessages = false;
    self->timeoutT2Trigger = false;

    return self->sendCount;
}

static CS104_Connection
createConnection(const char* hostname, int tcpPort)
{
    CS104_Connection self = (CS104_Connection) GLOBAL_MALLOC(sizeof(struct sCS104_Connection));

    if (self != NULL) {
        strncpy(self->hostname, hostname, HOST_NAME_MAX);
        self->tcpPort = tcpPort;
        self->parameters = defaultAPCIParameters;
        self->alParameters = defaultAppLayerParameters;

        self->receivedHandler = NULL;
        self->receivedHandlerParameter = NULL;

        self->connectionHandler = NULL;
        self->connectionHandlerParameter = NULL;

        self->rawMessageHandler = NULL;
        self->rawMessageHandlerParameter = NULL;

#if (CONFIG_USE_SEMAPHORES == 1)
        self->sentASDUsLock = Semaphore_create(1);
#endif

#if (CONFIG_USE_THREADS == 1)
        self->connectionHandlingThread = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        self->tlsConfig = NULL;
        self->tlsSocket = NULL;
#endif

        self->sentASDUs = NULL;

        prepareSMessage(self->sMessage);
    }

    return self;
}

CS104_Connection
CS104_Connection_create(const char* hostname, int tcpPort)
{
    if (tcpPort == -1)
        tcpPort = IEC_60870_5_104_DEFAULT_PORT;

    return createConnection(hostname, tcpPort);
}

#if (CONFIG_CS104_SUPPORT_TLS == 1)
CS104_Connection
CS104_Connection_createSecure(const char* hostname, int tcpPort, TLSConfiguration tlsConfig)
{
    if (tcpPort == -1)
        tcpPort = IEC_60870_5_104_DEFAULT_TLS_PORT;

    CS104_Connection self = createConnection(hostname, tcpPort);

    if (self != NULL) {
        self->tlsConfig = tlsConfig;
        TLSConfiguration_setClientMode(tlsConfig);
    }

    return self;
}
#endif /* (CONFIG_CS104_SUPPORT_TLS == 1) */

static void
resetT3Timeout(CS104_Connection self) {
    self->nextT3Timeout = Hal_getTimeInMs() + (self->parameters.t3 * 1000);
}


static void
resetConnection(CS104_Connection self)
{
    self->connectTimeoutInMs = self->parameters.t0 * 1000;
    self->recvBufPos = 0;

    self->running = false;
    self->failure = false;
    self->close = false;

    self->receiveCount = 0;
    self->sendCount = 0;

    self->unconfirmedReceivedIMessages = 0;
    self->lastConfirmationTime = 0xffffffffffffffff;
    self->timeoutT2Trigger = false;

    self->oldestSentASDU = -1;
    self->newestSentASDU = -1;

    if (self->sentASDUs == NULL) {
        self->maxSentASDUs = self->parameters.k;
        self->sentASDUs = (SentASDU*) GLOBAL_MALLOC(sizeof(SentASDU) * self->maxSentASDUs);
    }

    self->outstandingTestFCConMessages = 0;
    self->uMessageTimeout = 0;

    resetT3Timeout(self);
}


static bool
checkSequenceNumber(CS104_Connection self, int seqNo)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif

    /* check if received sequence number is valid */

    bool seqNoIsValid = false;
    bool counterOverflowDetected = false;
    int oldestValidSeqNo = -1;

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

        /* check if confirmed message was already removed from list */
        if (self->sentASDUs[self->oldestSentASDU].seqNo == 0)
            oldestValidSeqNo = 32767;
        else
            oldestValidSeqNo = (self->sentASDUs[self->oldestSentASDU].seqNo - 1) % 32768;

        if (oldestValidSeqNo == seqNo)
            seqNoIsValid = true;
    }

    if (seqNoIsValid) {

        if (self->oldestSentASDU != -1) {

            do {
                if (counterOverflowDetected == false) {
                    if (seqNo < self->sentASDUs [self->oldestSentASDU].seqNo)
                        break;
                }

                if (seqNo == oldestValidSeqNo)
                    break;



                if (self->sentASDUs [self->oldestSentASDU].seqNo == seqNo) {
                    /* we arrived at the seq# that has been confirmed */

                    if (self->oldestSentASDU == self->newestSentASDU)
                        self->oldestSentASDU = -1;
                    else
                        self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                    break;
                }

                self->oldestSentASDU = (self->oldestSentASDU + 1) % self->maxSentASDUs;

                int checkIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

                if (self->oldestSentASDU == checkIndex) {
                    self->oldestSentASDU = -1;
                    break;
                }

            } while (true);

        }
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif

    return seqNoIsValid;
}


static bool
isSentBufferFull(CS104_Connection self)
{
    if (self->oldestSentASDU == -1)
        return false;

    int newIndex = (self->newestSentASDU + 1) % self->maxSentASDUs;

    if (newIndex == self->oldestSentASDU)
        return true;
    else
        return false;
}

void
CS104_Connection_close(CS104_Connection self)
{
    self->close = true;
#if (CONFIG_USE_THREADS == 1)
    if (self->connectionHandlingThread)
    {
        Thread_destroy(self->connectionHandlingThread);
        self->connectionHandlingThread = NULL;
    }
#endif
}

void
CS104_Connection_destroy(CS104_Connection self)
{
    CS104_Connection_close(self);

    if (self->sentASDUs != NULL)
        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_destroy(self->sentASDUsLock);
#endif

    GLOBAL_FREEMEM(self);
}

void
CS104_Connection_setAPCIParameters(CS104_Connection self, CS104_APCIParameters parameters)
{
    self->parameters = *parameters;

    self->connectTimeoutInMs = self->parameters.t0 * 1000;
}

void
CS104_Connection_setAppLayerParameters(CS104_Connection self, CS101_AppLayerParameters parameters)
{
    self->alParameters = *parameters;
}

CS101_AppLayerParameters
CS104_Connection_getAppLayerParameters(CS104_Connection self)
{
    return &(self->alParameters);
}

void
CS104_Connection_setConnectTimeout(CS104_Connection self, int millies)
{
    self->connectTimeoutInMs = millies;
}

CS104_APCIParameters
CS104_Connection_getAPCIParameters(CS104_Connection self)
{
    return &(self->parameters);
}

/**
 * \return number of bytes read, or -1 in case of an error
 */
static int
readFromSocket(CS104_Connection self, uint8_t* buffer, int size)
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
receiveMessage(CS104_Connection self)
{
    uint8_t* buffer = self->recvBuffer;
    int bufPos = self->recvBufPos;

    /* read start byte */
    if (bufPos == 0) {
        int readFirst = readFromSocket(self, buffer, 1);

        if (readFirst < 1)
            return readFirst;

        if (buffer[0] != 0x68)
            return -1; /* message error */

        bufPos++;
    }

    /* read length byte */
    if (bufPos == 1)  {

        int readCnt = readFromSocket(self, buffer + 1, 1);

        if (readCnt < 0) {
            self->recvBufPos = 0;
            return -1;
        }
        else if (readCnt == 0) {
            self->recvBufPos = 1;
            return 0;
        }

        bufPos++;
    }

    /* read remaining frame */
    if (bufPos > 1) {
        int length = buffer[1];

        int remainingLength = length - bufPos + 2;

        int readCnt = readFromSocket(self, buffer + bufPos, remainingLength);

        if (readCnt == remainingLength) {
            self->recvBufPos = 0;
            return length + 2;
        }
        else if (readCnt == -1) {
            self->recvBufPos = 0;
            return -1;
        }
        else {
            self->recvBufPos = bufPos + readCnt;
            return 0;
        }
    }

    self->recvBufPos = bufPos;
    return 0;
}

static bool
checkConfirmTimeout(CS104_Connection self, uint64_t currentTime)
{
    if (currentTime > self->lastConfirmationTime) {
        if ((currentTime - self->lastConfirmationTime) >= (uint32_t) (self->parameters.t2 * 1000)) {
            return true;
        }
    }

    return false;
}

static bool
checkMessage(CS104_Connection self, uint8_t* buffer, int msgSize)
{
    if ((buffer[2] & 1) == 0) { /* I format frame */

        if (self->timeoutT2Trigger == false) {
            self->timeoutT2Trigger = true;
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

        CS101_ASDU asdu = CS101_ASDU_createFromBuffer((CS101_AppLayerParameters)&(self->alParameters), buffer + 6, msgSize - 6);

        if (asdu != NULL) {
            if (self->receivedHandler != NULL)
                self->receivedHandler(self->receivedHandlerParameter, -1, asdu);

            CS101_ASDU_destroy(asdu);
        }
        else
            return false;

    }
    else if ((buffer[2] & 0x03) == 0x03) { /* U format frame */

        DEBUG_PRINT("Received U frame\n");

        self->uMessageTimeout = 0;

        if (buffer[2] == 0x43) { /* Check for TESTFR_ACT message */
            DEBUG_PRINT("Send TESTFR_CON\n");
            writeToSocket(self, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
        }
        else if (buffer[2] == 0x83) { /* TESTFR_CON */
            DEBUG_PRINT("Rcvd TESTFR_CON\n");
            self->outstandingTestFCConMessages = 0;
        }
        else if (buffer[2] == 0x07) { /* STARTDT_ACT */
            DEBUG_PRINT("Send STARTDT_CON\n");
            writeToSocket(self, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);
        }
        else if (buffer[2] == 0x0b) { /* STARTDT_CON */
            DEBUG_PRINT("Received STARTDT_CON\n");

            if (self->connectionHandler != NULL)
                self->connectionHandler(self->connectionHandlerParameter, self, CS104_CONNECTION_STARTDT_CON_RECEIVED);
        }
        else if (buffer[2] == 0x23) { /* STOPDT_CON */
            DEBUG_PRINT("Received STOPDT_CON\n");

            if (self->connectionHandler != NULL)
                self->connectionHandler(self->connectionHandlerParameter, self, CS104_CONNECTION_STOPDT_CON_RECEIVED);
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
handleTimeouts(CS104_Connection self)
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

            writeToSocket(self, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE);
            self->uMessageTimeout = currentTime + (self->parameters.t1 * 1000);
            self->outstandingTestFCConMessages++;

            resetT3Timeout(self);
        }
    }

    if (self->unconfirmedReceivedIMessages > 0) {

        if (checkConfirmTimeout(self, currentTime)) {

            self->lastConfirmationTime = currentTime;
            self->unconfirmedReceivedIMessages = 0;
            self->timeoutT2Trigger = false;

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
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->sentASDUsLock);
#endif
    if (self->oldestSentASDU != -1) {
        if (currentTime > self->sentASDUs[self->oldestSentASDU].sentTime) {
            if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >= (uint64_t) (self->parameters.t1 * 1000)) {
                DEBUG_PRINT("I message timeout\n");
                retVal = false;
            }
        }
    }
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif


exit_function:

    return retVal;
}

#if (CONFIG_USE_THREADS == 1)
static void*
handleConnection(void* parameter)
{
    CS104_Connection self = (CS104_Connection) parameter;

    resetConnection(self);

    self->socket = TcpSocket_create();

    Socket_setConnectTimeout(self->socket, self->connectTimeoutInMs);

    if (Socket_connect(self->socket, self->hostname, self->tcpPort)) {

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        if (self->tlsConfig != NULL) {
            self->tlsSocket = TLSSocket_create(self->socket, self->tlsConfig, false);

            if (self->tlsSocket)
                self->running = true;
            else
                self->failure = true;
        }
        else
            self->running = true;
#else
        self->running = true;
#endif

        if (self->running) {

            /* Call connection handler */
            if (self->connectionHandler != NULL)
                self->connectionHandler(self->connectionHandlerParameter, self, CS104_CONNECTION_OPENED);

            HandleSet handleSet = Handleset_new();

            bool loopRunning = true;

            while (loopRunning) {

                Handleset_reset(handleSet);
                Handleset_addSocket(handleSet, self->socket);

                if (Handleset_waitReady(handleSet, 100)) {
                    int bytesRec = receiveMessage(self);

                    if (bytesRec == -1) {
                        loopRunning = false;
                        self->failure = true;
                    }

                    if (bytesRec > 0) {

                        if (self->rawMessageHandler)
                            self->rawMessageHandler(self->rawMessageHandlerParameter, self->recvBuffer, bytesRec, false);

                        if (checkMessage(self, self->recvBuffer, bytesRec) == false) {
                            /* close connection on error */
                            loopRunning = false;
                            self->failure = true;
                        }
                    }

                    if (self->unconfirmedReceivedIMessages >= self->parameters.w) {
                        self->lastConfirmationTime = Hal_getTimeInMs();
                        self->unconfirmedReceivedIMessages = 0;
                        self->timeoutT2Trigger = false;
                        sendSMessage(self);
                    }
                }

                if (handleTimeouts(self) == false)
                    loopRunning = false;

                if (self->close)
                    loopRunning = false;
            }

            Handleset_destroy(handleSet);

        }
    }
    else {
        self->failure = true;
    }

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket)
        TLSSocket_close(self->tlsSocket);
#endif

    Socket_destroy(self->socket);

    self->running = false;

    /* Call connection handler */
    if (self->connectionHandler != NULL)
        self->connectionHandler(self->connectionHandlerParameter, self, CS104_CONNECTION_CLOSED);

    DEBUG_PRINT("EXIT CONNECTION HANDLING THREAD\n");

    return NULL;
}
#endif /* (CONFIG_USE_THREADS == 1) */

void
CS104_Connection_connectAsync(CS104_Connection self)
{
    self->running = false;
    self->failure = false;
    self->close = false;

#if (CONFIG_USE_THREADS == 1)
    self->connectionHandlingThread = Thread_create(handleConnection, (void*) self, false);

    if (self->connectionHandlingThread)
        Thread_start(self->connectionHandlingThread);
#endif
}

bool
CS104_Connection_connect(CS104_Connection self)
{
    self->running = false;
    self->failure = false;
    self->close = false;

    CS104_Connection_connectAsync(self);

    while ((self->running == false) && (self->failure == false))
        Thread_sleep(1);

    return self->running;
}

void
CS104_Connection_setASDUReceivedHandler(CS104_Connection self, CS101_ASDUReceivedHandler handler, void* parameter)
{
    self->receivedHandler = handler;
    self->receivedHandlerParameter = parameter;
}

void
CS104_Connection_setConnectionHandler(CS104_Connection self, CS104_ConnectionHandler handler, void* parameter)
{
    self->connectionHandler = handler;
    self->connectionHandlerParameter = parameter;
}

void
CS104_Connection_setRawMessageHandler(CS104_Connection self, IEC60870_RawMessageHandler handler, void* parameter)
{
    self->rawMessageHandler = handler;
    self->rawMessageHandlerParameter = parameter;
}

static void
encodeIdentificationField(CS104_Connection self, Frame frame, TypeID typeId,
        int vsq, CS101_CauseOfTransmission cot, int ca)
{
    T104Frame_setNextByte(frame, typeId);
    T104Frame_setNextByte(frame, (uint8_t) vsq);

    /* encode COT */
    T104Frame_setNextByte(frame, (uint8_t) cot);
    if (self->alParameters.sizeOfCOT == 2)
        T104Frame_setNextByte(frame, (uint8_t) self->alParameters.originatorAddress);

    /* encode CA */
    T104Frame_setNextByte(frame, (uint8_t)(ca & 0xff));
    if (self->alParameters.sizeOfCA == 2)
        T104Frame_setNextByte(frame, (uint8_t) ((ca & 0xff00) >> 8));
}

static void
encodeIOA(CS104_Connection self, Frame frame, int ioa)
{
    T104Frame_setNextByte(frame, (uint8_t) (ioa & 0xff));

    if (self->alParameters.sizeOfIOA > 1)
        T104Frame_setNextByte(frame, (uint8_t) ((ioa / 0x100) & 0xff));

    if (self->alParameters.sizeOfIOA > 2)
        T104Frame_setNextByte(frame, (uint8_t) ((ioa / 0x10000) & 0xff));
}

void
CS104_Connection_sendStartDT(CS104_Connection self)
{
    writeToSocket(self, STARTDT_ACT_MSG, STARTDT_ACT_MSG_SIZE);
}

void
CS104_Connection_sendStopDT(CS104_Connection self)
{
    writeToSocket(self, STOPDT_ACT_MSG, STOPDT_ACT_MSG_SIZE);
}

static void
sendIMessageAndUpdateSentASDUs(CS104_Connection self, Frame frame)
{
#if (CONFIG_USE_SEMAPHORES == 1)
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

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->sentASDUsLock);
#endif
}

static bool
sendASDUInternal(CS104_Connection self, Frame frame)
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
CS104_Connection_sendInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, QualifierOfInterrogation qoi)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_IC_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode QOI (7.2.6.22) */
    T104Frame_setNextByte(frame, qoi); /* 20 = station interrogation */

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendCounterInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, uint8_t qcc)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_CI_NA_1, 1, cot, ca);

    encodeIOA(self, frame, 0);

    /* encode QCC */
    T104Frame_setNextByte(frame, qcc);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendReadCommand(CS104_Connection self, int ca, int ioa)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_RD_NA_1, 1, CS101_COT_REQUEST, ca);

    encodeIOA(self, frame, ioa);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendClockSyncCommand(CS104_Connection self, int ca, CP56Time2a newTime)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_CS_NA_1, 1, CS101_COT_ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_appendBytes(frame, CP56Time2a_getEncodedValue(newTime), 7);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendTestCommand(CS104_Connection self, int ca)
{
    Frame frame = (Frame) T104Frame_create();

    encodeIdentificationField(self, frame, C_TS_NA_1, 1, CS101_COT_ACTIVATION, ca);

    encodeIOA(self, frame, 0);

    T104Frame_setNextByte(frame, 0xcc);
    T104Frame_setNextByte(frame, 0x55);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendProcessCommand(CS104_Connection self, TypeID typeId, CS101_CauseOfTransmission cot, int ca, InformationObject sc)
{
    Frame frame = (Frame) T104Frame_create();

    if (typeId == 0)
        typeId = InformationObject_getType(sc);

    encodeIdentificationField (self, frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

    InformationObject_encode(sc, frame, (CS101_AppLayerParameters) &(self->alParameters), false);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendProcessCommandEx(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, InformationObject sc)
{
    Frame frame = (Frame) T104Frame_create();

    TypeID typeId = InformationObject_getType(sc);

    encodeIdentificationField (self, frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

    InformationObject_encode(sc, frame, (CS101_AppLayerParameters) &(self->alParameters), false);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_sendASDU(CS104_Connection self, CS101_ASDU asdu)
{
    Frame frame = (Frame) T104Frame_create();

    CS101_ASDU_encode(asdu, frame);

    return sendASDUInternal(self, frame);
}

bool
CS104_Connection_isTransmitBufferFull(CS104_Connection self)
{
    return isSentBufferFull(self);
}

