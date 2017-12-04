/*
 *  serial_port.h
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

#ifndef SRC_IEC60870_LINK_LAYER_SERIAL_PORT_H_
#define SRC_IEC60870_LINK_LAYER_SERIAL_PORT_H_

#include <stdint.h>

typedef struct sSerialPort* SerialPort;

typedef enum {
    SERIAL_PORT_ERROR_NONE = 0,
    SERIAL_PORT_ERROR_INVALID_ARGUMENT = 1,
    SERIAL_PORT_ERROR_INVALID_BAUDRATE = 2,
    SERIAL_PORT_ERROR_OPEN_FAILED = 3,
    SERIAL_PORT_ERROR_UNKNOWN = 99
} SerialPortError;

SerialPort
SerialPort_create(const char* interfaceName, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits);

void
SerialPort_destroy(SerialPort self);

void
SerialPort_open(SerialPort self);

void
SerialPort_close(SerialPort self);

int
SerialPort_getBaudRate(SerialPort self);

void
SerialPort_setTimeout(SerialPort self, int timeout);

void
SerialPort_discardInBuffer(SerialPort self);

int
SerialPort_readByte(SerialPort self);

int
SerialPort_write(SerialPort self, uint8_t* buffer, int startPos, int bufSize);

SerialPortError
SerialPort_getLastError(SerialPort self);


#endif /* SRC_IEC60870_LINK_LAYER_SERIAL_PORT_H_ */
