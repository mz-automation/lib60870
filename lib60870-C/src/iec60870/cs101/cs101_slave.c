/*
 *  cs101_slave.c
 *
 *  Copyright 2017-2025 Michael Zillgith
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

#include "cs101_slave.h"
#include "apl_types_internal.h"
#include "buffer_frame.h"
#include "cs101_asdu_internal.h"
#include "cs101_queue.h"
#include "iec60870_slave.h"
#include "lib60870_config.h"
#include "lib60870_internal.h"
#include "lib_memory.h"
#include "link_layer.h"
#include "linked_list.h"
#include "serial_transceiver_ft_1_2.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if ((CONFIG_USE_THREADS == 1) || (CONFIG_USE_SEMAPHORES == 1))
#include "hal_thread.h"
#endif

struct sCS101_Slave
{
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

    CS101_ResetCUHandler resetCUHandler;
    void* resetCUHandlerParameter;

    SerialTransceiverFT12 transceiver;

    LinkLayerSecondaryUnbalanced unbalancedLinkLayer;
    LinkLayerBalanced balancedLinkLayer;

    struct sLinkLayerParameters linkLayerParameters;

    struct sCS101_AppLayerParameters alParameters;

    struct sCS101_Queue userDataClass1Queue;

    struct sCS101_Queue userDataClass2Queue;

    struct sIMasterConnection iMasterConnection;

    IEC60870_LinkLayerMode linkLayerMode;

#if (CONFIG_USE_THREADS == 1)
    bool isRunning;
    Thread workerThread;
#endif

    LinkedList plugins;
};

static void
handleASDU(CS101_Slave self, CS101_ASDU asdu);

/********************************************
 * ISecondaryApplicationLayer
 ********************************************/

static bool
IsClass1DataAvailable(void* parameter)
{
    CS101_Slave self = (CS101_Slave)parameter;

    return (CS101_Queue_isEmpty(&(self->userDataClass1Queue)) == false);
}

static Frame
GetClass1Data(void* parameter, Frame frame)
{
    CS101_Slave self = (CS101_Slave)parameter;

    CS101_Queue_lock(&(self->userDataClass1Queue));

    Frame userData = CS101_Queue_dequeue(&(self->userDataClass1Queue), frame);

    CS101_Queue_unlock(&(self->userDataClass1Queue));

    return userData;
}

static Frame
GetClass2Data(void* parameter, Frame frame)
{
    CS101_Slave self = (CS101_Slave)parameter;

    CS101_Queue_lock(&(self->userDataClass2Queue));

    Frame userData = CS101_Queue_dequeue(&(self->userDataClass2Queue), frame);

    CS101_Queue_unlock(&(self->userDataClass2Queue));

    return userData;
}

static bool
HandleReceivedData(void* parameter, uint8_t* msg, bool isBroadcast, int userDataStart, int userDataLength)
{
    UNUSED_PARAMETER(isBroadcast);

    CS101_Slave self = (CS101_Slave)parameter;

    struct sCS101_ASDU _asdu;

    CS101_ASDU asdu = CS101_ASDU_createFromBufferEx(&_asdu, &(self->alParameters), msg + userDataStart, userDataLength);

    if (asdu)
    {
        handleASDU(self, asdu);
    }
    else
    {
        DEBUG_PRINT("CS101 slave: Failed to parse ASDU\n");
    }

    return true;
}

static void
ResetCUReceived(void* parameter, bool onlyFCB)
{
    CS101_Slave self = (CS101_Slave)parameter;

    if (onlyFCB)
    {
        DEBUG_PRINT("CS101 slave: Reset FCB received\n");
    }
    else
    {
        DEBUG_PRINT("CS101 slave: Reset CU received\n");

        if (self->resetCUHandler)
            self->resetCUHandler(self->resetCUHandlerParameter);
    }
}

static struct sISecondaryApplicationLayer cs101UnbalancedAppLayerInterface = {
    IsClass1DataAvailable, GetClass1Data, GetClass2Data, HandleReceivedData, ResetCUReceived};

