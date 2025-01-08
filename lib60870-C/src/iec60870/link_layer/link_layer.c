/*
 *  link_layer.c
 *
 *  Copyright 2017-2024 Michael Zillgith
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
#include <string.h>
#include "link_layer.h"
#include "buffer_frame.h"
#include "frame.h"
#include "hal_time.h"
#include "lib60870_internal.h"
#include "lib_memory.h"
#include "link_layer_private.h"
#include "linked_list.h"
#include "serial_transceiver_ft_1_2.h"

typedef struct sLinkLayerSecondaryUnbalanced* LL_Sec_Unb; /* short cut definition */

typedef struct sLinkLayerSecondaryBalanced* LinkLayerSecondaryBalanced;
typedef struct sLinkLayerSecondaryBalanced* LL_Sec_Bal; /* short cut definition */

typedef struct sLinkLayerPrimaryBalanced* LinkLayerPrimaryBalanced;

void
LinkLayerSecondaryBalanced_handleMessage(LinkLayerSecondaryBalanced self, uint8_t fc, bool isBroadcast, bool fcb,
                                         bool fcv, uint8_t* msg, int userDataStart, int userDataLength);

void
LinkLayerPrimaryBalanced_handleMessage(LinkLayerPrimaryBalanced self, uint8_t fc, bool dir, bool dfc, int address,
                                       uint8_t* msg, int userDataStart, int userDataLength);

void
LinkLayerPrimaryBalanced_setStateChangeHandler(LinkLayerPrimaryBalanced self,
                                               IEC60870_LinkLayerStateChangedHandler handler, void* parameter);

void
LinkLayerPrimaryBalanced_runStateMachine(LinkLayerPrimaryBalanced self);

void
LinkLayerPrimaryBalanced_resetIdleTimeout(LinkLayerPrimaryBalanced self);

void
LinkLayerPrimaryUnbalanced_handleMessage(LinkLayerPrimaryUnbalanced self, uint8_t fc, bool acd, bool dfc, int address,
                                         uint8_t* msg, int userDataStart, int userDataLength);

static void
llsu_setState(LL_Sec_Unb self, LinkLayerState newState);

void
LinkLayerPrimaryUnbalanced_runStateMachine(LinkLayerPrimaryUnbalanced self);

struct sLinkLayer
{
    uint8_t buffer[261]; /* 261 = maximum FT1.2 frame length */
    uint8_t userDataBuffer[255];
    uint8_t userDataSize; /* > 0 when last sent message is available */
    int address;
    SerialTransceiverFT12 transceiver;
    LinkLayerParameters linkLayerParameters;

    bool dir; /* DIR bit to be used when messages are sent in balanced mode */

    LinkLayerSecondaryUnbalanced llSecUnbalanced;
    LinkLayerSecondaryBalanced llSecBalanced;

    LinkLayerPrimaryBalanced llPriBalanced;
    LinkLayerPrimaryUnbalanced llPriUnbalanced;
};

struct sLinkLayerSecondaryUnbalanced
{
    /**
     * Initialized/timeout -> state = IDLE
     * received invalid message -> state = ERROR
     * received valid message/no timeout -> state = AVAILABLE
     */
    LinkLayerState state;

    bool expectedFcb; /* expected value of next frame count bit (FCB) */
    LinkLayer linkLayer;
    ISecondaryApplicationLayer applicationLayer;
    void* appLayerParam;
    LinkLayerParameters linkLayerParameters;

    uint64_t lastReceivedMsg;
    int idleTimeout; /* connection timeout in ms */

    IEC60870_LinkLayerStateChangedHandler stateChangedHandler;
    void* stateChangedHandlerParameter;

    struct sLinkLayer _linkLayer;
};

LinkLayer
LinkLayer_init(LinkLayer self, int address, SerialTransceiverFT12 transceiver, LinkLayerParameters linkLayerParameters)
{
    if (self == NULL)
        self = (LinkLayer)GLOBAL_MALLOC(sizeof(struct sLinkLayer));

    if (self != NULL)
    {
        self->address = address;
        self->transceiver = transceiver;
        self->linkLayerParameters = linkLayerParameters;

        self->dir = false;

        self->llSecUnbalanced = NULL;
        self->llSecBalanced = NULL;
        self->llPriBalanced = NULL;
        self->llPriUnbalanced = NULL;
    }

    return self;
}

void
LinkLayer_setDIR(LinkLayer self, bool dir)
{
    self->dir = dir;
}

void
LinkLayer_setAddress(LinkLayer self, int address)
{
    self->address = address;
}

static int
LinkLayer_getBroadcastAddress(LinkLayer self)
{
    if (self->linkLayerParameters->addressLength == 1)
    {
        return 255;
    }
    else if (self->linkLayerParameters->addressLength == 2)
    {
        return 65535;
    }

    return 0;
}

LinkLayerSecondaryUnbalanced
LinkLayerSecondaryUnbalanced_create(int linkLayerAddress, SerialTransceiverFT12 transceiver,
                                    LinkLayerParameters linkLayerParameters,
                                    ISecondaryApplicationLayer applicationLayer, void* applicationLayerParameter)
{
    LL_Sec_Unb self = (LL_Sec_Unb)GLOBAL_MALLOC(sizeof(struct sLinkLayerSecondaryUnbalanced));

    if (self != NULL)
    {
        self->expectedFcb = true;
        self->applicationLayer = applicationLayer;
        self->appLayerParam = applicationLayerParameter;
        self->linkLayerParameters = linkLayerParameters;
        self->linkLayer = &(self->_linkLayer);

        self->state = LL_STATE_IDLE;

        self->idleTimeout = 500;
        self->stateChangedHandler = NULL;

        self->lastReceivedMsg = 0;

        LinkLayer_init(self->linkLayer, linkLayerAddress, transceiver, self->linkLayerParameters);

        self->linkLayer->llSecUnbalanced = self;
    }

    return self;
}

void
LinkLayerSecondaryUnbalanced_destroy(LL_Sec_Unb self)
{
    if (self != NULL)
        GLOBAL_FREEMEM(self);
}

void
LinkLayerSecondaryUnbalanced_setStateChangeHandler(LinkLayerSecondaryUnbalanced self,
                                                   IEC60870_LinkLayerStateChangedHandler handler, void* parameter)
{
    self->stateChangedHandler = handler;
    self->stateChangedHandlerParameter = parameter;
}

static void
SendSingleCharCharacter(LinkLayer self)
{
    uint8_t singleCharAck[] = {0xe5};

    SerialTransceiverFT12_sendMessage(self->transceiver, singleCharAck, 1);
}

static void
SendFixedFrame(LinkLayer self, uint8_t fc, int address, bool prm, bool dir, bool acd /*FCB*/, bool dfc /*FCV*/)
{
    uint8_t* buffer = self->buffer;

    int bufPos = 0;

    buffer[bufPos++] = 0x10; /* START */

    uint8_t c = fc & 0x0f;

    if (prm)
        c += 0x40;

    if (dir)
        c += 0x80;

    if (acd)
        c += 0x20;

    if (dfc)
        c += 0x10;

    buffer[bufPos++] = c;

    if (self->linkLayerParameters->addressLength > 0)
    {
        buffer[bufPos++] = (uint8_t)(address % 0x100);

        if (self->linkLayerParameters->addressLength > 1)
            buffer[bufPos++] = (uint8_t)((address / 0x100) % 0x100);
    }

    uint8_t checksum = 0;

    int i;

    for (i = 1; i < bufPos; i++)
        checksum += buffer[i];

    buffer[bufPos++] = checksum;

    buffer[bufPos++] = 0x16; /* END */

    DEBUG_PRINT("Send fixed frame (fc=%i)\n", fc);

    SerialTransceiverFT12_sendMessage(self->transceiver, buffer, bufPos);
}

static void
SendVariableLengthFrame(LinkLayer self, uint8_t fc, int address, bool prm, bool dir, bool acd, bool dfc, Frame frame)
{
    uint8_t* buffer = self->buffer;
    int addressLength = self->linkLayerParameters->addressLength;

    buffer[0] = 0x68; /* START */
    buffer[3] = 0x68; /* START */

    uint8_t c = fc & 0x0f;

    if (prm)
        c += 0x40;

    if (dir)
        c += 0x80;

    if (acd)
        c += 0x20;

    if (dfc)
        c += 0x10;

    buffer[4] = c;

    int bufPos = 5;

    if (addressLength > 0)
    {
        buffer[bufPos++] = (uint8_t)(address % 0x100);

        if (addressLength > 1)
            buffer[bufPos++] = (uint8_t)((address / 0x100) % 0x100);
    }

    uint8_t* userData = Frame_getBuffer(frame);
    int userDataLength = Frame_getMsgSize(frame);

    int l = 1 + addressLength + userDataLength;

    if (l > 255)
        return;

    buffer[1] = (uint8_t)l;
    buffer[2] = (uint8_t)l;

    int i;

    for (i = 0; i < userDataLength; i++)
        buffer[bufPos++] = userData[i];

    uint8_t checksum = 0;

    for (i = 4; i < bufPos; i++)
        checksum += buffer[i];

    buffer[bufPos++] = checksum;

    buffer[bufPos++] = 0x16; /* END */

    DEBUG_PRINT("Send variable frame (fc=%i, size=%i)\n", (int)fc, bufPos);

    SerialTransceiverFT12_sendMessage(self->transceiver, buffer, bufPos);
}

