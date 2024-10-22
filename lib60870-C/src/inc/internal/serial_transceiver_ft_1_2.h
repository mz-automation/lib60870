/*
 *  serial_transceiver_ft_1_2.h
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

#ifndef SRC_IEC60870_LINK_LAYER_SERIAL_TRANSCEIVER_FT_1_2_H_
#define SRC_IEC60870_LINK_LAYER_SERIAL_TRANSCEIVER_FT_1_2_H_

#include "link_layer_parameters.h"
#include "hal_serial.h"
#include "iec60870_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sSerialTransceiverFT12* SerialTransceiverFT12;

typedef void (*SerialTXMessageHandler) (void* parameter, uint8_t* msg, int msgSize);

SerialTransceiverFT12
SerialTransceiverFT12_create(SerialPort serialPort, LinkLayerParameters linkLayerParameters);

void
SerialTransceiverFT12_destroy(SerialTransceiverFT12 self);

void
SerialTransceiverFT12_setTimeouts(SerialTransceiverFT12 self, int messageTimeout, int characterTimeout);

void
SerialTransceiverFT12_setRawMessageHandler(SerialTransceiverFT12 self, IEC60870_RawMessageHandler handler, void* parameter);

int
SerialTransceiverFT12_getBaudRate(SerialTransceiverFT12 self);

void
SerialTransceiverFT12_sendMessage(SerialTransceiverFT12 self, uint8_t* msg, int msgSize);

void
SerialTransceiverFT12_readNextMessage(SerialTransceiverFT12 self, uint8_t* buffer,
        SerialTXMessageHandler, void* parameter);

#ifdef __cplusplus
}
#endif

#endif /* SRC_IEC60870_LINK_LAYER_SERIAL_TRANSCEIVER_FT_1_2_H_ */


