/*
*  serial_port_win32.c
*
*  Copyright 2013-2021 Michael Zillgith
*
*  This file is part of Platform Abstraction Layer (libpal)
*  for libiec61850, libmms, and lib60870.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdint.h>
#include <stdio.h>

#include <windows.h>

#include "lib_memory.h"

#include "hal_serial.h"
#include "hal_time.h"

struct sSerialPort {
	char interfaceName[100];
	HANDLE comPort;
	int baudRate;
	uint8_t dataBits;
	char parity;
	uint8_t stopBits;
	uint64_t lastSentTime;
	int timeout;
	SerialPortError lastError;
};

SerialPort
SerialPort_create(const char* interfaceName, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits)
{
	SerialPort self = (SerialPort)GLOBAL_MALLOC(sizeof(struct sSerialPort));

	if (self != NULL) {
		self->comPort = NULL;
		self->baudRate = baudRate;
		self->dataBits = dataBits;
		self->stopBits = stopBits;
		self->parity = parity;
		self->lastSentTime = 0;
		self->timeout = 100; /* 100 ms */
		strncpy(self->interfaceName, interfaceName, 100);
		self->lastError = SERIAL_PORT_ERROR_NONE;
	}

	return self;
}

void
SerialPort_destroy(SerialPort self)
{
	if (self != NULL) {

		if (self->comPort != INVALID_HANDLE_VALUE)
			SerialPort_close(self);

		GLOBAL_FREEMEM(self);
	}
}

bool
SerialPort_open(SerialPort self)
{
    COMMTIMEOUTS timeouts = { 0 };

	self->comPort = CreateFile(self->interfaceName, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);

	if (self->comPort == INVALID_HANDLE_VALUE) {
		self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
		return false;
	}

	DCB _serialParams = { 0 };
	_serialParams.DCBlength = sizeof(DCB);

	LPDCB serialParams = &_serialParams;

	BOOL status = GetCommState(self->comPort, serialParams);

	if (status == false) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	/* set baud rate */
	switch (self->baudRate) {
	case 110:
		serialParams->BaudRate = CBR_110;
		break;
	case 300:
		serialParams->BaudRate = CBR_300;
		break;
	case 600:
		serialParams->BaudRate = CBR_600;
		break;
	case 1200:
		serialParams->BaudRate = CBR_1200;
		break;
	case 2400:
		serialParams->BaudRate = CBR_2400;
		break;
	case 4800:
		serialParams->BaudRate = CBR_4800;
		break;
	case 9600:
		serialParams->BaudRate = CBR_9600;
		break;
	case 19200:
		serialParams->BaudRate = CBR_19200;
		break;
	case 38400:
		serialParams->BaudRate = CBR_38400;
		break;
	case 57600:
		serialParams->BaudRate = CBR_57600;
		break;
	case 115200:
		serialParams->BaudRate = CBR_115200;
		break;
	default:
		serialParams->BaudRate = CBR_9600;
		self->lastError = SERIAL_PORT_ERROR_INVALID_BAUDRATE;
	}

	/* Set data bits (5/6/7/8) */
	serialParams->ByteSize = self->dataBits;

	/* Set stop bits (1/2) */
	if (self->stopBits == 1)
		serialParams->StopBits = ONESTOPBIT;
	else /* 2 */
		serialParams->StopBits = TWOSTOPBITS;

	if (self->parity == 'N')
		serialParams->Parity = NOPARITY;
	else if (self->parity == 'E')
		serialParams->Parity = EVENPARITY;
	else /* 'O' */
		serialParams->Parity = ODDPARITY;

	status = SetCommState(self->comPort, serialParams);

	if (status == false) {
		self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
		goto exit_error;
	}

	timeouts.ReadIntervalTimeout = 100;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 100;
	timeouts.WriteTotalTimeoutMultiplier = 10;

	status = SetCommTimeouts(self->comPort, &timeouts);

	if (status == false) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	status = SetCommMask(self->comPort, EV_RXCHAR);

	if (status == false) {
		printf("SetCommMask failed!\n");
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		goto exit_error;
	}

	self->lastError = SERIAL_PORT_ERROR_NONE;

	return true;

exit_error:

	if (self->comPort != INVALID_HANDLE_VALUE) {
		CloseHandle(self->comPort);
		self->comPort = INVALID_HANDLE_VALUE;
	}

	return false;
}


void
SerialPort_close(SerialPort self)
{
	if (self->comPort != INVALID_HANDLE_VALUE) {
		CloseHandle(self->comPort);
		self->comPort = INVALID_HANDLE_VALUE;
	}
}

int
SerialPort_getBaudRate(SerialPort self)
{
	return self->baudRate;
}

void
SerialPort_discardInBuffer(SerialPort self)
{
	PurgeComm(self->comPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

void
SerialPort_setTimeout(SerialPort self, int timeout)
{
	self->timeout = timeout;
}

SerialPortError
SerialPort_getLastError(SerialPort self)
{
	return self->lastError;
}

int
SerialPort_readByte(SerialPort self)
{
	uint8_t buf[1];

	DWORD bytesRead = 0;

	BOOL status = ReadFile(self->comPort, buf, 1, &bytesRead, NULL);

	if (status == false) {
		self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
		return -1;
	}

	self->lastError = SERIAL_PORT_ERROR_NONE;

	if (bytesRead == 0)
		return -1;
	else
		return (int) buf[0];
}

int
SerialPort_write(SerialPort self, uint8_t* buffer, int startPos, int bufSize)
{
	//TODO assure minimum line idle time

    self->lastError = SERIAL_PORT_ERROR_NONE;

	DWORD numberOfBytesWritten;

	BOOL status = WriteFile(self->comPort, buffer + startPos, bufSize, &numberOfBytesWritten, NULL);

	if (status == false) {
	    self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
	    return -1;
	}

	status = FlushFileBuffers(self->comPort);

	if (status == false) {
		printf("FlushFileBuffers failed!\n");
	}

	self->lastSentTime = Hal_getTimeInMs();

	return (int) numberOfBytesWritten;
}