static bool
checkFCB(LL_Sec_Unb self, bool fcb)
{
    if (fcb != self->expectedFcb)
        return false;
    else
    {
        self->expectedFcb = !(self->expectedFcb);
        return true;
    }
}

static void
LinkLayerSecondaryUnbalanced_handleMessage(LL_Sec_Unb self, uint8_t fc, bool isBroadcast, bool fcb, bool fcv,
                                           uint8_t* msg, int userDataStart, int userDataLength)
{
    llsu_setState(self, LL_STATE_AVAILABLE);

    switch (fc)
    {

    case LL_FC_09_REQUEST_LINK_STATUS:
        DEBUG_PRINT("SLL - REQUEST LINK STATUS\n");
        {
            /* check that FCV=0 */
            if (fcv != 0)
            {
                DEBUG_PRINT("SLL - REQUEST LINK STATUS failed - invalid FCV\n");
                llsu_setState(self, LL_STATE_ERROR);
                return;
            }

            bool accessDemand = self->applicationLayer->IsClass1DataAvailable(self->appLayerParam);

            SendFixedFrame(self->linkLayer, LL_FC_11_STATUS_OF_LINK_OR_ACCESS_DEMAND, self->linkLayer->address, false,
                           false, accessDemand, false);
        }
        break;

    case LL_FC_00_RESET_REMOTE_LINK:
        DEBUG_PRINT("SLL - RESET REMOTE LINK\n");
        {
            /* check that FCB=0 and FCV=0 */
            if ((fcv != 0) || (fcb != 0))
            {
                DEBUG_PRINT("SLL - RESET REMOTE LINK failed - invalid FCV/FCB\n");
                llsu_setState(self, LL_STATE_ERROR);
                return;
            }

            self->expectedFcb = true;

            if (self->linkLayerParameters->useSingleCharACK)
                SendSingleCharCharacter(self->linkLayer);
            else
                SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, false, false, false);

            self->applicationLayer->ResetCUReceived(self->appLayerParam, false);
        }
        break;

    case LL_FC_07_RESET_FCB:
        DEBUG_PRINT("SLL - RESET FCB\n");
        {
            /* used by CS103 */

            /* check that FCB=0 and FCV=0 */
            if ((fcv != 0) || (fcb != 0))
            {
                DEBUG_PRINT("SLL - RESET FCB failed - invalid FCV/FCB\n");
                llsu_setState(self, LL_STATE_ERROR);
                return;
            }

            self->expectedFcb = true;

            if (self->linkLayerParameters->useSingleCharACK)
                SendSingleCharCharacter(self->linkLayer);
            else
                SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, false, false, false);

            self->applicationLayer->ResetCUReceived(self->appLayerParam, true);
        }
        break;

    case LL_FC_11_REQUEST_USER_DATA_CLASS_2:
        DEBUG_PRINT("SLL - REQUEST USER DATA CLASS 2\n");
        {
            bool invalidFCB = false;

            if (fcv)
            {
                if (checkFCB(self, fcb) == false)
                {
                    DEBUG_PRINT("SLL - REQ UD2 - unexpected FCB\n");
                    invalidFCB = true;
                }
            }

            /* provide a buffer where the application layer can encode the user data */
            struct sBufferFrame _bufferFrame;
            Frame bufferFrame = NULL;
            Frame asdu = NULL;

            if (invalidFCB)
            {
                if (self->_linkLayer.userDataSize > 0)
                {
                    bufferFrame = BufferFrame_initialize(&_bufferFrame, self->_linkLayer.userDataBuffer,
                                                         self->_linkLayer.userDataSize);

                    DEBUG_PRINT("SLL - REQ UD2 - send old message\n");

                    asdu = bufferFrame;
                }
            }
            else
            {
                bufferFrame = BufferFrame_initialize(&_bufferFrame, self->_linkLayer.userDataBuffer, 0);

                asdu = self->applicationLayer->GetClass2Data(self->appLayerParam, bufferFrame);

                if (asdu != NULL)
                {
                    self->_linkLayer.userDataSize = Frame_getMsgSize(asdu);
                }
                else
                {
                    self->_linkLayer.userDataSize = 0;
                }
            }

            bool accessDemand = self->applicationLayer->IsClass1DataAvailable(self->appLayerParam);

            if (asdu != NULL)
            {
                SendVariableLengthFrame(self->linkLayer, LL_FC_08_RESP_USER_DATA, self->linkLayer->address, false,
                                        false, accessDemand, false, asdu);

                /* release frame buffer if required */
                if (asdu != bufferFrame)
                    Frame_destroy(asdu);
            }
            else
            {
                if (self->linkLayerParameters->useSingleCharACK && !accessDemand)
                    SendSingleCharCharacter(self->linkLayer);
                else
                    SendFixedFrame(self->linkLayer, LL_FC_09_RESP_NACK_NO_DATA, self->linkLayer->address, false, false,
                                   accessDemand, false);
            }
        }
        break;

    case LL_FC_10_REQUEST_USER_DATA_CLASS_1:
        DEBUG_PRINT("SLL - REQUEST USER DATA CLASS 1\n");
        {
            bool invalidFCB = false;

            if (fcv)
            {
                if (checkFCB(self, fcb) == false)
                {
                    DEBUG_PRINT("SLL - REQ UD1 - unexpected FCB\n");
                    invalidFCB = true;
                }
            }

            /* provide a buffer where the application layer can encode the user data */
            struct sBufferFrame _bufferFrame;
            Frame bufferFrame = NULL;
            Frame asdu = NULL;

            if (invalidFCB)
            {
                if (self->_linkLayer.userDataSize > 0)
                {
                    bufferFrame = BufferFrame_initialize(&_bufferFrame, self->_linkLayer.userDataBuffer,
                                                         self->_linkLayer.userDataSize);

                    DEBUG_PRINT("SLL - REQ UD1 - send old message\n");

                    asdu = bufferFrame;
                }
            }
            else
            {
                bufferFrame = BufferFrame_initialize(&_bufferFrame, self->_linkLayer.userDataBuffer, 0);

                asdu = self->applicationLayer->GetClass1Data(self->appLayerParam, bufferFrame);

                if (asdu != NULL)
                {
                    self->_linkLayer.userDataSize = Frame_getMsgSize(asdu);
                }
                else
                {
                    self->_linkLayer.userDataSize = 0;
                }
            }

            bool accessDemand = self->applicationLayer->IsClass1DataAvailable(self->appLayerParam);

            if (asdu != NULL)
            {
                SendVariableLengthFrame(self->linkLayer, LL_FC_08_RESP_USER_DATA, self->linkLayer->address, false,
                                        false, accessDemand, false, asdu);

                /* release frame buffer if required */
                if (asdu != bufferFrame)
                    Frame_destroy(asdu);
            }
            else
            {
                if (self->linkLayerParameters->useSingleCharACK && !accessDemand)
                    SendSingleCharCharacter(self->linkLayer);
                else
                    SendFixedFrame(self->linkLayer, LL_FC_09_RESP_NACK_NO_DATA, self->linkLayer->address, false, false,
                                   accessDemand, false);
            }
        }
        break;

    case LL_FC_03_USER_DATA_CONFIRMED:
        DEBUG_PRINT("SLL - USER DATA CONFIRMED\n");
        {
            bool indicateUserData = true;

            if (fcv)
            {
                if (checkFCB(self, fcb) == false)
                {
                    DEBUG_PRINT("SLL - FCB check failed -> ignore UD confirmed\n");
                    indicateUserData = false;
                }
            }

            if ((indicateUserData == true) && (userDataLength > 0))
            {
                self->applicationLayer->HandleReceivedData(self->appLayerParam, msg, isBroadcast, userDataStart,
                                                           userDataLength);
            }

            bool accessDemand = self->applicationLayer->IsClass1DataAvailable(self->appLayerParam);

            if (self->linkLayerParameters->useSingleCharACK && !accessDemand)
                SendSingleCharCharacter(self->linkLayer);
            else
                SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, false, accessDemand,
                               false);
        }
        break;

    case LL_FC_04_USER_DATA_NO_REPLY:
        DEBUG_PRINT("SLL - USER DATA NO REPLY\n");
        {
            /* check that FCV=0 */
            if (fcv != 0)
            {
                DEBUG_PRINT("SLL - USER DATA NO REPLY - invalid FCV\n");
                llsu_setState(self, LL_STATE_ERROR);
                return;
            }

            if (userDataLength > 0)
            {
                self->applicationLayer->HandleReceivedData(self->appLayerParam, msg, isBroadcast, userDataStart,
                                                           userDataLength);
            }
        }
        break;

    default:
        DEBUG_PRINT("SLL - UNEXPECTED LINK LAYER MESSAGE\n");

        SendFixedFrame(self->linkLayer, LL_FC_15_SERVICE_NOT_IMPLEMENTED, self->linkLayer->address, false, false, false,
                       false);

        break;
    }
}