/********************************************
 * END ISecondaryApplicationLayer
 ********************************************/

/********************************************
 * IBalancedApplicationLayer
 ********************************************/
static bool
IsClass2DataAvailable(void* parameter)
{
    CS101_Slave self = (CS101_Slave)parameter;

    return (CS101_Queue_isEmpty(&(self->userDataClass2Queue)) == false);
}

static Frame
IBalancedApplicationLayer_GetUserData(void* parameter, Frame frame)
{
    if (IsClass1DataAvailable(parameter))
        return GetClass1Data(parameter, frame);
    else if (IsClass2DataAvailable(parameter))
        return GetClass2Data(parameter, frame);
    else
        return NULL;
}

static bool
IBalancedApplicationLayer_HandleReceivedData(void* parameter, uint8_t* msg, bool isBroadcast, int userDataStart,
                                             int userDataLength)
{
    return HandleReceivedData(parameter, msg, isBroadcast, userDataStart, userDataLength);
}

static struct sIBalancedApplicationLayer cs101BalancedAppLayerInterface = {
    IBalancedApplicationLayer_GetUserData, IBalancedApplicationLayer_HandleReceivedData};

/********************************************
 * END IBalancedApplicationLayer
 ********************************************/

/********************************************
 * IMasterConnection
 *******************************************/

static bool
isReady(IMasterConnection self)
{
    CS101_Slave slave = (CS101_Slave)self->object;

    if (CS101_Slave_isClass1QueueFull(slave))
        return false;
    else
        return true;
}

static bool
sendASDU(IMasterConnection self, CS101_ASDU asdu)
{
    CS101_Slave slave = (CS101_Slave)self->object;

    CS101_Slave_enqueueUserDataClass1(slave, asdu);

    return true;
}

static bool
sendACT_CON(IMasterConnection self, CS101_ASDU asdu, bool negative)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
    CS101_ASDU_setNegative(asdu, negative);

    return sendASDU(self, asdu);
}

static bool
sendACT_TERM(IMasterConnection self, CS101_ASDU asdu)
{
    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_TERMINATION);
    CS101_ASDU_setNegative(asdu, false);

    return sendASDU(self, asdu);
}

static CS101_AppLayerParameters
getApplicationLayerParameters(IMasterConnection self)
{
    CS101_Slave slave = (CS101_Slave)self->object;

    return &(slave->alParameters);
}

/********************************************
 * END IMasterConnection
 *******************************************/

static struct sCS101_AppLayerParameters defaultAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249};

CS101_Slave
CS101_Slave_createEx(SerialPort serialPort, const LinkLayerParameters llParameters,
                     const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode,
                     int class1QueueSize, int class2QueueSize)
{
    CS101_Slave self = (CS101_Slave)GLOBAL_MALLOC(sizeof(struct sCS101_Slave));

    if (self)
    {
        self->asduHandler = NULL;
        self->interrogationHandler = NULL;
        self->counterInterrogationHandler = NULL;
        self->readHandler = NULL;
        self->clockSyncHandler = NULL;
        self->resetProcessHandler = NULL;
        self->delayAcquisitionHandler = NULL;
        self->resetCUHandler = NULL;

#if (CONFIG_USE_THREADS == 1)
        self->isRunning = false;
        self->workerThread = NULL;
#endif

        if (llParameters)
            self->linkLayerParameters = *llParameters;
        else
        {
            self->linkLayerParameters.addressLength = 1;
            self->linkLayerParameters.timeoutForAck = 200;
            self->linkLayerParameters.timeoutRepeat = 1000;
            self->linkLayerParameters.useSingleCharACK = true;
        }

        if (alParameters)
            self->alParameters = *alParameters;
        else
        {
            self->alParameters = defaultAppLayerParameters;
        }

        self->transceiver = SerialTransceiverFT12_create(serialPort, &(self->linkLayerParameters));

        self->linkLayerMode = linkLayerMode;

        if (linkLayerMode == IEC60870_LINK_LAYER_UNBALANCED)
        {
            self->balancedLinkLayer = NULL;

            self->unbalancedLinkLayer = LinkLayerSecondaryUnbalanced_create(
                0, self->transceiver, &(self->linkLayerParameters), &cs101UnbalancedAppLayerInterface, self);
        }
        else
        {
            self->unbalancedLinkLayer = NULL;

            self->balancedLinkLayer = LinkLayerBalanced_create(0, self->transceiver, &(self->linkLayerParameters),
                                                               &cs101BalancedAppLayerInterface, self);
        }

        self->iMasterConnection.isReady = isReady;
        self->iMasterConnection.sendASDU = sendASDU;
        self->iMasterConnection.sendACT_CON = sendACT_CON;
        self->iMasterConnection.sendACT_TERM = sendACT_TERM;
        self->iMasterConnection.getApplicationLayerParameters = getApplicationLayerParameters;
        self->iMasterConnection.close = NULL;
        self->iMasterConnection.getPeerAddress = NULL;
        self->iMasterConnection.object = self;

        CS101_Queue_initialize(&(self->userDataClass1Queue), class1QueueSize);
        CS101_Queue_initialize(&(self->userDataClass2Queue), class2QueueSize);

        self->plugins = NULL;
    }

    return self;
}

