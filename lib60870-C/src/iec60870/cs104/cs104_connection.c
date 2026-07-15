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

#include "cs104_connection.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cs104_frame.h"
#include "hal_socket.h"
#include "hal_thread.h"
#include "hal_time.h"
#include "lib_memory.h"
#include "linked_list.h"
#include "tls_socket.h"

#include "apl_types_internal.h"
#include "cs101_asdu_internal.h"
#include "information_objects_internal.h"
#include "lib60870_internal.h"

#ifdef SEC_AUTH_60870_5_7
#include "sec_endpoint_int.h"
#endif

struct sCS104_APCIParameters defaultAPCIParameters = {
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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

typedef enum
{
    STATE_IDLE = 0,
    STATE_INACTIVE = 1,
    STATE_ACTIVE = 2,
    STATE_WAITING_FOR_STARTDT_CON = 3,
    STATE_WAITING_FOR_STOPDT_CON = 4
} CS104_ConState;

typedef struct
{
    uint64_t sentTime; /* required for T1 timeout */
    int seqNo;
} SentASDU;

struct sCS104_Connection
{
    char hostname[HOST_NAME_MAX + 1];
    int tcpPort;

    char* localIpAddress;
    int localTcpPort;

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
    int outstandingTestFRConMessages;

    uint64_t uMessageTimeout;

    Socket socket;
    bool running;
    bool failure;
    bool close;

    CS104_ConState conState;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore conStateLock;
#endif

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

    struct sIPeerConnection peerConnection;

    LinkedList plugins; /* list of CS101_MasterPlugin references */

#ifdef SEC_AUTH_60870_5_7
    SecureEndpoint secureEndpoint;
#endif