static void
llsu_setState(LL_Sec_Unb self, LinkLayerState newState)
{
    if (self->state != newState)
    {
        self->state = newState;

        if (self->stateChangedHandler)
            self->stateChangedHandler(self->stateChangedHandlerParameter, -1, newState);
    }
}

static void
ParserHeaderSecondaryUnbalanced(void* parameter, uint8_t* msg, int msgSize)
{
    LL_Sec_Unb self = (LL_Sec_Unb)parameter;

    self->lastReceivedMsg = Hal_getMonotonicTimeInMs();

    int userDataLength = 0;
    int userDataStart = 0;
    uint8_t c;
    int csStart;
    int csIndex;
    int address = 0;

    int addressLength = self->linkLayer->linkLayerParameters->addressLength;

    if (msg[0] == 0x68)
    {
        if (msg[1] != msg[2])
        {
            DEBUG_PRINT("ERROR: L fields differ!\n");
            llsu_setState(self, LL_STATE_ERROR);
            return;
        }

        userDataLength = (int)msg[1] - addressLength - 1;
        userDataStart = 5 + addressLength;

        csStart = 4;
        csIndex = userDataStart + userDataLength;

        /* check if message size is reasonable */
        if (msgSize != (userDataStart + userDataLength + 2 /* CS + END */))
        {
            DEBUG_PRINT("ERROR: Invalid message length\n");
            llsu_setState(self, LL_STATE_ERROR);
            return;
        }

        c = msg[4];
    }
    else if (msg[0] == 0x10)
    {
        c = msg[1];
        csStart = 1;
        csIndex = 2 + addressLength;
    }
    else
    {
        DEBUG_PRINT("ERROR: Received unexpected message type in unbalanced slave mode!\n");
        llsu_setState(self, LL_STATE_ERROR);
        return;
    }

    bool isBroadcast = false;

    /* check address */
    if (addressLength > 0)
    {
        address = msg[csStart + 1];

        if (addressLength > 1)
        {
            address += msg[csStart + 2] * 0x100;

            if (address == 65535)
                isBroadcast = true;
        }
        else
        {
            if (address == 255)
                isBroadcast = true;
        }
    }

    int fc = c & 0x0f;

    if (isBroadcast)
    {
        if (fc != LL_FC_04_USER_DATA_NO_REPLY)
        {
            DEBUG_PRINT("ERROR: Invalid function code for broadcast message!\n");
            llsu_setState(self, LL_STATE_ERROR);
            return;
        }
    }
    else
    {
        if (address != self->linkLayer->address)
        {
            DEBUG_PRINT("INFO: unknown link layer address -> ignore message\n");
            return;
        }
    }

    /* check checksum */
    uint8_t checksum = 0;

    int i;

    for (i = csStart; i < csIndex; i++)
        checksum += msg[i];

    if (checksum != msg[csIndex])
    {
        DEBUG_PRINT("ERROR: checksum invalid!\n");
        llsu_setState(self, LL_STATE_ERROR);
        return;
    }

    /* parse C field bits */
    bool prm = ((c & 0x40) == 0x40);

    if (prm == false)
    {
        DEBUG_PRINT("ERROR: Received secondary message in unbalanced slave mode!\n");
        llsu_setState(self, LL_STATE_ERROR);
        return;
    }

    bool fcb = ((c & 0x20) == 0x20);
    bool fcv = ((c & 0x10) == 0x10);

    LinkLayerSecondaryUnbalanced_handleMessage(self, fc, isBroadcast, fcb, fcv, msg, userDataStart, userDataLength);
}

static void
HandleMessageBalancedAndPrimaryUnbalanced(void* parameter, uint8_t* msg, int msgSize)
{
    LinkLayer self = (LinkLayer)parameter;

    int userDataLength = 0;
    int userDataStart = 0;
    uint8_t c = 0;
    int csStart = 0;
    int csIndex = 0;
    int address = 0; /* address can be ignored in balanced mode? */

    bool isSingleCharAck = false;

    if (msg[0] == 0x68)
    {
        if (msg[1] != msg[2])
        {
            DEBUG_PRINT("ERROR: L fields differ!\n");
            return;
        }

        userDataLength = (int)msg[1] - self->linkLayerParameters->addressLength - 1;
        userDataStart = 5 + self->linkLayerParameters->addressLength;

        csStart = 4;
        csIndex = userDataStart + userDataLength;

        /* check if message size is reasonable */
        if (msgSize != (userDataStart + userDataLength + 2 /* CS + END */))
        {
            DEBUG_PRINT("ERROR: Invalid message length\n");
            return;
        }

        c = msg[4];

        if (self->linkLayerParameters->addressLength > 0)
            address += msg[5];

        if (self->linkLayerParameters->addressLength > 1)
            address += msg[6] * 0x100;
    }
    else if (msg[0] == 0x10)
    {
        c = msg[1];
        csStart = 1;
        csIndex = 2 + self->linkLayerParameters->addressLength;

        if (self->linkLayerParameters->addressLength > 0)
            address += msg[2];

        if (self->linkLayerParameters->addressLength > 1)
            address += msg[3] * 0x100;
    }
    else if (msg[0] == 0xe5)
    {
        isSingleCharAck = true;
        DEBUG_PRINT("Received single char ACK\n");
    }
    else
    {
        DEBUG_PRINT("ERROR: Received unexpected message type!\n");
        return;
    }

    if (isSingleCharAck == false)
    {
        /* check checksum */
        uint8_t checksum = 0;

        int i;

        for (i = csStart; i < csIndex; i++)
            checksum += msg[i];

        if (checksum != msg[csIndex])
        {
            DEBUG_PRINT("ERROR: checksum invalid!\n");
            return;
        }

        /* parse C field bits */
        uint8_t fc = c & 0x0f;
        bool prm = ((c & 0x40) == 0x40);

        if (prm)
        { /* we are secondary link layer */
            bool fcb = ((c & 0x20) == 0x20);
            bool fcv = ((c & 0x10) == 0x10);

            if (self->llSecBalanced != NULL)
                LinkLayerSecondaryBalanced_handleMessage(self->llSecBalanced, fc, false, fcb, fcv, msg, userDataStart,
                                                         userDataLength);
            else
                DEBUG_PRINT("No secondary link layer available!\n");

            if (self->llPriBalanced != NULL)
            {
                LinkLayerPrimaryBalanced_resetIdleTimeout(self->llPriBalanced);
            }
        }
        else
        {                                    /* we are primary link layer */
            bool dir = ((c & 0x80) == 0x80); /* DIR - direction for balanced transmission */
            bool dfc = ((c & 0x10) == 0x10); /* DFC - Data flow control */

            if (self->llPriBalanced != NULL)
            {
                LinkLayerPrimaryBalanced_handleMessage(self->llPriBalanced, fc, dir, dfc, address, msg, userDataStart,
                                                       userDataLength);
            }
            else if (self->llPriUnbalanced != NULL)
            {
                bool acd =
                    ((c & 0x20) == 0x20); /* ACD - access demand for class 1 data - for unbalanced transmission */

                LinkLayerPrimaryUnbalanced_handleMessage(self->llPriUnbalanced, fc, acd, dfc, address, msg,
                                                         userDataStart, userDataLength);
            }
            else
                DEBUG_PRINT("No primary link layer available!\n");
        }
    }
    else
    {
        /* Single byte ACK */
        if (self->llPriBalanced != NULL)
        {
            LinkLayerPrimaryBalanced_handleMessage(self->llPriBalanced, LL_FC_00_ACK, false, false, -1, NULL, 0, 0);
        }
        else if (self->llPriUnbalanced != NULL)
        {
            LinkLayerPrimaryUnbalanced_handleMessage(self->llPriUnbalanced, LL_FC_00_ACK, false, false, -1, NULL, 0, 0);
        }
        else
            DEBUG_PRINT("No primary link layer available!\n");
    }
}

void
LinkLayerSecondaryUnbalanced_setIdleTimeout(LinkLayerSecondaryUnbalanced self, int timeoutInMs)
{
    self->idleTimeout = timeoutInMs;
}

void
LinkLayerSecondaryUnbalanced_setAddress(LinkLayerSecondaryUnbalanced self, int address)
{
    self->_linkLayer.address = address;
}

void
LinkLayerSecondaryUnbalanced_run(LinkLayerSecondaryUnbalanced self)
{
    LinkLayer ll = self->linkLayer;

    SerialTransceiverFT12_readNextMessage(ll->transceiver, ll->buffer, ParserHeaderSecondaryUnbalanced, self);

    if (self->state != LL_STATE_IDLE)
    {
        if ((Hal_getMonotonicTimeInMs() - self->lastReceivedMsg) > (unsigned int)self->idleTimeout)
            llsu_setState(self, LL_STATE_IDLE);
    }
}

struct sLinkLayerSecondaryBalanced
{
    bool expectedFcb; /* expected value of next frame count bit (FCB) */
    LinkLayer linkLayer;
    IBalancedApplicationLayer applicationLayer;
    void* appLayerParam;
};

