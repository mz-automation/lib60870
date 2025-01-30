/*
 *  cs101_master.c
 *
 *  Copyright 2017 MZ Automation GmbH
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "buffer_frame.h"
#include "lib_memory.h"
#include "apl_types_internal.h"
#include "lib60870_config.h"
#include "lib60870_internal.h"
#include "serial_transceiver_ft_1_2.h"
#include "link_layer.h"
#include "cs101_master.h"
#include "cs101_queue.h"
#include "cs101_asdu_internal.h"


struct sCS101_Master
{
    SerialPort serialPort;

    struct sLinkLayerParameters linkLayerParameters;
    struct sCS101_AppLayerParameters alParameters;

    SerialTransceiverFT12 transceiver;

    IEC60870_LinkLayerMode linkLayerMode;

    LinkLayerBalanced balancedLinkLayer;

    LinkLayerPrimaryUnbalanced unbalancedLinkLayer;
    int slaveAddress; /* address of the currently selected slave (unbalanced link layer only) */

    CS101_ASDUReceivedHandler asduReceivedHandler;
    void* asduReceivedHandlerParameter;

    struct sCS101_Queue userDataQueue;

#if (CONFIG_USE_THREADS == 1)
    bool isRunning;
    Thread workerThread;
#endif
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

/********************************************
 * IBalancedApplicationLayer
 ********************************************/
static Frame
IBalancedApplicationLayer_GetUserData (void* parameter, Frame frame)
{
    CS101_Master self = (CS101_Master) parameter;

    Frame ret = NULL;

    CS101_Queue_lock(&(self->userDataQueue));

    ret = CS101_Queue_dequeue(&(self->userDataQueue), frame);

    CS101_Queue_unlock(&(self->userDataQueue));

    return ret;
}

static bool
IBalancedApplicationLayer_HandleReceivedData (void* parameter, uint8_t* msg, bool isBroadcast, int userDataStart, int userDataLength)
{
    UNUSED_PARAMETER(isBroadcast);

    CS101_Master self = (CS101_Master) parameter;

    struct sCS101_ASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_createFromBufferEx(&_asdu, &(self->alParameters), msg + userDataStart, userDataLength);

    if (self->asduReceivedHandler)
        self->asduReceivedHandler(self->asduReceivedHandlerParameter, 0, asdu);

    return true;
}

static struct sIBalancedApplicationLayer cs101BalancedAppLayerInterface = {
    IBalancedApplicationLayer_GetUserData,
    IBalancedApplicationLayer_HandleReceivedData
};

/********************************************
 * END IBalancedApplicationLayer
 ********************************************/

/********************************************
 * IPrimaryApplicationLayer
 ********************************************/

static void
IPrimaryApplicationLayer_AccessDemand(void* parameter, int slaveAddress)
{
    CS101_Master self = (CS101_Master) parameter;

    DEBUG_PRINT ("MASTER: Access demand for slave %i\n", slaveAddress);

    LinkLayerPrimaryUnbalanced_requestClass1Data(self->unbalancedLinkLayer, slaveAddress);
}

static void
IPrimaryApplicationLayer_UserData(void* parameter, int slaveAddress, uint8_t* msg, int start, int length)
{
    CS101_Master self = (CS101_Master) parameter;

    struct sCS101_ASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_createFromBufferEx(&_asdu, &(self->alParameters), msg + start, length);

    if (self->asduReceivedHandler)
        self->asduReceivedHandler(self->asduReceivedHandlerParameter, slaveAddress, asdu);
}

static void
IPrimaryApplicationLayer_Timeout (void* parameter, int slaveAddress)
{
    UNUSED_PARAMETER(parameter);
    UNUSED_PARAMETER(slaveAddress);
}

static struct sIPrimaryApplicationLayer cs101UnbalancedAppLayerInterface = {
    IPrimaryApplicationLayer_AccessDemand,
    IPrimaryApplicationLayer_UserData,
    IPrimaryApplicationLayer_Timeout
};

/********************************************
 * END IPrimaryApplicationLayer
 ********************************************/

