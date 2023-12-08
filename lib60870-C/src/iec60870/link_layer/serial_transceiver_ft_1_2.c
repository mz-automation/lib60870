/*
 *  serial_transceiver_ft_1_2.c
 *
 *  Copyright 2017-2022 Michael Zillgith
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

#include "hal_serial.h"
#include "serial_transceiver_ft_1_2.h"
#include "lib_memory.h"
#include <stdlib.h>
#include <stdbool.h>
#include "lib60870_internal.h"

struct sSerialTransceiverFT12 {
    int messageTimeout;
    int characterTimeout;
    LinkLayerParameters linkLayerParameters;
    SerialPort serialPort;
    IEC60870_RawMessageHandler rawMessageHandler;
    void* rawMessageHandlerParameter;
};

SerialTransceiverFT12
SerialTransceiverFT12_create(SerialPort serialPort, LinkLayerParameters linkLayerParameters)
{
    SerialTransceiverFT12 self = (SerialTransceiverFT12) GLOBAL_MALLOC(sizeof(struct sSerialTransceiverFT12));

    if (self != NULL) {
        self->messageTimeout = 10;
        self->characterTimeout = 300;
        self->linkLayerParameters = linkLayerParameters;
        self->serialPort = serialPort;
        self->rawMessageHandler = NULL;
    }

    return self;
}

void
SerialTransceiverFT12_destroy(SerialTransceiverFT12 self)
{
    if (self != NULL)
        GLOBAL_FREEMEM(self);
}

void
SerialTransceiverFT12_setTimeouts(SerialTransceiverFT12 self, int messageTimeout, int characterTimeout)
{
    self->messageTimeout = messageTimeout;
    self->characterTimeout = characterTimeout;
}

void
SerialTransceiverFT12_setRawMessageHandler(SerialTransceiverFT12 self, IEC60870_RawMessageHandler handler, void* parameter)
{
    self->rawMessageHandler = handler;
    self->rawMessageHandlerParameter = parameter;
}

int
SerialTransceiverFT12_getBaudRate(SerialTransceiverFT12 self)
{
    return SerialPort_getBaudRate(self->serialPort);
}

void
SerialTransceiverFT12_sendMessage(SerialTransceiverFT12 self, uint8_t* msg, int msgSize)
{
    if (self->rawMessageHandler)
        self->rawMessageHandler(self->rawMessageHandlerParameter, msg, msgSize, true);

    SerialPort_write(self->serialPort, msg, 0, msgSize);
}

static int
readBytesWithTimeout(SerialTransceiverFT12 self, uint8_t* buffer, int startIndex, int count)
{
    int readByte;
    int readBytes = 0;

    while ((readByte = SerialPort_readByte(self->serialPort)) != -1) {
        buffer[startIndex++] = (uint8_t) readByte;

        readBytes++;

        if (readBytes >= count)
            break;
    }

    return readBytes;
}

void
SerialTransceiverFT12_readNextMessage(SerialTransceiverFT12 self, uint8_t* buffer,
        SerialTXMessageHandler messageHandler, void* parameter)
{
    SerialPort_setTimeout(self->serialPort, self->messageTimeout);

    int read = SerialPort_readByte(self->serialPort);

    if (read != -1) {

        if (read == 0x68) {

            SerialPort_setTimeout(self->serialPort, self->characterTimeout);

            int msgSize = SerialPort_readByte(self->serialPort);

            if (msgSize == -1)
                goto sync_error;

            buffer[0] = (uint8_t) 0x68;
            buffer[1] = (uint8_t) msgSize;

            msgSize += 4;

            int readBytes = readBytesWithTimeout(self, buffer, 2, msgSize);

            if (readBytes == msgSize) {
                msgSize += 2;

                if (self->rawMessageHandler)
                    self->rawMessageHandler(self->rawMessageHandlerParameter, buffer, msgSize, false);

                messageHandler(parameter, buffer, msgSize);
            }
            else {
                DEBUG_PRINT("RECV: Timeout reading variable length frame size = %i (expected = %i)\n", readBytes, msgSize);
            }

        }
        else if (read == 0x10) {

            SerialPort_setTimeout(self->serialPort, self->characterTimeout);

            buffer[0] = 0x10;

            int msgSize = 3 + self->linkLayerParameters->addressLength;

            int readBytes = readBytesWithTimeout(self, buffer, 1, msgSize);

            if (readBytes == msgSize) {
                msgSize += 1;

                if (self->rawMessageHandler)
                    self->rawMessageHandler(self->rawMessageHandlerParameter, buffer, msgSize, false);

                messageHandler(parameter, buffer, msgSize);
            }
            else {
                DEBUG_PRINT("RECV: Timeout reading fixed length frame size = %i (expected = %i)\n", readBytes, msgSize);
            }

        }
        else if (read == 0xe5) {
            int msgSize = 1;
            buffer[0] = (uint8_t) read;

            if (self->rawMessageHandler)
                self->rawMessageHandler(self->rawMessageHandlerParameter, buffer, msgSize, false);

            messageHandler(parameter, buffer, msgSize);
        }
        else {
            goto sync_error;
        }

    }

    return;

sync_error:

    DEBUG_PRINT("RECV: SYNC ERROR\n");

    SerialPort_discardInBuffer(self->serialPort);

    return;
}