static void
LinkLayerSecondaryBalanced_init(LinkLayerSecondaryBalanced self, LinkLayer linkLayer,
                                IBalancedApplicationLayer applicationLayer, void* appLayerParam)
{
    self->expectedFcb = true;
    self->linkLayer = linkLayer;
    self->applicationLayer = applicationLayer;
    self->appLayerParam = appLayerParam;
}

static bool
LinkLayerSecondaryBalanced_checkFCB(LinkLayerSecondaryBalanced self, bool fcb)
{
    if (fcb != self->expectedFcb)
    {
        DEBUG_PRINT("ERROR: Frame count bit (FCB) invalid!\n");
        /* TODO change link status */
        return false;
    }
    else
    {
        self->expectedFcb = !(self->expectedFcb);
        return true;
    }
}

void
LinkLayerSecondaryBalanced_handleMessage(LinkLayerSecondaryBalanced self, uint8_t fc, bool isBroadcast, bool fcb,
                                         bool fcv, uint8_t* msg, int userDataStart, int userDataLength)
{
    if (fcv)
    {
        if (LinkLayerSecondaryBalanced_checkFCB(self, fcb) == false)
            return;
    }

    switch (fc)
    {

    case LL_FC_00_RESET_REMOTE_LINK:

        DEBUG_PRINT("SLL - RECV FC 00 - RESET REMOTE LINK\n");

        self->expectedFcb = true;

        DEBUG_PRINT("SLL - SEND FC 00 - ACK\n");

        if (self->linkLayer->linkLayerParameters->useSingleCharACK)
            SendSingleCharCharacter(self->linkLayer);
        else
            SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, self->linkLayer->dir, false,
                           false);

        break;

    case LL_FC_02_TEST_FUNCTION_FOR_LINK:

        DEBUG_PRINT("SLL - RECV FC 02 - TEST FUNCTION FOR LINK\n");

        DEBUG_PRINT("SLL - SEND FC 00 - ACK\n");

        if (self->linkLayer->linkLayerParameters->useSingleCharACK)
            SendSingleCharCharacter(self->linkLayer);
        else
            SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, self->linkLayer->dir, false,
                           false);

        break;

    case LL_FC_03_USER_DATA_CONFIRMED:

        DEBUG_PRINT("SLL - RECV FC 03 - USER DATA CONFIRMED\n");

        if (userDataLength > 0)
        {
            if (self->applicationLayer->HandleReceivedData(self->appLayerParam, msg, isBroadcast, userDataStart,
                                                           userDataLength))
            {
                DEBUG_PRINT("SLL - SEND FC 00 - ACK\n");

                if (self->linkLayer->linkLayerParameters->useSingleCharACK)
                    SendSingleCharCharacter(self->linkLayer);
                else
                    SendFixedFrame(self->linkLayer, LL_FC_00_ACK, self->linkLayer->address, false, self->linkLayer->dir,
                                   false, false);
            }
        }

        break;

    case LL_FC_04_USER_DATA_NO_REPLY:

        DEBUG_PRINT("SLL -FC 04 - USER DATA NO REPLY\n");

        if (userDataLength > 0)
        {
            self->applicationLayer->HandleReceivedData(self->appLayerParam, msg, isBroadcast, userDataStart,
                                                       userDataLength);
        }

        break;

    case LL_FC_09_REQUEST_LINK_STATUS:

        DEBUG_PRINT("SLL - RECV FC 09 - REQUEST LINK STATUS\n");

        DEBUG_PRINT("SLL - SEND FC 11 - STATUS OF LINK\n");

        SendFixedFrame(self->linkLayer, LL_FC_11_STATUS_OF_LINK_OR_ACCESS_DEMAND, self->linkLayer->address, false,
                       self->linkLayer->dir, false, false);

        break;

    default:
        DEBUG_PRINT("SLL - UNEXPECTED LINK LAYER MESSAGE");

        DEBUG_PRINT("SLL - SEND FC 15 - SERVICE NOT IMPLEMENTED\n");

        SendFixedFrame(self->linkLayer, LL_FC_15_SERVICE_NOT_IMPLEMENTED, self->linkLayer->address, false,
                       self->linkLayer->dir, false, false);

        break;
    }
}

/**********************************************
 * Primary link-layer balanced
 **********************************************/

typedef enum
{
    PLL_IDLE,
    PLL_EXECUTE_REQUEST_STATUS_OF_LINK,
    PLL_EXECUTE_RESET_REMOTE_LINK,
    PLL_LINK_LAYERS_AVAILABLE,
    PLL_EXECUTE_SERVICE_SEND_CONFIRM,
    PLL_EXECUTE_SERVICE_REQUEST_RESPOND,
    PLL_SECONDARY_LINK_LAYER_BUSY, /* Only required in balanced link layer */
    PLL_TIMEOUT                    /* only required in unbalanced link layer */
} PrimaryLinkLayerState;

struct sLinkLayerPrimaryBalanced
{
    LinkLayerState state; /* state information for user */

    PrimaryLinkLayerState primaryState; /* internal PLL state machine state */

    bool waitingForResponse;
    uint64_t lastSendTime;
    uint64_t originalSendTime;
    bool sendLinkLayerTestFunction;
    bool nextFcb;

    int otherStationAddress;

    struct sBufferFrame lastSendAsdu;

    IBalancedApplicationLayer applicationLayer;
    void* applicationLayerParam;

    uint64_t lastReceivedMsg;
    int idleTimeout; /* connection timeout in ms */

    IEC60870_LinkLayerStateChangedHandler stateChangedHandler;
    void* stateChangedHandlerParameter;

    LinkLayer linkLayer;
};

static void
LinkLayerPrimaryBalanced_init(LinkLayerPrimaryBalanced self, LinkLayer linkLayer,
                              IBalancedApplicationLayer applicationLayer, void* appLayerParam)
{
    self->primaryState = PLL_IDLE;
    self->state = LL_STATE_IDLE;

    self->waitingForResponse = false;
    self->sendLinkLayerTestFunction = false;
    self->nextFcb = true;

    self->linkLayer = linkLayer;

    self->applicationLayer = applicationLayer;
    self->applicationLayerParam = appLayerParam;

    self->stateChangedHandler = NULL;

    self->idleTimeout = 5000;

    self->otherStationAddress = 0;
}

static void
llpb_setNewState(LinkLayerPrimaryBalanced self, LinkLayerState newState)
{
    if (newState != self->state)
    {
        self->state = newState;

        if (self->stateChangedHandler)
            self->stateChangedHandler(self->stateChangedHandlerParameter, -1, newState);
    }
}

void
LinkLayerPrimaryBalanced_setStateChangeHandler(LinkLayerPrimaryBalanced self,
                                               IEC60870_LinkLayerStateChangedHandler handler, void* parameter)
{
    self->stateChangedHandler = handler;
    self->stateChangedHandlerParameter = parameter;
}