CS101_Slave
CS101_Slave_create(SerialPort serialPort, const LinkLayerParameters llParameters,
                   const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode)
{
    return CS101_Slave_createEx(serialPort, llParameters, alParameters, linkLayerMode, CS101_MAX_QUEUE_SIZE,
                                CS101_MAX_QUEUE_SIZE);
}

void
CS101_Slave_destroy(CS101_Slave self)
{
    if (self)
    {
        if (self->unbalancedLinkLayer)
            LinkLayerSecondaryUnbalanced_destroy(self->unbalancedLinkLayer);

        if (self->balancedLinkLayer)
            LinkLayerBalanced_destroy(self->balancedLinkLayer);

        SerialTransceiverFT12_destroy(self->transceiver);

        CS101_Queue_dispose(&(self->userDataClass1Queue));
        CS101_Queue_dispose(&(self->userDataClass2Queue));

        if (self->plugins)
        {
            LinkedList_destroyStatic(self->plugins);
        }

        GLOBAL_FREEMEM(self);
    }
}

void
CS101_Slave_addPlugin(CS101_Slave self, CS101_SlavePlugin plugin)
{
    if (self->plugins == NULL)
        self->plugins = LinkedList_create();

    if (self->plugins)
        LinkedList_add(self->plugins, plugin);
}

void
CS101_Slave_setDIR(CS101_Slave self, bool dir)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_BALANCED)
    {
        LinkLayerBalanced_setDIR(self->balancedLinkLayer, dir);
    }
}

void
CS101_Slave_setIdleTimeout(CS101_Slave self, int timeoutInMs)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_UNBALANCED)
        LinkLayerSecondaryUnbalanced_setIdleTimeout(self->unbalancedLinkLayer, timeoutInMs);
    else
        LinkLayerBalanced_setIdleTimeout(self->balancedLinkLayer, timeoutInMs);
}

void
CS101_Slave_setLinkLayerStateChanged(CS101_Slave self, IEC60870_LinkLayerStateChangedHandler handler, void* parameter)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_UNBALANCED)
    {
        LinkLayerSecondaryUnbalanced_setStateChangeHandler(self->unbalancedLinkLayer, handler, parameter);
    }
    else
    {
        LinkLayerBalanced_setStateChangeHandler(self->balancedLinkLayer, handler, parameter);
    }
}

void
CS101_Slave_setLinkLayerAddress(CS101_Slave self, int address)
{
    if (self->linkLayerMode == IEC60870_LINK_LAYER_UNBALANCED)
        LinkLayerSecondaryUnbalanced_setAddress(self->unbalancedLinkLayer, address);
    else
        LinkLayerBalanced_setAddress(self->balancedLinkLayer, address);
}

void
CS101_Slave_setLinkLayerAddressOtherStation(CS101_Slave self, int address)
{
    if (self->balancedLinkLayer)
        LinkLayerBalanced_setOtherStationAddress(self->balancedLinkLayer, address);
}