    bool isThreadlessMode; /* true when started via CS104_Connection_startThreadless */
};

/* Forward prototypes for internal static functions referenced by threadless API before their definitions */
static int
receiveMessage(CS104_Connection self);

static void
confirmOutstandingMessages(CS104_Connection self);

static bool
checkMessage(CS104_Connection self, uint8_t* buffer, int msgSize);

static bool
handleTimeouts(CS104_Connection self);

static bool
isClose(CS104_Connection self);

static uint8_t STARTDT_ACT_MSG[] = {0x68, 0x04, 0x07, 0x00, 0x00, 0x00};
#define STARTDT_ACT_MSG_SIZE 6

static uint8_t TESTFR_ACT_MSG[] = {0x68, 0x04, 0x43, 0x00, 0x00, 0x00};
#define TESTFR_ACT_MSG_SIZE 6

static uint8_t TESTFR_CON_MSG[] = {0x68, 0x04, 0x83, 0x00, 0x00, 0x00};
#define TESTFR_CON_MSG_SIZE 6

static uint8_t STOPDT_ACT_MSG[] = {0x68, 0x04, 0x13, 0x00, 0x00, 0x00};
#define STOPDT_ACT_MSG_SIZE 6

static uint8_t STARTDT_CON_MSG[] = {0x68, 0x04, 0x0b, 0x00, 0x00, 0x00};
#define STARTDT_CON_MSG_SIZE 6

static int
writeToSocket(CS104_Connection self, uint8_t* buf, int size)
{
    if (self->rawMessageHandler)
        self->rawMessageHandler(self->rawMessageHandlerParameter, buf, size, true);

    if (self->socket == NULL || self->conState == STATE_IDLE)
        return 0;

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

    msg[4] = (uint8_t)((self->receiveCount % 128) * 2);
    msg[5] = (uint8_t)(self->receiveCount / 128);

    writeToSocket(self, msg, 6);
}

static int
sendIMessage(CS104_Connection self, Frame frame)
{
    T104Frame_prepareToSend((T104Frame)frame, self->sendCount, self->receiveCount);

    writeToSocket(self, T104Frame_getBuffer(frame), T104Frame_getMsgSize(frame));

    self->sendCount = (self->sendCount + 1) % 32768;

    self->unconfirmedReceivedIMessages = false;
    self->timeoutT2Trigger = false;

    int sendCount = self->sendCount;

    return sendCount;
}

static bool
_IPeerConnection_isReady(IPeerConnection self)
{
    CS104_Connection con = (CS104_Connection)self->object;

    if (con->conState != STATE_ACTIVE)
        return false;

    // TODO: Implement this function
    //  check if send buffer is full
    return true;
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

static void
sendIMessageAndUpdateSentASDUs(CS104_Connection self, Frame frame)
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

    self->sentASDUs[currentIndex].seqNo = sendIMessage(self, frame);
    self->sentASDUs[currentIndex].sentTime = Hal_getTimeInMs();

    self->newestSentASDU = currentIndex;
}

static bool
isRunning(CS104_Connection self)
{
    bool isRunning;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    isRunning = self->running;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    return isRunning;
}

bool
CS104_Connection_isConnected(CS104_Connection self)
{
    return isRunning(self);
}

static bool
sendASDUInternal(CS104_Connection self, Frame frame)
{
    bool retVal = false;

    if (isRunning(self))
    {
#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->conStateLock);
#endif

        if (isSentBufferFull(self) == false)
        {
            sendIMessageAndUpdateSentASDUs(self, frame);
            retVal = true;
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->conStateLock);
#endif
    }

    Frame_destroy(frame);

    return retVal;
}

static bool
sendASDUInternalNoLock(CS104_Connection self, Frame frame)
{
    bool retVal = false;

    if (self->running)
    {
        if (isSentBufferFull(self) == false)
        {
            sendIMessageAndUpdateSentASDUs(self, frame);
            retVal = true;
        }
    }

    Frame_destroy(frame);

    return retVal;
}

static bool
_IPeerConnection_sendASDU(IPeerConnection self, CS101_ASDU asdu)
{
    CS104_Connection con = (CS104_Connection)self->object;

    struct sT104Frame _frame;

    Frame frame = (Frame)T104Frame_createEx(&_frame);

    CS101_ASDU_encode(asdu, frame);

    bool result = sendASDUInternalNoLock(con, frame);

    return result;
}

static void
_IPeerConnection_close(IPeerConnection self)
{
    CS104_Connection con = (CS104_Connection)self->object;

    // CS104_Connection_close(con); // -> can this cause a deadlock?
    //  instead use
    con->close = true;
}

static CS101_AppLayerParameters
_IPeerConnection_getApplicationLayerParameters(IPeerConnection self)
{
    CS104_Connection con = (CS104_Connection)self->object;

    return &(con->alParameters);
}

static int
_IPeerConnection_getPeerAddress(IPeerConnection self, char* addrBuf, int addrBufSize)
{
    CS104_Connection con = (CS104_Connection)self->object;

    if (con->socket == NULL)
    {
        return 0;
    }

    char buf[54];

    char* addrStr = Socket_getPeerAddressStatic(con->socket, buf);

    if (addrStr == NULL)
        return 0;

    int len = (int)strnlen(buf, sizeof(buf));

    if (len < addrBufSize)
    {
        memcpy(addrBuf, buf, len);
        addrBuf[len] = 0;
        return len;
    }
    else
        return 0;
}

static CS104_Connection
createConnection(const char* hostname, int tcpPort)
{
    CS104_Connection self = (CS104_Connection)GLOBAL_CALLOC(1, sizeof(struct sCS104_Connection));

    if (self)
    {
        strncpy(self->hostname, hostname, HOST_NAME_MAX);
        self->hostname[HOST_NAME_MAX] = 0;
        self->tcpPort = tcpPort;
        self->parameters = defaultAPCIParameters;
        self->alParameters = defaultAppLayerParameters;

        self->localIpAddress = NULL;
        self->localTcpPort = -1;

        self->receivedHandler = NULL;
        self->receivedHandlerParameter = NULL;

        self->connectionHandler = NULL;
        self->connectionHandlerParameter = NULL;

        self->rawMessageHandler = NULL;
        self->rawMessageHandlerParameter = NULL;

        self->peerConnection.object = self;
        self->peerConnection.sendACT_CON = NULL;
        self->peerConnection.sendACT_TERM = NULL;
        self->peerConnection.isReady = _IPeerConnection_isReady;
        self->peerConnection.sendASDU = _IPeerConnection_sendASDU;
        self->peerConnection.close = _IPeerConnection_close;
        self->peerConnection.getPeerAddress = _IPeerConnection_getPeerAddress;
        self->peerConnection.getApplicationLayerParameters = _IPeerConnection_getApplicationLayerParameters;

#if (CONFIG_USE_SEMAPHORES == 1)
        self->conStateLock = Semaphore_create(1);
#endif

#if (CONFIG_USE_THREADS == 1)
        self->connectionHandlingThread = NULL;
#endif

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        self->tlsConfig = NULL;
        self->tlsSocket = NULL;
#endif

        self->sentASDUs = NULL;

        self->conState = STATE_IDLE;
        self->isThreadlessMode = false;

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

    if (self != NULL)
    {
        self->tlsConfig = tlsConfig;
        TLSConfiguration_setClientMode(tlsConfig);
    }

    return self;
}
#endif /* (CONFIG_CS104_SUPPORT_TLS == 1) */

static void
CS104_Connection_forwardASDU(CS101_MasterPlugin callingPlugin, void* ctx, CS101_ASDU asdu)
{
    CS104_Connection self = (CS104_Connection)ctx;

    if (self->conState != STATE_ACTIVE)
    {
        printf("CS104_Connection: ignore forwarded ASDU, connection not active\n");
        return;
    }

    /* call other plugins first */
    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);

        while (pluginElem)
        {
            CS101_MasterPlugin plugin = (CS101_MasterPlugin)LinkedList_getData(pluginElem);

            if (plugin->handleAsdu)
            {
                CS101_MasterPlugin_Result result = plugin->handleAsdu(plugin->parameter, NULL, asdu);

                if (result == CS101_MASTER_PLUGIN_RESULT_HANDLED)
                {
                    return;
                }
                else if (result == CS101_MASTER_PLUGIN_RESULT_INVALID_ASDU)
                {
                    DEBUG_PRINT("Invalid message");
                    return;
                }
            }

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    if (self->receivedHandler)
        self->receivedHandler(self->receivedHandlerParameter, -1, asdu);
}

void
CS104_Connection_addPlugin(CS104_Connection self, CS101_MasterPlugin plugin)
{
    if (self->plugins == NULL)
        self->plugins = LinkedList_create();

    if (self->plugins)
    {
        if (plugin->setForwardAsduFunction)
            plugin->setForwardAsduFunction(plugin->parameter,
                                           (CS101_MasterPluginForwardAsduFunc)CS104_Connection_forwardASDU, self);

        LinkedList_add(self->plugins, plugin);
    }
}

#ifdef SEC_AUTH_60870_5_7
void
CS104_Connection_setSecureEndpoint(CS104_Connection self, SecureEndpoint secureEndpoint)
{
    self->secureEndpoint = secureEndpoint;

    SecureEndpoint_addForwardASDUFunctionForMaster(secureEndpoint, CS104_Connection_forwardASDU, self);
}
#endif /* SEC_AUTH_60870_5_7 */

static void
invokeConnectionHandler(CS104_Connection self, CS104_ConnectionEvent event)
{
#ifdef SEC_AUTH_60870_5_7
    /* call events for security endpoint */

    if (self->secureEndpoint)
    {
        SecureEndpoint_handleCS104ConnectionEvent(self->secureEndpoint, &(self->peerConnection), (int)event);
    }

#endif /* SEC_AUTH_60870_5_7 */

    if (self->connectionHandler)
        self->connectionHandler(self->connectionHandlerParameter, self, event);
}

static void
resetT3Timeout(CS104_Connection self)
{
    self->nextT3Timeout = Hal_getMonotonicTimeInMs() + (self->parameters.t3 * 1000);
}

static void
resetConnection(CS104_Connection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

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

    if (self->sentASDUs == NULL)
    {
        self->maxSentASDUs = self->parameters.k;
        self->sentASDUs = (SentASDU*)GLOBAL_MALLOC(sizeof(SentASDU) * self->maxSentASDUs);
    }

    self->outstandingTestFRConMessages = 0;
    self->uMessageTimeout = 0;

    self->conState = STATE_IDLE;

    resetT3Timeout(self);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

static bool
checkSequenceNumber(CS104_Connection self, int seqNo)
{
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
        /* Two cases are required to reflect sequence number overflow */
        if (self->sentASDUs[self->oldestSentASDU].seqNo <= self->sentASDUs[self->newestSentASDU].seqNo)
        {
            if ((seqNo >= self->sentASDUs[self->oldestSentASDU].seqNo) &&
                (seqNo <= self->sentASDUs[self->newestSentASDU].seqNo))
                seqNoIsValid = true;
        }
        else
        {
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

    if (seqNoIsValid)
    {
        if (self->oldestSentASDU != -1)
        {
            do
            {
                if (counterOverflowDetected == false)
                {
                    if (seqNo < self->sentASDUs[self->oldestSentASDU].seqNo)
                        break;
                }

                if (seqNo == oldestValidSeqNo)
                    break;

                if (self->sentASDUs[self->oldestSentASDU].seqNo == seqNo)
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

    return seqNoIsValid;
}

void
CS104_Connection_close(CS104_Connection self)
{
    /* if threadless mode we simply set close flag and let CS104_Connection_run handle resource cleanup */
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    self->close = true;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

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

#ifdef SEC_AUTH_60870_5_7
    if (self->secureEndpoint)
        SecureEndpoint_removeForwardASDUFunction(self->secureEndpoint, self);
#endif /* SEC_AUTH_60870_5_7 */

    if (self->sentASDUs != NULL)
        GLOBAL_FREEMEM(self->sentASDUs);

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_destroy(self->conStateLock);
#endif

    if (self->localIpAddress)
    {
        GLOBAL_FREEMEM(self->localIpAddress);
        self->localIpAddress = NULL;
    }

    if (self->plugins)
    {
        LinkedList_destroyStatic(self->plugins);
        self->plugins = NULL;
    }

    GLOBAL_FREEMEM(self);
}

bool
CS104_Connection_isThreadless(CS104_Connection self)
{
    return self->isThreadlessMode;
}

/* Internal helper to perform socket connect similar to threaded path */
static bool
_CS104_Connection_establishSocket(CS104_Connection self)
{
    resetConnection(self);

    self->socket = TcpSocket_create();
    if (self->socket == NULL)
        return false;

    Socket_setConnectTimeout(self->socket, self->connectTimeoutInMs);

    if (self->localIpAddress)
        Socket_bind(self->socket, self->localIpAddress, self->localTcpPort);

    if (!Socket_connect(self->socket, self->hostname, self->tcpPort))
        return false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsConfig != NULL)
    {
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

    self->conState = STATE_INACTIVE;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif

    invokeConnectionHandler(self, CS104_CONNECTION_OPENED);
    return (self->failure == false) && (self->running == true);
}

bool
CS104_Connection_startThreadless(CS104_Connection self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->connectionHandlingThread) /* safety: don't allow both modes */
        return false;
#endif

    if (_CS104_Connection_establishSocket(self) == false)
    {
        /* cleanup if socket connect failed */
        if (self->socket)
        {
            Socket_destroy(self->socket);
            self->socket = NULL;
        }
        return false;
    }

    self->isThreadlessMode = true;
    return true;
}

void
CS104_Connection_stopThreadless(CS104_Connection self)
{
    if (self->isThreadlessMode == false)
        return;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif
    /* Confirm outstanding messages before closing */
    if (self->unconfirmedReceivedIMessages > 0)
        confirmOutstandingMessages(self);

    self->conState = STATE_IDLE;
    self->running = false;
    self->close = true;
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    if (self->tlsSocket)
    {
        TLSSocket_close(self->tlsSocket);
        self->tlsSocket = NULL;
    }
#endif
    if (self->socket)
    {
        Socket_destroy(self->socket);
        self->socket = NULL;
    }

    self->isThreadlessMode = false;
    invokeConnectionHandler(self, CS104_CONNECTION_CLOSED);
}

bool
CS104_Connection_run(CS104_Connection self, int timeoutMs)
{
    if (!self->isThreadlessMode || !self->running || self->socket == NULL)
        return false;

    bool continueLoop = true;

    HandleSet handleSet = Handleset_new();
    Handleset_reset(handleSet);
    Handleset_addSocket(handleSet, self->socket);

    int waitTime = (timeoutMs > 0) ? timeoutMs : 0;
    if (Handleset_waitReady(handleSet, waitTime))
    {
        int bytesRec = receiveMessage(self);
        if (bytesRec == -1)
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->conStateLock);
#endif
            self->failure = true;
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->conStateLock);
#endif
            continueLoop = false;
        }
        else if (bytesRec > 0)
        {
            if (self->rawMessageHandler)
                self->rawMessageHandler(self->rawMessageHandlerParameter, self->recvBuffer, bytesRec, false);

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->conStateLock);
#endif
            CS104_ConState oldState = self->conState;
            if (checkMessage(self, self->recvBuffer, bytesRec) == false)
            {
                self->failure = true;
                continueLoop = false;
            }
            CS104_ConState newState = self->conState;
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->conStateLock);
#endif
            if (newState != oldState)
            {
                if (newState == STATE_ACTIVE)
                    invokeConnectionHandler(self, CS104_CONNECTION_STARTDT_CON_RECEIVED);
                else if (newState == STATE_INACTIVE)
                    invokeConnectionHandler(self, CS104_CONNECTION_STOPDT_CON_RECEIVED);
            }
        }
    }

    /* confirmations */
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif
    if ((self->unconfirmedReceivedIMessages >= self->parameters.w) ||
        ((self->conState == STATE_WAITING_FOR_STOPDT_CON) && (self->unconfirmedReceivedIMessages > 0)))
    {
        confirmOutstandingMessages(self);
    }
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif

    if (handleTimeouts(self) == false)
        continueLoop = false;

    if (isClose(self))
        continueLoop = false;

#ifdef SEC_AUTH_60870_5_7
    if (self->secureEndpoint && self->conState == STATE_ACTIVE)
    {
        if (SecureEndpoint_runTask(self->secureEndpoint, &(self->peerConnection)) == false)
            continueLoop = false;
    }
#endif

    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);
        while (pluginElem)
        {
            CS101_MasterPlugin plugin = (CS101_MasterPlugin)LinkedList_getData(pluginElem);
            if (plugin->runTask)
                plugin->runTask(plugin->parameter, &(self->peerConnection));
            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    Handleset_destroy(handleSet);

    if (continueLoop == false)
    {
        /* perform cleanup similar to threaded path */
        CS104_Connection_stopThreadless(self); /* will invoke CLOSED event */
    }

    return continueLoop;
}

void
CS104_Connection_setLocalAddress(CS104_Connection self, const char* localIpAddress, int localPort)
{
    if (self)
    {
        if (self->localIpAddress)
        {
            GLOBAL_FREEMEM(self->localIpAddress);
            self->localIpAddress = NULL;
        }

        if (localIpAddress)
        {
            self->localIpAddress = strdup(localIpAddress);
            self->localTcpPort = localPort;
        }
    }
}

void
CS104_Connection_setAPCIParameters(CS104_Connection self, const CS104_APCIParameters parameters)
{
    self->parameters = *parameters;

    self->connectTimeoutInMs = self->parameters.t0 * 1000;
}

void
CS104_Connection_setAppLayerParameters(CS104_Connection self, const CS101_AppLayerParameters parameters)
{
    self->alParameters = *parameters;
}

CS101_AppLayerParameters
CS104_Connection_getAppLayerParameters(CS104_Connection self)
{
    return &(self->alParameters);
}

void
CS104_Connection_setOriginatorAddress(CS104_Connection self, uint8_t originatorAddress)
{
    self->alParameters.originatorAddress = originatorAddress;
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
        int readCnt = readFromSocket(self, buffer + 1, 1);

        if (readCnt < 0)
        {
            self->recvBufPos = 0;
            return -1;
        }
        else if (readCnt == 0)
        {
            self->recvBufPos = 1;
            return 0;
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

static bool
checkConfirmTimeout(CS104_Connection self, uint64_t currentTime)
{
    if (currentTime > self->lastConfirmationTime)
    {
        if ((currentTime - self->lastConfirmationTime) >= (uint32_t)(self->parameters.t2 * 1000))
        {
            return true;
        }
    }

    return false;
}

static void
confirmOutstandingMessages(CS104_Connection self)
{
    self->lastConfirmationTime = Hal_getMonotonicTimeInMs();
    self->unconfirmedReceivedIMessages = 0;
    self->timeoutT2Trigger = false;
    sendSMessage(self);
}

/* returns true when the ASDU was handled by the plugin and can be discarded */
static bool
callPluginsToHandleAsdu(CS104_Connection self, CS101_ASDU asdu)
{
    bool retVal = false;

#ifdef SEC_AUTH_60870_5_7
    if (self->secureEndpoint)
    {
        if (SecureEndpoint_asduReceived(self->secureEndpoint, &(self->peerConnection), asdu, NULL, 0))
        {
            return true;
        }
    }
#endif /* SEC_AUTH_60870_5_7 */

    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);

        while (pluginElem)
        {
            CS101_MasterPlugin plugin = (CS101_MasterPlugin)LinkedList_getData(pluginElem);

            CS101_MasterPlugin_Result result = plugin->handleAsdu(plugin->parameter, &(self->peerConnection), asdu);

            if (result == CS101_MASTER_PLUGIN_RESULT_HANDLED)
            {
                retVal = true;
                break;
            }

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    return retVal;
}

static bool
checkMessage(CS104_Connection self, uint8_t* buffer, int msgSize)
{
    bool retVal = true;

    if ((buffer[2] & 1) == 0) /* I format frame */
    {
        if (self->timeoutT2Trigger == false)
        {
            self->timeoutT2Trigger = true;
            self->lastConfirmationTime = Hal_getMonotonicTimeInMs(); /* start timeout T2 */
        }

        if (msgSize < 7)
        {
            DEBUG_PRINT("I msg too small!\n");
            retVal = false;

            goto exit_function;
        }

        int frameSendSequenceNumber = ((buffer[3] * 0x100) + (buffer[2] & 0xfe)) / 2;
        int frameRecvSequenceNumber = ((buffer[5] * 0x100) + (buffer[4] & 0xfe)) / 2;

        DEBUG_PRINT("Received I frame: N(S) = %i N(R) = %i\n", frameSendSequenceNumber, frameRecvSequenceNumber);

        /* check the receive sequence number N(R) - connection will be closed on an unexpected value */
        if (frameSendSequenceNumber != self->receiveCount)
        {
            DEBUG_PRINT("Sequence error: Close connection!");

            retVal = false;

            goto exit_function;
        }

        if (checkSequenceNumber(self, frameRecvSequenceNumber) == false)
        {
            retVal = false;

            goto exit_function;
        }

        self->receiveCount = (self->receiveCount + 1) % 32768;
        self->unconfirmedReceivedIMessages++;

        struct sCS101_ASDU _asdu;

        CS101_ASDU asdu = CS101_ASDU_createFromBufferEx(&_asdu, (CS101_AppLayerParameters) & (self->alParameters),
                                                        buffer + 6, msgSize - 6);

        if (asdu)
        {
            if (callPluginsToHandleAsdu(self, asdu) == false)
            {
                if (self->receivedHandler != NULL)
                    self->receivedHandler(self->receivedHandlerParameter, 0, asdu);
            }
        }
        else
        {
            retVal = false;

            goto exit_function;
        }
    }
    else if ((buffer[2] & 0x03) == 0x03) /* U format frame */
    {
        DEBUG_PRINT("Received U frame\n");

        self->uMessageTimeout = 0;

        if (buffer[2] == 0x43)
        { /* Check for TESTFR_ACT message */
            DEBUG_PRINT("Send TESTFR_CON\n");

            writeToSocket(self, TESTFR_CON_MSG, TESTFR_CON_MSG_SIZE);
        }
        else if (buffer[2] == 0x83)
        { /* TESTFR_CON */
            DEBUG_PRINT("Rcvd TESTFR_CON\n");
            self->outstandingTestFRConMessages = 0;
        }
        else if (buffer[2] == 0x07)
        { /* STARTDT_ACT */
            DEBUG_PRINT("Send STARTDT_CON\n");

            writeToSocket(self, STARTDT_CON_MSG, STARTDT_CON_MSG_SIZE);

            self->conState = STATE_ACTIVE;
        }
        else if (buffer[2] == 0x0b)
        { /* STARTDT_CON */
            DEBUG_PRINT("Received STARTDT_CON\n");

            self->conState = STATE_ACTIVE;
        }
        else if (buffer[2] == 0x23)
        { /* STOPDT_CON */
            DEBUG_PRINT("Received STOPDT_CON\n");

            self->conState = STATE_INACTIVE;
        }
    }
    else if (buffer[2] == 0x01) /* S-message */
    {
        int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

        DEBUG_PRINT("Rcvd S(%i) (own sendcounter = %i)\n", seqNo, self->sendCount);

        if (checkSequenceNumber(self, seqNo) == false)
        {
            retVal = false;

            goto exit_function;
        }
    }

    resetT3Timeout(self);

exit_function:

    return retVal;
}

static bool
handleTimeouts(CS104_Connection self)
{
    bool retVal = true;

    uint64_t currentTime = Hal_getMonotonicTimeInMs();

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    if (currentTime > self->nextT3Timeout)
    {
        if (self->outstandingTestFRConMessages > 2)
        {
            DEBUG_PRINT("Timeout for TESTFR_CON message\n");

            /* close connection */
            retVal = false;
            goto exit_function;
        }
        else
        {
            DEBUG_PRINT("U message T3 timeout\n");

            writeToSocket(self, TESTFR_ACT_MSG, TESTFR_ACT_MSG_SIZE);

            self->uMessageTimeout = currentTime + (self->parameters.t1 * 1000);
            self->outstandingTestFRConMessages++;

            resetT3Timeout(self);
        }
    }

    if (self->unconfirmedReceivedIMessages > 0)
    {
        if (checkConfirmTimeout(self, currentTime))
        {
            confirmOutstandingMessages(self);
        }
    }

    if (self->uMessageTimeout != 0)
    {
        if (currentTime > self->uMessageTimeout)
        {
            DEBUG_PRINT("U message T1 timeout\n");
            retVal = false;
            goto exit_function;
        }
    }

    /* check if counterpart confirmed I messages */
    if (self->oldestSentASDU != -1)
    {
        if (currentTime > self->sentASDUs[self->oldestSentASDU].sentTime)
        {
            if ((currentTime - self->sentASDUs[self->oldestSentASDU].sentTime) >=
                (uint64_t)(self->parameters.t1 * 1000))
            {
                DEBUG_PRINT("I message timeout\n");
                retVal = false;
            }
        }
    }

exit_function:

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    return retVal;
}

static bool
isFailure(CS104_Connection self)
{
    bool isFailure;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    isFailure = self->failure;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    return isFailure;
}

static bool
isClose(CS104_Connection self)
{
    bool isClose;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    isClose = self->close;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    return isClose;
}

#if (CONFIG_USE_THREADS == 1)
static void*
handleConnection(void* parameter)
{
    CS104_Connection self = (CS104_Connection)parameter;

    CS104_ConnectionEvent event = CS104_CONNECTION_OPENED;

    resetConnection(self);

    self->socket = TcpSocket_create();

    if (self->socket)
    {
        Socket_setConnectTimeout(self->socket, self->connectTimeoutInMs);

        if (self->localIpAddress)
        {
            Socket_bind(self->socket, self->localIpAddress, self->localTcpPort);
        }

        if (Socket_connect(self->socket, self->hostname, self->tcpPort))
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

#if (CONFIG_CS104_SUPPORT_TLS == 1)
            if (self->tlsConfig != NULL)
            {
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

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

            if (isRunning(self))
            {
#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                self->conState = STATE_INACTIVE;

#if (CONFIG_USE_SEMAPHORES == 1)
                Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                /* Call connection handler */
                invokeConnectionHandler(self, CS104_CONNECTION_OPENED);

                HandleSet handleSet = Handleset_new();

                bool loopRunning = true;

                while (loopRunning)
                {
                    Handleset_reset(handleSet);
                    Handleset_addSocket(handleSet, self->socket);

                    if (Handleset_waitReady(handleSet, 100))
                    {
                        int bytesRec = receiveMessage(self);

                        if (bytesRec == -1)
                        {
                            loopRunning = false;

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                            self->failure = true;

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                        }

                        if (bytesRec > 0)
                        {
                            if (self->rawMessageHandler)
                                self->rawMessageHandler(self->rawMessageHandlerParameter, self->recvBuffer, bytesRec,
                                                        false);

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                            CS104_ConState oldState = self->conState;

                            if (checkMessage(self, self->recvBuffer, bytesRec) == false)
                            {
                                /* close connection on error */
                                loopRunning = false;

                                self->failure = true;
                            }

                            CS104_ConState newState = self->conState;

#if (CONFIG_USE_SEMAPHORES == 1)
                            Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                            /* call connection handler when required */
                            if (newState != oldState)
                            {
                                if (newState == STATE_ACTIVE)
                                    invokeConnectionHandler(self, CS104_CONNECTION_STARTDT_CON_RECEIVED);
                                else if (newState == STATE_INACTIVE)
                                    invokeConnectionHandler(self, CS104_CONNECTION_STOPDT_CON_RECEIVED);
                            }
                        }

#if (CONFIG_USE_SEMAPHORES == 1)
                        Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

                        if ((self->unconfirmedReceivedIMessages >= self->parameters.w) ||
                            ((self->conState == STATE_WAITING_FOR_STOPDT_CON) && (self->unconfirmedReceivedIMessages > 0)))
                        {
                            confirmOutstandingMessages(self);
                        }

#if (CONFIG_USE_SEMAPHORES == 1)
                        Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
                    }

                    if (handleTimeouts(self) == false)
                        loopRunning = false;

                    if (isClose(self))
                        loopRunning = false;

#if (CONFIG_CS104_SUPPORT_TLS == 1)
                    if (self->tlsSocket != NULL)
                    {
                        if (TLSSocket_tick(self->tlsSocket) == false)
                        {
                            loopRunning = false;
                        }
                    }
#endif /* (CONFIG_CS104_SUPPORT_TLS == 1) */

#ifdef SEC_AUTH_60870_5_7
                    if (self->secureEndpoint)
                    {
                        if (self->conState == STATE_ACTIVE)
                        {
                            if (SecureEndpoint_runTask(self->secureEndpoint, &(self->peerConnection)) == false)
                            {
                                event = CS104_CONNECTION_CLOSED;

                                loopRunning = false;
                            }
                        }
                    }
#endif /* SEC_AUTH_60870_5_7 */

                    if (self->plugins)
                    {
                        LinkedList pluginElem = LinkedList_getNext(self->plugins);

                        while (pluginElem)
                        {
                            CS101_MasterPlugin plugin = (CS101_MasterPlugin)LinkedList_getData(pluginElem);

                            if (plugin->runTask)
                                plugin->runTask(plugin->parameter, &(self->peerConnection));

                            pluginElem = LinkedList_getNext(pluginElem);
                        }
                    }
                }

                Handleset_destroy(handleSet);

                /* register CLOSED event */
                event = CS104_CONNECTION_CLOSED;
            }
        }
        else
        {
#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
            self->failure = true;

#if (CONFIG_USE_SEMAPHORES == 1)
            Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

            /* register CLOSED event */
            event = CS104_CONNECTION_FAILED;
        }

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

        /* Confirm all unconfirmed received I-messages before closing the connection */
        if (self->unconfirmedReceivedIMessages > 0)
        {
            confirmOutstandingMessages(self);
        }

#if (CONFIG_CS104_SUPPORT_TLS == 1)
        if (self->tlsSocket)
        {
            TLSSocket_close(self->tlsSocket);
            self->tlsSocket = NULL;
        }
#endif

        Socket_destroy(self->socket);
        self->socket = NULL;

        self->conState = STATE_IDLE;

        self->running = false;

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
    }
    else
    {
        DEBUG_PRINT("Failed to create socket\n");

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

        self->running = false;

#if (CONFIG_USE_SEMAPHORES == 1)
        Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
    }

    /* Call connection handler */
    if ((event == CS104_CONNECTION_CLOSED) || (event == CS104_CONNECTION_FAILED))
    {
        invokeConnectionHandler(self, event);
    }

    return NULL;
}
#endif /* (CONFIG_USE_THREADS == 1) */

void
CS104_Connection_connectAsync(CS104_Connection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    self->running = false;
    self->failure = false;
    self->close = false;

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

#if (CONFIG_USE_THREADS == 1)
    if (self->connectionHandlingThread)
    {
        Thread_destroy(self->connectionHandlingThread);
        self->connectionHandlingThread = NULL;
    }

    self->connectionHandlingThread = Thread_create(handleConnection, (void*)self, false);

    if (self->connectionHandlingThread)
        Thread_start(self->connectionHandlingThread);
#endif
}

bool
CS104_Connection_connect(CS104_Connection self)
{
    CS104_Connection_connectAsync(self);

    while ((isRunning(self) == false) && (isFailure(self) == false))
        Thread_sleep(1);

    return isRunning(self);
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

void
CS104_Connection_sendStartDT(CS104_Connection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    if (self->socket)
    {
        self->conState = STATE_WAITING_FOR_STARTDT_CON;
        self->uMessageTimeout = Hal_getMonotonicTimeInMs() + (self->parameters.t1 * 1000);

        writeToSocket(self, STARTDT_ACT_MSG, STARTDT_ACT_MSG_SIZE);
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

/* this function is only for test purposes and not part of the API! */
int
CS104_Connection_sendMessage(CS104_Connection self, uint8_t* message, int messageSize)
{
    return writeToSocket(self, message, messageSize);
}

void
CS104_Connection_sendStopDT(CS104_Connection self)
{
#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_wait(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */

    if (self->socket)
    {
        if (self->unconfirmedReceivedIMessages > 0)
            confirmOutstandingMessages(self);

        self->conState = STATE_WAITING_FOR_STOPDT_CON;
        self->uMessageTimeout = Hal_getMonotonicTimeInMs() + (self->parameters.t1 * 1000);

        writeToSocket(self, STOPDT_ACT_MSG, STOPDT_ACT_MSG_SIZE);
    }

#if (CONFIG_USE_SEMAPHORES == 1)
    Semaphore_post(self->conStateLock);
#endif /* (CONFIG_USE_SEMAPHORES == 1) */
}

bool
CS104_Connection_sendInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca,
                                          QualifierOfInterrogation qoi)
{
    struct sInterrogationCommand _interrogationCommand;

    InterrogationCommand interrogationCommand = InterrogationCommand_create(&_interrogationCommand, 0, qoi);

    return CS104_Connection_sendProcessCommandEx(self, cot, ca, (InformationObject)interrogationCommand);
}

bool
CS104_Connection_sendCounterInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca,
                                                 uint8_t qcc)
{
    struct sCounterInterrogationCommand _counterInterrogationCommand;

    CounterInterrogationCommand counterInterrogationCommand =
        CounterInterrogationCommand_create(&_counterInterrogationCommand, 0, qcc);

    return CS104_Connection_sendProcessCommandEx(self, cot, ca, (InformationObject)counterInterrogationCommand);
}

bool
CS104_Connection_sendReadCommand(CS104_Connection self, int ca, int ioa)
{
    struct sReadCommand _readCommand;

    ReadCommand readCommand = ReadCommand_create(&_readCommand, ioa);

    return CS104_Connection_sendProcessCommandEx(self, CS101_COT_REQUEST, ca, (InformationObject)readCommand);
}

bool
CS104_Connection_sendClockSyncCommand(CS104_Connection self, int ca, CP56Time2a newTime)
{
    struct sClockSynchronizationCommand _clockSyncCommand;

    ClockSynchronizationCommand clockSyncCommand = ClockSynchronizationCommand_create(&_clockSyncCommand, 0, newTime);

    return CS104_Connection_sendProcessCommandEx(self, CS101_COT_ACTIVATION, ca, (InformationObject)clockSyncCommand);
}

bool
CS104_Connection_sendTestCommand(CS104_Connection self, int ca)
{
    struct sTestCommand _testCommand;

    TestCommand testCommand = TestCommand_create(&_testCommand);

    return CS104_Connection_sendProcessCommandEx(self, CS101_COT_ACTIVATION, ca, (InformationObject)testCommand);
}

bool
CS104_Connection_sendTestCommandWithTimestamp(CS104_Connection self, int ca, uint16_t tsc, CP56Time2a timestamp)
{
    struct sTestCommandWithCP56Time2a _testCommand;

    TestCommandWithCP56Time2a testCommand = TestCommandWithCP56Time2a_create(&_testCommand, tsc, timestamp);

    return CS104_Connection_sendProcessCommandEx(self, CS101_COT_ACTIVATION, ca, (InformationObject)testCommand);
}

bool
CS104_Connection_sendProcessCommand(CS104_Connection self, TypeID typeId, CS101_CauseOfTransmission cot, int ca,
                                    InformationObject sc)
{
    UNUSED_PARAMETER(typeId);

    return CS104_Connection_sendProcessCommandEx(self, cot, ca, sc);
}

bool
CS104_Connection_sendProcessCommandEx(CS104_Connection self, CS101_CauseOfTransmission cot, int ca,
                                      InformationObject sc)
{
    sCS101_StaticASDU sAsdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic((CS101_StaticASDU)&sAsdu, &(self->alParameters), false, cot,
                                                  self->alParameters.originatorAddress, ca, false, false);

    CS101_ASDU_addInformationObject(asdu, sc);

    return CS104_Connection_sendASDU(self, asdu);
}

bool
CS104_Connection_sendASDU(CS104_Connection self, CS101_ASDU asdu)
{
    bool asduSent = false;

#ifdef SEC_AUTH_60870_5_7
    if (self->secureEndpoint)
    {
        if (SecureEndpoint_sendAsdu(self->secureEndpoint, &(self->peerConnection), asdu))
        {
            return true;
        }
    }
#endif /* SEC_AUTH_60870_5_7 */

    /* call plugins */
    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);

        while (pluginElem)
        {
            CS101_MasterPlugin plugin = (CS101_MasterPlugin)LinkedList_getData(pluginElem);

            CS101_MasterPlugin_Result result = plugin->sendAsdu(plugin->parameter, &(self->peerConnection), asdu);

            if (result == CS101_MASTER_PLUGIN_RESULT_HANDLED)
            {
                asduSent = true;
            }

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    if (asduSent == false)
    {
        struct sT104Frame _frame;

        Frame frame = (Frame)T104Frame_createEx(&_frame);

        CS101_ASDU_encode(asdu, frame);

        return sendASDUInternal(self, frame);
    }

    return true;
}

bool
CS104_Connection_isTransmitBufferFull(CS104_Connection self)
{
    return isSentBufferFull(self);
}