void
LinkLayerPrimaryBalanced_handleMessage(LinkLayerPrimaryBalanced self, uint8_t fc, bool dir, bool dfc, int address,
                                       uint8_t* msg, int userDataStart, int userDataLength)
{
    UNUSED_PARAMETER(dir);
    UNUSED_PARAMETER(address);
    UNUSED_PARAMETER(msg);
    UNUSED_PARAMETER(userDataStart);
    UNUSED_PARAMETER(userDataLength);

    PrimaryLinkLayerState primaryState = self->primaryState;
    PrimaryLinkLayerState newState = primaryState;

    self->lastReceivedMsg = Hal_getMonotonicTimeInMs();

    if (dfc)
    {
        switch (self->primaryState)
        {
        case PLL_EXECUTE_REQUEST_STATUS_OF_LINK:
        case PLL_EXECUTE_RESET_REMOTE_LINK:
            newState = PLL_EXECUTE_REQUEST_STATUS_OF_LINK;
            break;
        case PLL_EXECUTE_SERVICE_SEND_CONFIRM:
        case PLL_SECONDARY_LINK_LAYER_BUSY:
            newState = PLL_SECONDARY_LINK_LAYER_BUSY;
            break;

        default:
            break;
        }

        llpb_setNewState(self, LL_STATE_BUSY);
        self->primaryState = newState;
        return;
    }

    switch (fc)
    {

    case LL_FC_00_ACK:

        DEBUG_PRINT("PLL - RECV FC 00 - ACK\n");

        if (primaryState == PLL_EXECUTE_RESET_REMOTE_LINK)
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;
            llpb_setNewState(self, LL_STATE_AVAILABLE);

            self->waitingForResponse = false;
        }
        else if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            if (self->sendLinkLayerTestFunction)
                self->sendLinkLayerTestFunction = false;

            newState = PLL_LINK_LAYERS_AVAILABLE;
            llpb_setNewState(self, LL_STATE_AVAILABLE);

            self->waitingForResponse = false;
        }
        else if (primaryState == PLL_EXECUTE_REQUEST_STATUS_OF_LINK)
        {
            /* stay in state and wait for response */
            DEBUG_PRINT("ACK (FC 00) unexpected -> expected status-of-link (FC 11)\n");
        }
        else
        {
            self->waitingForResponse = false;
        }

        break;

    case LL_FC_01_NACK:

        DEBUG_PRINT("PLL - RECV FC 01 - NACK\n");

        if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            newState = PLL_SECONDARY_LINK_LAYER_BUSY;
            llpb_setNewState(self, LL_STATE_BUSY);
        }

        break;

    case LL_FC_08_RESP_USER_DATA:

        DEBUG_PRINT("PLL - RECV FC 08 - RESP USER DATA\n");

        newState = PLL_IDLE;
        llpb_setNewState(self, LL_STATE_ERROR);

        break;

    case LL_FC_09_RESP_NACK_NO_DATA:

        DEBUG_PRINT("PLL - RECV FC 09 - RESP NACK - NO DATA\n");

        newState = PLL_IDLE;
        llpb_setNewState(self, LL_STATE_ERROR);

        break;

    case LL_FC_11_STATUS_OF_LINK_OR_ACCESS_DEMAND:

        DEBUG_PRINT("PLL - RECV FC 11 - STATUS OF LINK\n");

        if (primaryState == PLL_EXECUTE_REQUEST_STATUS_OF_LINK)
        {
            DEBUG_PRINT("PLL - SEND  FC 00 - RESET REMOTE LINK\n");

            SendFixedFrame(self->linkLayer, LL_FC_00_RESET_REMOTE_LINK, self->otherStationAddress, true,
                           self->linkLayer->dir, false, false);

            self->lastSendTime = Hal_getMonotonicTimeInMs();
            self->waitingForResponse = true;
            newState = PLL_EXECUTE_RESET_REMOTE_LINK;
            llpb_setNewState(self, LL_STATE_BUSY);
        }
        else
        { /* illegal message in this state */
            newState = PLL_IDLE;
            llpb_setNewState(self, LL_STATE_ERROR);
        }

        break;

    case LL_FC_14_SERVICE_NOT_FUNCTIONING:
    case LL_FC_15_SERVICE_NOT_IMPLEMENTED:

        DEBUG_PRINT("PLL - link layer service not functioning/not implemented in secondary station\n");

        if (self->sendLinkLayerTestFunction)
            self->sendLinkLayerTestFunction = false;

        if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;
            llpb_setNewState(self, LL_STATE_AVAILABLE);
        }

        break;

    default:

        DEBUG_PRINT("UNEXPECTED SECONDARY LINK LAYER MESSAGE\n");

        break;
    }

    DEBUG_PRINT("PLL RECV - old state: %i new state %i\n", primaryState, newState);

    self->primaryState = newState;
}

void
LinkLayerPrimaryBalanced_runStateMachine(LinkLayerPrimaryBalanced self)
{
    uint64_t currentTime = Hal_getMonotonicTimeInMs();

    PrimaryLinkLayerState primaryState = self->primaryState;
    PrimaryLinkLayerState newState = primaryState;

    switch (primaryState)
    {

    case PLL_IDLE:

        self->originalSendTime = 0;
        self->sendLinkLayerTestFunction = false;

        SendFixedFrame(self->linkLayer, LL_FC_09_REQUEST_LINK_STATUS, self->otherStationAddress, true,
                       self->linkLayer->dir, false, false);

        self->lastSendTime = currentTime;
        self->waitingForResponse = true;

        newState = PLL_EXECUTE_REQUEST_STATUS_OF_LINK;

        break;

    case PLL_EXECUTE_REQUEST_STATUS_OF_LINK:

        if (self->waitingForResponse)
        {
            if (self->lastSendTime > currentTime)
            {
                /* last sent time not plausible! */
                self->lastSendTime = currentTime;
            }

            if (currentTime > (self->lastSendTime + self->linkLayer->linkLayerParameters->timeoutForAck))
            {
                newState = PLL_IDLE;
            }
        }
        else
        {
            DEBUG_PRINT("PLL - SEND RESET REMOTE LINK\n");

            SendFixedFrame(self->linkLayer, LL_FC_00_RESET_REMOTE_LINK, self->otherStationAddress, true,
                           self->linkLayer->dir, false, false);

            self->lastSendTime = currentTime;
            self->waitingForResponse = true;
            newState = PLL_EXECUTE_RESET_REMOTE_LINK;
        }

        break;

    case PLL_EXECUTE_RESET_REMOTE_LINK:

        if (self->waitingForResponse)
        {
            if (self->lastSendTime > currentTime)
            {
                /* last sent time not plausible! */
                self->lastSendTime = currentTime;
            }

            if (currentTime > (self->lastSendTime + self->linkLayer->linkLayerParameters->timeoutForAck))
            {
                self->waitingForResponse = false;
                newState = PLL_IDLE;
                llpb_setNewState(self, LL_STATE_ERROR);
            }
        }
        else
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;
            llpb_setNewState(self, LL_STATE_AVAILABLE);
        }

        break;

    case PLL_LINK_LAYERS_AVAILABLE:

        if (self->lastReceivedMsg > currentTime)
        {
            /* last received message not plausible */
            self->lastReceivedMsg = currentTime;
        }

        if ((currentTime - self->lastReceivedMsg) > (unsigned int)self->idleTimeout)
        {
            DEBUG_PRINT("PLL - Idle timeout detected. Send link layer test function\n");
            self->sendLinkLayerTestFunction = true;
        }

        if (self->sendLinkLayerTestFunction)
        {
            DEBUG_PRINT("PLL - SEND TEST LINK\n");

            SendFixedFrame(self->linkLayer, LL_FC_02_TEST_FUNCTION_FOR_LINK, self->otherStationAddress, true,
                           self->linkLayer->dir, self->nextFcb, true);

            self->nextFcb = !(self->nextFcb);
            self->lastSendTime = currentTime;
            self->originalSendTime = self->lastSendTime;
            newState = PLL_EXECUTE_SERVICE_SEND_CONFIRM;
        }
        else
        {
            /* provide a buffer where the application layer can encode the user data */
            Frame bufferFrame = BufferFrame_initialize(&(self->lastSendAsdu), self->linkLayer->userDataBuffer, 0);

            Frame asdu = self->applicationLayer->GetUserData(self->applicationLayerParam, bufferFrame);

            if (asdu)
            {
                DEBUG_PRINT("PLL: SEND USER DATA CONFIRMED\n");

                SendVariableLengthFrame(self->linkLayer, LL_FC_03_USER_DATA_CONFIRMED, self->otherStationAddress, true,
                                        self->linkLayer->dir, self->nextFcb, true, asdu);

                self->nextFcb = !(self->nextFcb);
                self->lastSendTime = currentTime;
                self->originalSendTime = self->lastSendTime;
                self->waitingForResponse = true;

                newState = PLL_EXECUTE_SERVICE_SEND_CONFIRM;
            }
        }

        break;

    case PLL_EXECUTE_SERVICE_SEND_CONFIRM:

        if (self->lastSendTime > currentTime)
        {
            /* last sent time not plausible! */
            self->lastSendTime = currentTime;
        }

        if (currentTime > (self->lastSendTime + self->linkLayer->linkLayerParameters->timeoutForAck))
        {
            if (currentTime > (self->originalSendTime + self->linkLayer->linkLayerParameters->timeoutRepeat))
            {
                DEBUG_PRINT("TIMEOUT: ASDU not confirmed after repeated transmission\n");

                newState = PLL_IDLE;
                llpb_setNewState(self, LL_STATE_ERROR);
            }
            else
            {
                DEBUG_PRINT("TIMEOUT: ASDU not confirmed\n");

                if (self->sendLinkLayerTestFunction)
                {
                    DEBUG_PRINT("PLL - repeat send test function\n");

                    SendFixedFrame(self->linkLayer, LL_FC_02_TEST_FUNCTION_FOR_LINK, self->otherStationAddress, true,
                                   self->linkLayer->dir, !(self->nextFcb), true);
                }
                else
                {
                    DEBUG_PRINT("PLL - repeat last ASDU\n");

                    SendVariableLengthFrame(self->linkLayer, LL_FC_03_USER_DATA_CONFIRMED, self->otherStationAddress,
                                            true, self->linkLayer->dir, !(self->nextFcb), true,
                                            (Frame) & (self->lastSendAsdu));
                }

                self->lastSendTime = currentTime;
            }
        }

        break;

    case PLL_SECONDARY_LINK_LAYER_BUSY:
        break;

    default:
        break;
    }

    if (primaryState != newState)
        DEBUG_PRINT("PLL - old state: %i new state: %i\n", primaryState, newState);

    self->primaryState = newState;
}

struct sLinkLayerBalanced
{
    LinkLayer linkLayer;

    IBalancedApplicationLayer applicationLayer;
    void* appLayerParameter;

    struct sLinkLayer _linkLayer;

    struct sLinkLayerPrimaryBalanced primaryLinkLayer;
    struct sLinkLayerSecondaryBalanced secondaryLinkLayer;
};