CS101_Master
CS101_Master_createEx(SerialPort serialPort, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode,
        int queueSize)
{
    CS101_Master self = (CS101_Master) GLOBAL_MALLOC(sizeof(struct sCS101_Master));

    if (self != NULL) {

        if (llParameters)
            self->linkLayerParameters = *llParameters;
        else {
            self->linkLayerParameters.addressLength = 1;
            self->linkLayerParameters.timeoutForAck = 200;
            self->linkLayerParameters.timeoutRepeat = 1000;
            self->linkLayerParameters.timeoutLinkState = 5000;
            self->linkLayerParameters.useSingleCharACK = true;
        }

        if (alParameters)
            self->alParameters = *alParameters;
        else
            self->alParameters = defaultAppLayerParameters;

        self->transceiver = SerialTransceiverFT12_create(serialPort,  &(self->linkLayerParameters));

        self->linkLayerMode = linkLayerMode;

        if (linkLayerMode == IEC60870_LINK_LAYER_UNBALANCED) {

            self->balancedLinkLayer = NULL;

            self->unbalancedLinkLayer = LinkLayerPrimaryUnbalanced_create(self->transceiver,
                    &(self->linkLayerParameters), &cs101UnbalancedAppLayerInterface, self);
        }
        else {
            CS101_Queue_initialize(&(self->userDataQueue), queueSize);

            self->unbalancedLinkLayer = NULL;

            self->balancedLinkLayer = LinkLayerBalanced_create(0, self->transceiver,
                    &(self->linkLayerParameters),
                    &cs101BalancedAppLayerInterface, self);

            LinkLayerBalanced_setDIR(self->balancedLinkLayer, true);
        }

        self->asduReceivedHandler = NULL;

#if (CONFIG_USE_THREADS == 1)
        self->isRunning = false;
        self->workerThread = NULL;
#endif

    }

    return self;
}

CS101_Master
CS101_Master_create(SerialPort serialPort, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode)
{
    return CS101_Master_createEx(serialPort, llParameters, alParameters, linkLayerMode, CS101_MAX_QUEUE_SIZE);
}

void
CS101_Master_run(CS101_Master self)
{
    if (self->unbalancedLinkLayer) {
        LinkLayerPrimaryUnbalanced_run(self->unbalancedLinkLayer);
    }
    else {
        LinkLayerBalanced_run(self->balancedLinkLayer);
    }
}

#if (CONFIG_USE_THREADS == 1)
static void*
masterMainThread(void* parameter)
{
    CS101_Master self = (CS101_Master) parameter;

    self->isRunning = true;

    while (self->isRunning) {
        CS101_Master_run(self);
    }

    return NULL;
}
#endif /* (CONFIG_USE_THREADS == 1) */

void
CS101_Master_start(CS101_Master self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->workerThread == NULL) {
        self->workerThread = Thread_create(masterMainThread, self, false);
        Thread_start(self->workerThread);
    }
#endif /* (CONFIG_USE_THREADS == 1) */
}

void
CS101_Master_stop(CS101_Master self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->isRunning) {
        self->isRunning = false;
        Thread_destroy(self->workerThread);
        self->workerThread = NULL;
    }
#endif /* (CONFIG_USE_THREADS == 1) */
}


void
CS101_Master_destroy(CS101_Master self)
{
    if (self) {

        if (self->balancedLinkLayer) {
            LinkLayerBalanced_destroy(self->balancedLinkLayer);
            CS101_Queue_dispose(&(self->userDataQueue));
        }

        if (self->unbalancedLinkLayer) {
            LinkLayerPrimaryUnbalanced_destroy(self->unbalancedLinkLayer);
        }

        SerialTransceiverFT12_destroy(self->transceiver);

        GLOBAL_FREEMEM(self);
    }
}

void
CS101_Master_setDIR(CS101_Master self, bool dir)
{
    if (self->balancedLinkLayer)
        LinkLayerBalanced_setDIR(self->balancedLinkLayer, dir);
}

void
CS101_Master_setOwnAddress(CS101_Master self, int address)
{
    if (self->balancedLinkLayer)
        LinkLayerBalanced_setAddress(self->balancedLinkLayer, address);
}

void
CS101_Master_useSlaveAddress(CS101_Master self, int address)
{
    self->slaveAddress = address;

    if (self->balancedLinkLayer) {
        LinkLayerBalanced_setOtherStationAddress(self->balancedLinkLayer, address);
    }
}

LinkLayerParameters
CS101_Master_getLinkLayerParameters(CS101_Master self)
{
    return &(self->linkLayerParameters);
}

CS101_AppLayerParameters
CS101_Master_getAppLayerParameters(CS101_Master self)
{
    return &(self->alParameters);
}


bool
CS101_Master_isChannelReady(CS101_Master self, int address)
{
    if (self->unbalancedLinkLayer)
        return LinkLayerPrimaryUnbalanced_isChannelAvailable(self->unbalancedLinkLayer, address);

    return false;
}

void
CS101_Master_sendLinkLayerTestFunction(CS101_Master self)
{
    if (self->unbalancedLinkLayer)
        LinkLayerPrimaryUnbalanced_sendLinkLayerTestFunction(self->unbalancedLinkLayer,
                self->slaveAddress);
    else if (self->balancedLinkLayer)
        LinkLayerBalanced_sendLinkLayerTestFunction(self->balancedLinkLayer);
}

void
CS101_Master_sendInterrogationCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, QualifierOfInterrogation qoi)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, cot, self->alParameters.originatorAddress, ca, false, false);

    struct sInterrogationCommand _io;

    InformationObject io = (InformationObject) InterrogationCommand_create(&_io, 0, qoi);

    CS101_ASDU_addInformationObject(asdu, io);

    CS101_Master_sendASDU(self, asdu);
}