bool
CS101_Slave_isClass1QueueFull(CS101_Slave self)
{
    return CS101_Queue_isFull(&(self->userDataClass1Queue));
}

void
CS101_Slave_enqueueUserDataClass1(CS101_Slave self, CS101_ASDU asdu)
{
    CS101_Queue_enqueue(&(self->userDataClass1Queue), asdu);
}

bool
CS101_Slave_isClass2QueueFull(CS101_Slave self)
{
    return CS101_Queue_isFull(&(self->userDataClass2Queue));
}

void
CS101_Slave_enqueueUserDataClass2(CS101_Slave self, CS101_ASDU asdu)
{
    CS101_Queue_enqueue(&(self->userDataClass2Queue), asdu);
}

void
CS101_Slave_flushQueues(CS101_Slave self)
{
    CS101_Queue_flush(&(self->userDataClass1Queue));
    CS101_Queue_flush(&(self->userDataClass2Queue));
}

void
CS101_Slave_run(CS101_Slave self)
{
    if (self->unbalancedLinkLayer)
        LinkLayerSecondaryUnbalanced_run(self->unbalancedLinkLayer);
    else
        LinkLayerBalanced_run(self->balancedLinkLayer);

    /* call plugins */
    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);

        while (pluginElem)
        {
            CS101_SlavePlugin plugin = (CS101_SlavePlugin)LinkedList_getData(pluginElem);

            plugin->runTask(plugin->parameter, &(self->iMasterConnection));

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }
}

#if (CONFIG_USE_THREADS == 1)
static void*
slaveMainThread(void* parameter)
{
    CS101_Slave self = (CS101_Slave)parameter;

    self->isRunning = true;

    while (self->isRunning)
    {
        CS101_Slave_run(self);
    }

    return NULL;
}
#endif /* (CONFIG_USE_THREADS == 1) */

void
CS101_Slave_start(CS101_Slave self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->workerThread == NULL)
    {
        self->workerThread = Thread_create(slaveMainThread, self, false);
        Thread_start(self->workerThread);
    }
#endif /* (CONFIG_USE_THREADS == 1) */
}

void
CS101_Slave_stop(CS101_Slave self)
{
#if (CONFIG_USE_THREADS == 1)
    if (self->isRunning)
    {
        self->isRunning = false;
        Thread_destroy(self->workerThread);
        self->workerThread = NULL;
    }
#endif /* (CONFIG_USE_THREADS == 1) */
}

CS101_AppLayerParameters
CS101_Slave_getAppLayerParameters(CS101_Slave self)
{
    return &(self->alParameters);
}

LinkLayerParameters
CS101_Slave_getLinkLayerParameters(CS101_Slave self)
{
    return &(self->linkLayerParameters);
}

void
CS101_Slave_setResetCUHandler(CS101_Slave self, CS101_ResetCUHandler handler, void* parameter)
{
    self->resetCUHandler = handler;
    self->resetCUHandlerParameter = parameter;
}

void
CS101_Slave_setInterrogationHandler(CS101_Slave self, CS101_InterrogationHandler handler, void* parameter)
{
    self->interrogationHandler = handler;
    self->interrogationHandlerParameter = parameter;
}

void
CS101_Slave_setCounterInterrogationHandler(CS101_Slave self, CS101_CounterInterrogationHandler handler, void* parameter)
{
    self->counterInterrogationHandler = handler;
    self->counterInterrogationHandlerParameter = parameter;
}

void
CS101_Slave_setReadHandler(CS101_Slave self, CS101_ReadHandler handler, void* parameter)
{
    self->readHandler = handler;
    self->readHandlerParameter = parameter;
}

void
CS101_Slave_setASDUHandler(CS101_Slave self, CS101_ASDUHandler handler, void* parameter)
{
    self->asduHandler = handler;
    self->asduHandlerParameter = parameter;
}

void
CS101_Slave_setClockSyncHandler(CS101_Slave self, CS101_ClockSynchronizationHandler handler, void* parameter)
{
    self->clockSyncHandler = handler;
    self->clockSyncHandlerParameter = parameter;
}