LinkLayerBalanced
LinkLayerBalanced_create(int linkLayerAddress, SerialTransceiverFT12 transceiver,
                         LinkLayerParameters linkLayerParameters, IBalancedApplicationLayer applicationLayer,
                         void* applicationLayerParameter)
{
    LinkLayerBalanced self = (LinkLayerBalanced)GLOBAL_MALLOC(sizeof(struct sLinkLayerBalanced));

    if (self)
    {
        self->linkLayer = LinkLayer_init(&(self->_linkLayer), linkLayerAddress, transceiver, linkLayerParameters);
        self->applicationLayer = applicationLayer;
        self->appLayerParameter = applicationLayerParameter;

        LinkLayerPrimaryBalanced_init(&(self->primaryLinkLayer), self->linkLayer, applicationLayer,
                                      applicationLayerParameter);
        LinkLayerSecondaryBalanced_init(&(self->secondaryLinkLayer), self->linkLayer, applicationLayer,
                                        applicationLayerParameter);

        self->linkLayer->llPriBalanced = &(self->primaryLinkLayer);
        self->linkLayer->llSecBalanced = &(self->secondaryLinkLayer);
    }

    return self;
}

void
LinkLayerPrimaryBalanced_resetIdleTimeout(LinkLayerPrimaryBalanced self)
{
    self->lastReceivedMsg = Hal_getMonotonicTimeInMs();
}

void
LinkLayerBalanced_sendLinkLayerTestFunction(LinkLayerBalanced self)
{
    self->primaryLinkLayer.sendLinkLayerTestFunction = true;
}

void
LinkLayerBalanced_setStateChangeHandler(LinkLayerBalanced self, IEC60870_LinkLayerStateChangedHandler handler,
                                        void* parameter)
{
    self->primaryLinkLayer.stateChangedHandler = handler;
    self->primaryLinkLayer.stateChangedHandlerParameter = parameter;
}

void
LinkLayerBalanced_setIdleTimeout(LinkLayerBalanced self, int timeoutInMs)
{
    self->primaryLinkLayer.idleTimeout = timeoutInMs;
}

void
LinkLayerBalanced_setDIR(LinkLayerBalanced self, bool dir)
{
    self->linkLayer->dir = dir;
}

void
LinkLayerBalanced_setAddress(LinkLayerBalanced self, int address)
{
    self->linkLayer->address = address;
}

void
LinkLayerBalanced_setOtherStationAddress(LinkLayerBalanced self, int address)
{
    self->primaryLinkLayer.otherStationAddress = address;
}

void
LinkLayerBalanced_destroy(LinkLayerBalanced self)
{
    if (self)
    {
        GLOBAL_FREEMEM(self);
    }
}

void
LinkLayerBalanced_run(LinkLayerBalanced self)
{
    LinkLayer ll = self->linkLayer;

    SerialTransceiverFT12_readNextMessage(ll->transceiver, ll->buffer, HandleMessageBalancedAndPrimaryUnbalanced,
                                          (void*)ll);

    LinkLayerPrimaryBalanced_runStateMachine(&(self->primaryLinkLayer));
}

/******************************************************
 * Unbalanced primary link layer
 *
 * - LinkLayerPrimaryUnbalanced represents the functions
 *   related to all slaves
 * - LinkLayerSlaveConnection represents the primary
 *   layer and state information for a single slave
 *
 *******************************************************/

typedef struct sLinkLayerSlaveConnection* LinkLayerSlaveConnection;

struct sLinkLayerPrimaryUnbalanced
{
    LinkLayerSlaveConnection currentSlave;
    int currentSlaveIndex;

    bool hasNextBroadcastToSend;
    struct sBufferFrame nextBroadcastMessage;
    uint8_t buffer[256];

    IPrimaryApplicationLayer applicationLayer;
    void* applicationLayerParam;

    struct sLinkLayer _linkLayer;
    LinkLayer linkLayer;

    LinkedList slaveConnections;

    IEC60870_LinkLayerStateChangedHandler stateChangedHandler;
    void* stateChangedHandlerParameter;
};

LinkLayerPrimaryUnbalanced
LinkLayerPrimaryUnbalanced_create(SerialTransceiverFT12 transceiver, LinkLayerParameters linkLayerParameters,
                                  IPrimaryApplicationLayer applicationLayer, void* applicationLayerParam)
{
    LinkLayerPrimaryUnbalanced self =
        (LinkLayerPrimaryUnbalanced)GLOBAL_MALLOC(sizeof(struct sLinkLayerPrimaryUnbalanced));

    if (self)
    {
        self->currentSlave = NULL;
        self->currentSlaveIndex = 0;

        self->hasNextBroadcastToSend = false;

        self->applicationLayer = applicationLayer;
        self->applicationLayerParam = applicationLayerParam;

        self->linkLayer = &(self->_linkLayer);

        LinkLayer_init(self->linkLayer, 0, transceiver, linkLayerParameters);

        self->linkLayer->llPriUnbalanced = self;

        BufferFrame_initialize(&(self->nextBroadcastMessage), self->buffer, 0);

        self->slaveConnections = LinkedList_create();

        self->stateChangedHandler = NULL;
    }

    return self;
}

void
LinkLayerPrimaryUnbalanced_setStateChangeHandler(LinkLayerPrimaryUnbalanced self,
                                                 IEC60870_LinkLayerStateChangedHandler handler, void* parameter)
{
    self->stateChangedHandler = handler;
    self->stateChangedHandlerParameter = parameter;
}

void
LinkLayerPrimaryUnbalanced_destroy(LinkLayerPrimaryUnbalanced self)
{
    if (self)
    {
        if (self->slaveConnections)
            LinkedList_destroy(self->slaveConnections);

        GLOBAL_FREEMEM(self);
    }
}

struct sLinkLayerSlaveConnection
{
    LinkLayerPrimaryUnbalanced primaryLink;

    LinkLayerState state; /* user visible state */

    int address;
    PrimaryLinkLayerState primaryState; /* internal PLL state machine state */

    bool hasMessageToSend; /* indicates if an app layer message is ready to be sent */
    struct sBufferFrame nextMessage;
    uint8_t buffer[256];

    uint64_t lastSendTime;
    uint64_t originalSendTime;

    bool requestClass1Data;
    bool requestClass2Data;

    bool dontSendMessages;

    bool waitingForResponse;

    bool sendLinkLayerTestFunction;

    bool nextFcb;
};

static LinkLayerSlaveConnection
LinkLayerSlaveConnection_create(LinkLayerSlaveConnection self, LinkLayerPrimaryUnbalanced primaryLink, int slaveAddress)
{
    if (self == NULL)
        self = (LinkLayerSlaveConnection)GLOBAL_MALLOC(sizeof(struct sLinkLayerSlaveConnection));

    if (self)
    {
        self->primaryLink = primaryLink;
        self->address = slaveAddress;

        self->state = LL_STATE_IDLE;

        self->lastSendTime = 0;
        self->originalSendTime = 0;
        self->nextFcb = true;
        self->waitingForResponse = false;

        self->primaryState = PLL_IDLE;

        self->hasMessageToSend = false;

        self->sendLinkLayerTestFunction = false;

        self->requestClass1Data = false;
        self->requestClass2Data = false;

        BufferFrame_initialize(&(self->nextMessage), self->buffer, 0);
    }

    return self;
}

static void
llsc_setState(LinkLayerSlaveConnection self, LinkLayerState newState)
{
    if (self->state != newState)
    {
        self->state = newState;

        if (self->primaryLink->stateChangedHandler)
            self->primaryLink->stateChangedHandler(self->primaryLink->stateChangedHandlerParameter, self->address,
                                                   newState);
    }
}