void
CS101_Master_sendCounterInterrogationCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, uint8_t qcc)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, cot, self->alParameters.originatorAddress, ca, false, false);

    struct sCounterInterrogationCommand _io;

    InformationObject io = (InformationObject) CounterInterrogationCommand_create(&_io, 0, qcc);

    CS101_ASDU_addInformationObject(asdu, io);

    CS101_Master_sendASDU(self, asdu);
}

void
CS101_Master_sendReadCommand(CS101_Master self, int ca, int ioa)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, CS101_COT_REQUEST, self->alParameters.originatorAddress, ca, false, false);

    struct sReadCommand _io;

    InformationObject io = (InformationObject) ReadCommand_create(&_io, ioa);

    CS101_ASDU_addInformationObject(asdu, io);

    CS101_Master_sendASDU(self, asdu);
}

void
CS101_Master_sendClockSyncCommand(CS101_Master self, int ca, CP56Time2a time)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, CS101_COT_ACTIVATION, self->alParameters.originatorAddress, ca, false, false);

    struct sClockSynchronizationCommand _io;

    InformationObject io = (InformationObject) ClockSynchronizationCommand_create(&_io, 0, time);

    CS101_ASDU_addInformationObject(asdu, io);

    CS101_Master_sendASDU(self, asdu);
}

void
CS101_Master_sendTestCommand(CS101_Master self, int ca)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, CS101_COT_ACTIVATION, self->alParameters.originatorAddress, ca, false, false);

    struct sTestCommand _io;

    InformationObject io = (InformationObject) TestCommand_create(&_io);

    CS101_ASDU_addInformationObject(asdu, io);

    CS101_Master_sendASDU(self, asdu);
}

void
CS101_Master_sendProcessCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, InformationObject command)
{
    sCS101_StaticASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_initializeStatic(&_asdu, &(self->alParameters), false, cot, self->alParameters.originatorAddress, ca, false, false);

    CS101_ASDU_addInformationObject(asdu, command);

    CS101_Master_sendASDU(self, asdu);
}

static bool
isBroadcastAddress(CS101_Master self, int address)
{
    if (self->linkLayerParameters.addressLength == 1) {
        return (address == 255);
    }
    else if (self->linkLayerParameters.addressLength == 2) {
        return (address == 65535);
    }

    return 0;
}

void
CS101_Master_sendASDU(CS101_Master self, CS101_ASDU asdu)
{
    if (self->unbalancedLinkLayer) {

        struct sBufferFrame bufferFrame;
        uint8_t buffer[256];
        BufferFrame_initialize(&bufferFrame, buffer, 0);

        CS101_ASDU_encode(asdu, (Frame) &bufferFrame);

        if (isBroadcastAddress(self, self->slaveAddress))
            LinkLayerPrimaryUnbalanced_sendNoReply(self->unbalancedLinkLayer, self->slaveAddress, &bufferFrame);
        else
            LinkLayerPrimaryUnbalanced_sendConfirmed(self->unbalancedLinkLayer, self->slaveAddress, &bufferFrame);
    }
    else
        CS101_Queue_enqueue(&(self->userDataQueue), asdu);
}

void
CS101_Master_addSlave(CS101_Master self, int address)
{
    if (self->unbalancedLinkLayer)
        LinkLayerPrimaryUnbalanced_addSlaveConnection(self->unbalancedLinkLayer, address);
}

void
CS101_Master_pollSingleSlave(CS101_Master self, int address)
{
    if (self->unbalancedLinkLayer) {
        LinkLayerPrimaryUnbalanced_requestClass2Data(self->unbalancedLinkLayer, address);
    }
}

void
CS101_Master_setASDUReceivedHandler(CS101_Master self, CS101_ASDUReceivedHandler handler, void* parameter)
{
    self->asduReceivedHandler = handler;
    self->asduReceivedHandlerParameter = parameter;
}

void
CS101_Master_setLinkLayerStateChanged(CS101_Master self, IEC60870_LinkLayerStateChangedHandler handler, void* parameter)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_BALANCED) {
        LinkLayerBalanced_setStateChangeHandler(self->balancedLinkLayer, handler, parameter);
    }
    else {
        LinkLayerPrimaryUnbalanced_setStateChangeHandler(self->unbalancedLinkLayer, handler, parameter);
    }
}

void
CS101_Master_setRawMessageHandler(CS101_Master self, IEC60870_RawMessageHandler handler, void* parameter)
{
    SerialTransceiverFT12_setRawMessageHandler(self->transceiver, handler, parameter);
}

void
CS101_Master_setIdleTimeout(CS101_Master self, int timeoutInMs)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_BALANCED)
        LinkLayerBalanced_setIdleTimeout(self->balancedLinkLayer, timeoutInMs);

    /* unbalanced primary layer does not support automatic idle detection */
}