void
CS101_Slave_setResetProcessHandler(CS101_Slave self, CS101_ResetProcessHandler handler, void* parameter)
{
    self->resetProcessHandler = handler;
    self->resetProcessHandlerParameter = parameter;
}

void
CS101_Slave_setDelayAcquisitionHandler(CS101_Slave self, CS101_DelayAcquisitionHandler handler, void* parameter)
{
    self->delayAcquisitionHandler = handler;
    self->delayAcquisitionHandlerParameter = parameter;
}

static void
responseCOTUnknown(CS101_ASDU asdu, IMasterConnection self)
{
    DEBUG_PRINT("  with unknown COT\n");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
    CS101_ASDU_setNegative(asdu, true);
    sendASDU(self, asdu);
}

/*
 * Handle received ASDUs
 *
 * Call the appropriate callbacks according to ASDU type and CoT
 */
static void
handleASDU(CS101_Slave self, CS101_ASDU asdu)
{
    bool messageHandled = false;

    /* call plugins */
    if (self->plugins)
    {
        LinkedList pluginElem = LinkedList_getNext(self->plugins);

        while (pluginElem)
        {
            CS101_SlavePlugin plugin = (CS101_SlavePlugin)LinkedList_getData(pluginElem);

            CS101_SlavePlugin_Result result = plugin->handleAsdu(plugin->parameter, &(self->iMasterConnection), asdu);

            if (result == CS101_PLUGIN_RESULT_HANDLED)
            {
                return;
            }
            else if (result == CS101_PLUGIN_RESULT_INVALID_ASDU)
            {
                DEBUG_PRINT("Invalid message");
            }

            pluginElem = LinkedList_getNext(pluginElem);
        }
    }

    uint8_t cot = CS101_ASDU_getCOT(asdu);

    switch (CS101_ASDU_getTypeID(asdu))
    {
    case C_IC_NA_1: /* 100 - interrogation command */

        DEBUG_PRINT("Rcvd interrogation command C_IC_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION))
        {
            if (self->interrogationHandler)
            {
                union uInformationObject _io;

                InterrogationCommand irc =
                    (InterrogationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (irc)
                {
                    if (self->interrogationHandler(self->interrogationHandlerParameter, &(self->iMasterConnection),
                                                   asdu, InterrogationCommand_getQOI(irc)))
                        messageHandled = true;
                }
                else
                {
                    DEBUG_PRINT("Invalid message");
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    case C_CI_NA_1: /* 101 - counter interrogation command */

        DEBUG_PRINT("Rcvd counter interrogation command C_CI_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_DEACTIVATION))
        {
            if (self->counterInterrogationHandler)
            {
                union uInformationObject _io;

                CounterInterrogationCommand cic =
                    (CounterInterrogationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (cic)
                {
                    if (self->counterInterrogationHandler(self->counterInterrogationHandlerParameter,
                                                          &(self->iMasterConnection), asdu,
                                                          CounterInterrogationCommand_getQCC(cic)))
                        messageHandled = true;
                }
                else
                {
                    DEBUG_PRINT("Invalid message");
                    return;
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    case C_RD_NA_1: /* 102 - read command */

        DEBUG_PRINT("Rcvd read command C_RD_NA_1\n");

        if (cot == CS101_COT_REQUEST)
        {
            if (self->readHandler)
            {
                union uInformationObject _io;

                ReadCommand rc = (ReadCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (rc)
                {
                    if (self->readHandler(self->readHandlerParameter, &(self->iMasterConnection), asdu,
                                          InformationObject_getObjectAddress((InformationObject)rc)))
                        messageHandled = true;
                }
                else
                {
                    DEBUG_PRINT("Invalid message");
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    case C_CS_NA_1: /* 103 - Clock synchronization command */

        DEBUG_PRINT("Rcvd clock sync command C_CS_NA_1\n");

        if (cot == CS101_COT_ACTIVATION)
        {
            if (self->clockSyncHandler)
            {
                union uInformationObject _io;

                ClockSynchronizationCommand csc =
                    (ClockSynchronizationCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (csc)
                {
                    if (self->clockSyncHandler(self->clockSyncHandlerParameter, &(self->iMasterConnection), asdu,
                                               ClockSynchronizationCommand_getTime(csc)))
                        messageHandled = true;

                    if (messageHandled)
                    {
                        /* send ACT-CON message */
                        CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);

                        CS101_Slave_enqueueUserDataClass1(self, asdu);
                    }
                }
                else
                {
                    DEBUG_PRINT("Invalid message");
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    case C_TS_NA_1: /* 104 - test command */

        DEBUG_PRINT("Rcvd test command C_TS_NA_1\n");

        {
            union uInformationObject _io;

            TestCommand tc = (TestCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

            if (tc)
            {
                /* Verify IOA = 0 */
                if (InformationObject_getObjectAddress((InformationObject)tc) != 0)
                {
                    DEBUG_PRINT("CS101 SLAVE: test command has invalid IOA - should be 0\n");
                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);
                    CS101_ASDU_setNegative(asdu, true);
                }
                else
                {
                    /* Only COT = ACTIVATION is allowed */
                    if (cot != CS101_COT_ACTIVATION)
                    {
                        DEBUG_PRINT("CS101 SLAVE: test command has invalid COT - should be ACTIVATION(6)\n");
                        CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
                        CS101_ASDU_setNegative(asdu, true);
                    }
                    else
                        CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                }

                CS101_Slave_enqueueUserDataClass1(self, asdu);
            }
            else
            {
                DEBUG_PRINT("CS101 SLAVE: invalid test command\n");
            }

            messageHandled = true;
        }

        break;

    case C_RP_NA_1: /* 105 - Reset process command */

        DEBUG_PRINT("Rcvd reset process command C_RP_NA_1\n");

        if (cot == CS101_COT_ACTIVATION)
        {
            if (self->resetProcessHandler)
            {
                union uInformationObject _io;

                ResetProcessCommand rpc =
                    (ResetProcessCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (rpc)
                {
                    if (self->resetProcessHandler(self->resetProcessHandlerParameter, &(self->iMasterConnection), asdu,
                                                  ResetProcessCommand_getQRP(rpc)))
                        messageHandled = true;
                }
                else
                {
                    DEBUG_PRINT("Invalid reset-process-command message");
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    case C_CD_NA_1: /* 106 - Delay acquisition command */

        DEBUG_PRINT("Rcvd delay acquisition command C_CD_NA_1\n");

        if ((cot == CS101_COT_ACTIVATION) || (cot == CS101_COT_SPONTANEOUS))
        {
            if (self->delayAcquisitionHandler)
            {
                union uInformationObject _io;

                DelayAcquisitionCommand dac =
                    (DelayAcquisitionCommand)CS101_ASDU_getElementEx(asdu, (InformationObject)&_io, 0);

                if (dac)
                {
                    if (self->delayAcquisitionHandler(self->delayAcquisitionHandlerParameter,
                                                      &(self->iMasterConnection), asdu,
                                                      DelayAcquisitionCommand_getDelay(dac)))
                        messageHandled = true;
                }
                else
                {
                    DEBUG_PRINT("Invalid message");
                }
            }
        }
        else
            responseCOTUnknown(asdu, &(self->iMasterConnection));

        break;

    default: /* no special handler available -> use default handler */
        break;
    }

    if ((messageHandled == false) && self->asduHandler)
        if (self->asduHandler(self->asduHandlerParameter, &(self->iMasterConnection), asdu))
            messageHandled = true;

    if (messageHandled == false)
    {
        /* send error response */
        CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
        CS101_ASDU_setNegative(asdu, true);
        CS101_Slave_enqueueUserDataClass1(self, asdu);
    }
}

void
CS101_Slave_setRawMessageHandler(CS101_Slave self, IEC60870_RawMessageHandler handler, void* parameter)
{
    SerialTransceiverFT12_setRawMessageHandler(self->transceiver, handler, parameter);
}