static void
LinkLayerSlaveConnection_HandleMessage(LinkLayerSlaveConnection self, uint8_t fc, bool acd, bool dfc, int address,
                                       uint8_t* msg, int userDataStart, int userDataLength)
{
    IPrimaryApplicationLayer applicationLayer = self->primaryLink->applicationLayer;

    PrimaryLinkLayerState primaryState = self->primaryState;
    PrimaryLinkLayerState newState = primaryState;

    if (dfc)
    {
        DEBUG_PRINT("[SLAVE %i] PLL - DFC = true!\n", self->address);

        /* stop sending ASDUs; only send Status of link requests */
        self->dontSendMessages = true;

        switch (self->primaryState)
        {
        case PLL_EXECUTE_REQUEST_STATUS_OF_LINK:
        case PLL_EXECUTE_RESET_REMOTE_LINK:
            newState = PLL_EXECUTE_REQUEST_STATUS_OF_LINK;
            break;
        case PLL_EXECUTE_SERVICE_SEND_CONFIRM:
        case PLL_SECONDARY_LINK_LAYER_BUSY:
            newState = PLL_SECONDARY_LINK_LAYER_BUSY;
            break;

        default:
            break;
        }

        llsc_setState(self, LL_STATE_BUSY);

        self->primaryState = newState;
        return;
    }
    else
    {
        /* unblock transmission of application layer messages */
        self->dontSendMessages = false;
    }

    if (acd)
        self->requestClass1Data = true;

    switch (fc)
    {

    case LL_FC_00_ACK:

        DEBUG_PRINT("[SLAVE %i] PLL - received ACK\n", self->address);

        if (primaryState == PLL_EXECUTE_RESET_REMOTE_LINK)
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;

            llsc_setState(self, LL_STATE_AVAILABLE);
        }
        else if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            if (self->sendLinkLayerTestFunction)
                self->sendLinkLayerTestFunction = false;
            else
                self->hasMessageToSend = false;

            llsc_setState(self, LL_STATE_AVAILABLE);

            newState = PLL_LINK_LAYERS_AVAILABLE;
        }
        else if (primaryState == PLL_EXECUTE_SERVICE_REQUEST_RESPOND)
        {
            /* single char ACK is interpreted as RESP NO DATA */

            llsc_setState(self, LL_STATE_AVAILABLE);

            newState = PLL_LINK_LAYERS_AVAILABLE;
        }

        self->waitingForResponse = false;
        break;

    case LL_FC_01_NACK:

        DEBUG_PRINT("[SLAVE %i] PLL - received NACK\n", self->address);

        if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            llsc_setState(self, LL_STATE_BUSY);

            newState = PLL_SECONDARY_LINK_LAYER_BUSY;
        }

        self->waitingForResponse = false;

        break;

    case LL_FC_11_STATUS_OF_LINK_OR_ACCESS_DEMAND:

        DEBUG_PRINT("[SLAVE %i] PLL - received STATUS OF LINK\n", self->address);

        if (primaryState == PLL_EXECUTE_REQUEST_STATUS_OF_LINK)
        {
            DEBUG_PRINT("[SLAVE %i] PLL - SEND RESET REMOTE LINK\n", self->address);

            SendFixedFrame(self->primaryLink->linkLayer, LL_FC_00_RESET_REMOTE_LINK, self->address, true, false, false,
                           false);

            self->lastSendTime = Hal_getMonotonicTimeInMs();
            self->waitingForResponse = true;
            newState = PLL_EXECUTE_RESET_REMOTE_LINK;

            llsc_setState(self, LL_STATE_BUSY);
        }
        else
        { /* illegal message */
            newState = PLL_IDLE;

            llsc_setState(self, LL_STATE_ERROR);
        }

        break;

    case LL_FC_08_RESP_USER_DATA:

        DEBUG_PRINT("[SLAVE %i] PLL - received USER DATA\n", self->address);

        if (primaryState == PLL_EXECUTE_SERVICE_REQUEST_RESPOND)
        {
            if (applicationLayer->UserData)
                applicationLayer->UserData(self->primaryLink->applicationLayerParam, address, msg, userDataStart,
                                           userDataLength);

            self->requestClass1Data = false;
            self->requestClass2Data = false;

            newState = PLL_LINK_LAYERS_AVAILABLE;

            llsc_setState(self, LL_STATE_AVAILABLE);
        }
        else
        { /* illegal message */
            newState = PLL_IDLE;

            llsc_setState(self, LL_STATE_ERROR);
        }

        self->waitingForResponse = false;

        break;

    case LL_FC_09_RESP_NACK_NO_DATA:

        DEBUG_PRINT("[SLAVE %i] PLL - received RESP NO DATA\n", self->address);

        if (primaryState == PLL_EXECUTE_SERVICE_REQUEST_RESPOND)
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;

            llsc_setState(self, LL_STATE_AVAILABLE);
        }
        else
        { /* illegal message */
            newState = PLL_IDLE;

            llsc_setState(self, LL_STATE_ERROR);
        }

        self->waitingForResponse = false;

        break;

    case LL_FC_14_SERVICE_NOT_FUNCTIONING:
    case LL_FC_15_SERVICE_NOT_IMPLEMENTED:

        DEBUG_PRINT("[SLAVE %i] PLL - link layer service not functioning/not implemented in secondary station\n",
                    self->address);

        if (primaryState == PLL_EXECUTE_SERVICE_SEND_CONFIRM)
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;

            llsc_setState(self, LL_STATE_AVAILABLE);
        }

        self->waitingForResponse = false;

        break;

    default:
        DEBUG_PRINT("[SLAVE %i] UNEXPECTED SECONDARY LINK LAYER MESSAGE\n", self->address);

        self->waitingForResponse = false;

        break;
    }

    if (acd)
    {
        if (applicationLayer->AccessDemand)
            applicationLayer->AccessDemand(self->primaryLink->applicationLayerParam, address);
    }

    if (primaryState != newState)
        DEBUG_PRINT("[SLAVE %i] PLL RECV: old state: %i new state: %i\n", self->address, primaryState, newState);

    self->primaryState = newState;
}

static bool
llsc_isMessageWaitingToSend(LinkLayerSlaveConnection self)
{
    if ((self->requestClass1Data) || (self->requestClass2Data) || (self->hasMessageToSend))
        return true;
    else
        return false;
}

static void
LinkLayerSlaveConnection_runStateMachine(LinkLayerSlaveConnection self)
{
    uint64_t currentTime = Hal_getMonotonicTimeInMs();

    PrimaryLinkLayerState primaryState = self->primaryState;
    PrimaryLinkLayerState newState = primaryState;

    switch (primaryState)
    {
    case PLL_TIMEOUT:

        if (self->lastSendTime > currentTime)
        {
            /* last sent time not plausible! */
            self->lastSendTime = currentTime;
        }

        if (currentTime > (self->lastSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutLinkState))
        {
            newState = PLL_IDLE;
        }

        break;

    case PLL_IDLE:

        self->originalSendTime = 0;
        self->sendLinkLayerTestFunction = false;

        DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 09 - REQUEST LINK STATUS\n", self->address);

        SendFixedFrame(self->primaryLink->linkLayer, LL_FC_09_REQUEST_LINK_STATUS, self->address, true, false, false,
                       false);

        self->lastSendTime = currentTime;
        self->waitingForResponse = true;
        newState = PLL_EXECUTE_REQUEST_STATUS_OF_LINK;

        break;

    case PLL_EXECUTE_REQUEST_STATUS_OF_LINK:

        if (self->waitingForResponse)
        {
            if (self->lastSendTime > currentTime)
            {
                /* last sent time not plausible! */
                self->lastSendTime = currentTime;
            }

            if (currentTime > (self->lastSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutForAck))
            {
                self->waitingForResponse = false;
                self->lastSendTime = currentTime;
                newState = PLL_TIMEOUT;
            }
        }
        else
        {
            DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 00 - RESET REMOTE LINK\n", self->address);

            SendFixedFrame(self->primaryLink->linkLayer, LL_FC_00_RESET_REMOTE_LINK, self->address, true, false, false,
                           false);

            self->lastSendTime = currentTime;
            self->waitingForResponse = true;
            self->nextFcb = true;
            newState = PLL_EXECUTE_RESET_REMOTE_LINK;
        }

        break;

    case PLL_EXECUTE_RESET_REMOTE_LINK:

        if (self->waitingForResponse)
        {
            if (self->lastSendTime > currentTime)
            {
                /* last sent time not plausible! */
                self->lastSendTime = currentTime;
            }

            if (currentTime > (self->lastSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutForAck))
            {
                self->waitingForResponse = false;
                self->lastSendTime = currentTime;
                newState = PLL_TIMEOUT;

                llsc_setState(self, LL_STATE_ERROR);
            }
        }
        else
        {
            newState = PLL_LINK_LAYERS_AVAILABLE;

            llsc_setState(self, LL_STATE_AVAILABLE);
        }

        break;

    case PLL_LINK_LAYERS_AVAILABLE:

        if (self->sendLinkLayerTestFunction)
        {
            DEBUG_PRINT("[SLAVE %i] PLL - FC 02 - SEND TEST LINK\n", self->address);

            SendFixedFrame(self->primaryLink->linkLayer, LL_FC_02_TEST_FUNCTION_FOR_LINK, self->address, true, false,
                           self->nextFcb, true);

            self->nextFcb = !(self->nextFcb);
            self->lastSendTime = currentTime;
            self->originalSendTime = currentTime;
            self->waitingForResponse = true;

            newState = PLL_EXECUTE_SERVICE_REQUEST_RESPOND;
        }
        else if (self->hasMessageToSend)
        {
            DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 03 - USER DATA CONFIRMED\n", self->address);

            SendVariableLengthFrame(self->primaryLink->linkLayer, LL_FC_03_USER_DATA_CONFIRMED, self->address, true,
                                    false, self->nextFcb, true, (Frame) & (self->nextMessage));

            self->nextFcb = !(self->nextFcb);
            self->lastSendTime = currentTime;
            self->originalSendTime = currentTime;
            self->waitingForResponse = true;

            newState = PLL_EXECUTE_SERVICE_SEND_CONFIRM;
        }
        else if (self->requestClass1Data || self->requestClass2Data)
        {
            if (self->requestClass1Data)
            {
                DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 10 - REQ UD 1\n", self->address);

                SendFixedFrame(self->primaryLink->linkLayer, LL_FC_10_REQUEST_USER_DATA_CLASS_1, self->address, true,
                               false, self->nextFcb, true);

                self->requestClass1Data = false;
            }
            else
            {
                DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 11 - REQ UD 2\n", self->address);

                SendFixedFrame(self->primaryLink->linkLayer, LL_FC_11_REQUEST_USER_DATA_CLASS_2, self->address, true,
                               false, self->nextFcb, true);

                self->requestClass2Data = false;
            }

            self->nextFcb = !(self->nextFcb);
            self->lastSendTime = currentTime;
            self->originalSendTime = currentTime;
            self->waitingForResponse = true;
            newState = PLL_EXECUTE_SERVICE_REQUEST_RESPOND;
        }

        break;

    case PLL_EXECUTE_SERVICE_SEND_CONFIRM:

        if (self->lastSendTime > currentTime)
        {
            /* last sent time not plausible! */
            self->lastSendTime = currentTime;
        }

        if (currentTime > (self->lastSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutForAck))
        {
            if (currentTime >
                (self->originalSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutRepeat))
            {
                DEBUG_PRINT("[SLAVE %i] TIMEOUT: ASDU not confirmed after repeated transmission\n", self->address);

                self->waitingForResponse = false;
                self->lastSendTime = currentTime;
                newState = PLL_TIMEOUT;

                llsc_setState(self, LL_STATE_ERROR);
            }
            else
            {
                DEBUG_PRINT("[SLAVE %i] TIMEOUT: ASDU not confirmed\n", self->address);

                if (self->sendLinkLayerTestFunction)
                {
                    DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 02 - RESET REMOTE LINK [REPEAT]\n", self->address);

                    SendFixedFrame(self->primaryLink->linkLayer, LL_FC_02_TEST_FUNCTION_FOR_LINK, self->address, true,
                                   false, !(self->nextFcb), true);
                }
                else
                {
                    DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 03 - USER DATA CONFIRMED [REPEAT]\n", self->address);

                    SendVariableLengthFrame(self->primaryLink->linkLayer, LL_FC_03_USER_DATA_CONFIRMED, self->address,
                                            true, false, !(self->nextFcb), true, (Frame) & (self->nextMessage));
                }

                self->lastSendTime = currentTime;
            }
        }

        break;

    case PLL_EXECUTE_SERVICE_REQUEST_RESPOND:

        if (self->lastSendTime > currentTime)
        {
            /* last sent time not plausible! */
            self->lastSendTime = currentTime;
        }

        if (currentTime > (self->lastSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutForAck))
        {
            if (currentTime >
                (self->originalSendTime + self->primaryLink->linkLayer->linkLayerParameters->timeoutRepeat))
            {
                DEBUG_PRINT("[SLAVE %i] TIMEOUT: ASDU not confirmed after repeated transmission\n", self->address);

                newState = PLL_IDLE;
                self->requestClass1Data = false;
                self->requestClass2Data = false;

                llsc_setState(self, LL_STATE_ERROR);
            }
            else
            {
                DEBUG_PRINT("[SLAVE %i] TIMEOUT: ASDU not confirmed\n", self->address);

                if (self->requestClass1Data)
                {
                    DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 10 - REQ UD 1 [REPEAT]\n", self->address);

                    SendFixedFrame(self->primaryLink->linkLayer, LL_FC_10_REQUEST_USER_DATA_CLASS_1, self->address,
                                   true, false, !(self->nextFcb), true);
                }
                else
                {
                    DEBUG_PRINT("[SLAVE %i] PLL - SEND FC 11 - REQ UD 2 [REPEAT]\n", self->address);

                    SendFixedFrame(self->primaryLink->linkLayer, LL_FC_11_REQUEST_USER_DATA_CLASS_2, self->address,
                                   true, false, !(self->nextFcb), true);
                }

                self->lastSendTime = currentTime;
            }
        }

        break;

    case PLL_SECONDARY_LINK_LAYER_BUSY:
        break;
    }

    if (primaryState != newState)
        DEBUG_PRINT("[SLAVE %i] PLL: old state: %i new state: %i\n", self->address, primaryState, newState);

    self->primaryState = newState;
}

