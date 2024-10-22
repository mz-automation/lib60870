/*
 *  link_layer.h
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

#ifndef SRC_IEC60870_LINK_LAYER_LINK_LAYER_H_
#define SRC_IEC60870_LINK_LAYER_LINK_LAYER_H_

#include <stdbool.h>
#include "iec60870_common.h"
#include "frame.h"
#include "buffer_frame.h"
#include "serial_transceiver_ft_1_2.h"
#include "link_layer_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sLinkLayer* LinkLayer;

typedef struct sLinkLayerBalanced* LinkLayerBalanced;

typedef struct sLinkLayerSecondaryUnbalanced* LinkLayerSecondaryUnbalanced;

typedef struct sLinkLayerPrimaryUnbalanced* LinkLayerPrimaryUnbalanced;

typedef struct sISecondaryApplicationLayer* ISecondaryApplicationLayer;

struct sISecondaryApplicationLayer {
    bool (*IsClass1DataAvailable) (void* parameter);
    Frame (*GetClass1Data) (void* parameter, Frame frame);
    Frame (*GetClass2Data) (void* parameter, Frame frame);
    bool (*HandleReceivedData) (void* parameter, uint8_t* msg, bool isBroadcast, int userDataStart, int userDataLength);
    void (*ResetCUReceived) (void* parameter, bool onlyFCB);
};

typedef struct sIPrimaryApplicationLayer* IPrimaryApplicationLayer;

struct sIPrimaryApplicationLayer {
    void (*AccessDemand) (void* parameter, int slaveAddress);
    void (*UserData) (void* parameter, int slaveAddress, uint8_t* msg, int start, int length);
    void (*Timeout) (void* parameter, int slaveAddress);
};

typedef struct sIBalancedApplicationLayer* IBalancedApplicationLayer;

struct sIBalancedApplicationLayer {
    Frame (*GetUserData) (void* parameter, Frame frame);
    bool (*HandleReceivedData) (void* parameter, uint8_t* msg, bool isBroadcast, int userDataStart, int userDataLength);
};


LinkLayerPrimaryUnbalanced
LinkLayerPrimaryUnbalanced_create(
        SerialTransceiverFT12 transceiver,
        LinkLayerParameters linkLayerParameters,
        IPrimaryApplicationLayer applicationLayer,
        void* applicationLayerParameter
        );

void
LinkLayerPrimaryUnbalanced_destroy(LinkLayerPrimaryUnbalanced self);

void
LinkLayerPrimaryUnbalanced_setStateChangeHandler(LinkLayerPrimaryUnbalanced self,
        IEC60870_LinkLayerStateChangedHandler handler, void* parameter);

void
LinkLayerPrimaryUnbalanced_addSlaveConnection(LinkLayerPrimaryUnbalanced self, int slaveAddress);

void
LinkLayerPrimaryUnbalanced_resetCU(LinkLayerPrimaryUnbalanced self, int slaveAddress);

bool
LinkLayerPrimaryUnbalanced_isChannelAvailable(LinkLayerPrimaryUnbalanced self, int slaveAddress);

void
LinkLayerPrimaryUnbalanced_sendLinkLayerTestFunction(LinkLayerPrimaryUnbalanced self, int slaveAddress);

bool
LinkLayerPrimaryUnbalanced_requestClass1Data(LinkLayerPrimaryUnbalanced self, int slaveAddress);

bool
LinkLayerPrimaryUnbalanced_requestClass2Data(LinkLayerPrimaryUnbalanced self, int slaveAddress);

bool
LinkLayerPrimaryUnbalanced_sendConfirmed(LinkLayerPrimaryUnbalanced self, int slaveAddress, BufferFrame message);

bool
LinkLayerPrimaryUnbalanced_sendNoReply(LinkLayerPrimaryUnbalanced self, int slaveAddress, BufferFrame message);

void
LinkLayerPrimaryUnbalanced_run(LinkLayerPrimaryUnbalanced self);




LinkLayerSecondaryUnbalanced
LinkLayerSecondaryUnbalanced_create(
        int linkLayerAddress,
        SerialTransceiverFT12 transceiver,
        LinkLayerParameters linkLayerParameters,
        ISecondaryApplicationLayer applicationLayer,
        void* applicationLayerParameter
);

void
LinkLayerSecondaryUnbalanced_destroy(LinkLayerSecondaryUnbalanced self);

void
LinkLayerSecondaryUnbalanced_run(LinkLayerSecondaryUnbalanced self);

void
LinkLayerSecondaryUnbalanced_setIdleTimeout(LinkLayerSecondaryUnbalanced self, int timeoutInMs);

void
LinkLayerSecondaryUnbalanced_setStateChangeHandler(LinkLayerSecondaryUnbalanced self,
        IEC60870_LinkLayerStateChangedHandler handler, void* parameter);

void
LinkLayerSecondaryUnbalanced_setAddress(LinkLayerSecondaryUnbalanced self, int address);

LinkLayerBalanced
LinkLayerBalanced_create(
        int linkLayerAddress,
        SerialTransceiverFT12 transceiver,
        LinkLayerParameters linkLayerParameters,
        IBalancedApplicationLayer applicationLayer,
        void* applicationLayerParameter
        );

void
LinkLayerBalanced_setStateChangeHandler(LinkLayerBalanced self,
        IEC60870_LinkLayerStateChangedHandler handler, void* parameter);

void
LinkLayerBalanced_sendLinkLayerTestFunction(LinkLayerBalanced self);

void
LinkLayerBalanced_setIdleTimeout(LinkLayerBalanced self, int timeoutInMs);

void
LinkLayerBalanced_setDIR(LinkLayerBalanced self, bool dir);

void
LinkLayerBalanced_setAddress(LinkLayerBalanced self, int address);

void
LinkLayerBalanced_setOtherStationAddress(LinkLayerBalanced self, int address);

void
LinkLayerBalanced_destroy(LinkLayerBalanced self);

void
LinkLayerBalanced_run(LinkLayerBalanced self);

LinkLayer
LinkLayer_init(LinkLayer self, int address, SerialTransceiverFT12 transceiver, LinkLayerParameters linkLayerParameters);

void
LinkLayer_setDIR(LinkLayer self, bool dir);

void
LinkLayer_setAddress(LinkLayer self, int address);

#ifdef __cplusplus
}
#endif

#endif /* SRC_IEC60870_LINK_LAYER_LINK_LAYER_H_ */