static LinkLayerSlaveConnection
LinkLayerPrimaryUnbalanced_getSlaveConnection(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection retVal = NULL;

    LinkedList element = LinkedList_getNext(self->slaveConnections);

    while (element)
    {
        LinkLayerSlaveConnection slaveConnection = (LinkLayerSlaveConnection)LinkedList_getData(element);

        if (slaveConnection->address == slaveAddress)
        {
            retVal = slaveConnection;
            break;
        }

        element = LinkedList_getNext(element);
    }

    return retVal;
}

void
LinkLayerPrimaryUnbalanced_addSlaveConnection(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slaveConnection = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slaveConnection == NULL)
    {
        LinkLayerSlaveConnection newSlave = LinkLayerSlaveConnection_create(NULL, self, slaveAddress);

        LinkedList_add(self->slaveConnections, newSlave);
    }
}

void
LinkLayerPrimaryUnbalanced_resetCU(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
    {
        /* slave->resetCU = true; */
    }
}

bool
LinkLayerPrimaryUnbalanced_isChannelAvailable(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
        return !(llsc_isMessageWaitingToSend(slave));

    return false;
}

void
LinkLayerPrimaryUnbalanced_sendLinkLayerTestFunction(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
        slave->sendLinkLayerTestFunction = true;
}

bool
LinkLayerPrimaryUnbalanced_requestClass1Data(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
    {
        slave->requestClass1Data = true;
        return true;
    }

    return false;
}

bool
LinkLayerPrimaryUnbalanced_requestClass2Data(LinkLayerPrimaryUnbalanced self, int slaveAddress)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
    {
        slave->requestClass2Data = true;
        return true;
    }

    return false;
}

bool
LinkLayerPrimaryUnbalanced_sendConfirmed(LinkLayerPrimaryUnbalanced self, int slaveAddress, BufferFrame message)
{
    LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

    if (slave)
    {
        if (slave->hasMessageToSend == false)
        {
            slave->nextMessage.msgSize = message->msgSize;
            slave->nextMessage.startSize = message->startSize;
            memcpy(slave->nextMessage.buffer, message->buffer, message->msgSize);

            slave->hasMessageToSend = true;
            return true;
        }
    }

    return false;
}

bool
LinkLayerPrimaryUnbalanced_sendNoReply(LinkLayerPrimaryUnbalanced self, int slaveAddress, BufferFrame message)
{
    if (slaveAddress == LinkLayer_getBroadcastAddress(self->linkLayer))
    {
        if (self->hasNextBroadcastToSend)
            return false;
        else
        {
            self->nextBroadcastMessage.msgSize = message->msgSize;
            self->nextBroadcastMessage.startSize = message->startSize;
            memcpy(self->nextBroadcastMessage.buffer, message->buffer, message->msgSize);

            self->hasNextBroadcastToSend = true;
            return true;
        }
    }
    else
    {
        LinkLayerSlaveConnection slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, slaveAddress);

        if (slave)
        {
            if (slave->hasMessageToSend == false)
            {
                slave->nextMessage.msgSize = message->msgSize;
                slave->nextMessage.startSize = message->startSize;
                memcpy(slave->nextMessage.buffer, message->buffer, message->msgSize);

                slave->hasMessageToSend = true;
                return true;
            }
        }
    }

    return false;
}

void
LinkLayerPrimaryUnbalanced_handleMessage(LinkLayerPrimaryUnbalanced self, uint8_t fc, bool acd, bool dfc, int address,
                                         uint8_t* msg, int userDataStart, int userDataLength)
{
    LinkLayerSlaveConnection slave = NULL;

    if (address == -1)
        slave = self->currentSlave;
    else
        slave = LinkLayerPrimaryUnbalanced_getSlaveConnection(self, address);

    if (slave)
    {
        LinkLayerSlaveConnection_HandleMessage(slave, fc, acd, dfc, address, msg, userDataStart, userDataLength);
    }
    else
    {
        DEBUG_PRINT("PLL RECV - response from unknown slave %i\n", address);
    }
}

void
LinkLayerPrimaryUnbalanced_runStateMachine(LinkLayerPrimaryUnbalanced self)
{
    if (self->hasNextBroadcastToSend)
    {
        /* send pending broadcast message */

        SendVariableLengthFrame(self->linkLayer, LL_FC_04_USER_DATA_NO_REPLY,
                                LinkLayer_getBroadcastAddress(self->linkLayer), true, false, false, false,
                                (Frame) & (self->nextBroadcastMessage));

        self->hasNextBroadcastToSend = false;
    }

    /* run all the link layer state machines for the registered slaves */
    if (LinkedList_size(self->slaveConnections) > 0)
    {

        if (self->currentSlave != NULL)
        {
            if (self->currentSlave->waitingForResponse == false)
                self->currentSlave = NULL;
        }

        if (self->currentSlave == NULL)
        {
            /* schedule next slave connection */
            self->currentSlave = (LinkLayerSlaveConnection)LinkedList_getData(
                LinkedList_get(self->slaveConnections, self->currentSlaveIndex));
            self->currentSlaveIndex = (self->currentSlaveIndex + 1) % LinkedList_size(self->slaveConnections);
        }

        if (self->currentSlave)
            LinkLayerSlaveConnection_runStateMachine(self->currentSlave);
    }
}

void
LinkLayerPrimaryUnbalanced_run(LinkLayerPrimaryUnbalanced self)
{
    LinkLayer ll = self->linkLayer;

    SerialTransceiverFT12_readNextMessage(ll->transceiver, ll->buffer, HandleMessageBalancedAndPrimaryUnbalanced,
                                          (void*)ll);

    LinkLayerPrimaryUnbalanced_runStateMachine(self);
}
