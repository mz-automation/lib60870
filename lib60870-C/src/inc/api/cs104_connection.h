/*
 *  Copyright 2016-2022 Michael Zillgith
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

#ifndef SRC_INC_CS104_CONNECTION_H_
#define SRC_INC_CS104_CONNECTION_H_

#include <stdbool.h>
#include <stdint.h>

#include "tls_config.h"
#include "iec60870_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file cs104_connection.h
 * \brief CS 104 master side definitions
 */

/**
 * @addtogroup MASTER Master related functions
 *
 * @{
 */

/**
 * @defgroup CS104_MASTER CS 104 master related functions
 *
 * @{
 */

typedef struct sCS104_Connection* CS104_Connection;

/**
 * \brief Create a new connection object
 *
 * \param hostname host name of IP address of the server to connect
 * \param tcpPort tcp port of the server to connect. If set to -1 use default port (2404)
 *
 * \return the new connection object
 */
CS104_Connection
CS104_Connection_create(const char* hostname, int tcpPort);

/**
 * \brief Create a new secure connection object (uses TLS)
 *
 * \param hostname host name of IP address of the server to connect
 * \param tcpPort tcp port of the server to connect. If set to -1 use default port (19998)
 * \param tlcConfig the TLS configuration (certificates, keys, and parameters)
 *
 * \return the new connection object
 */
CS104_Connection
CS104_Connection_createSecure(const char* hostname, int tcpPort, TLSConfiguration tlsConfig);


/**
 * \brief Set the local IP address and port to be used by the client
 * 
 * NOTE: This function is optional. When not used the OS decides what IP address and TCP port to use.
 * 
 * \param self CS104_Connection instance
 * \param localIpAddress the local IP address or hostname as C string
 * \param localPort the local TCP port to use. When < 1 the OS will chose the TCP port to use.
 */
void
CS104_Connection_setLocalAddress(CS104_Connection self, const char* localIpAddress, int localPort);

/**
 * \brief Set the CS104 specific APCI parameters.
 *
 * If not set the default parameters are used. This function must be called before the
 * CS104_Connection_connect function is called! If the function is called after the connect
 * the behavior is undefined.
 *
 * \param self CS104_Connection instance
 * \param parameters the APCI layer parameters
 */
void
CS104_Connection_setAPCIParameters(CS104_Connection self, const CS104_APCIParameters parameters);

/**
 * \brief Get the currently used CS104 specific APCI parameters
 */
CS104_APCIParameters
CS104_Connection_getAPCIParameters(CS104_Connection self);

/**
 * \brief Set the CS101 application layer parameters
 *
 * If not set the default parameters are used. This function must be called before the
 * CS104_Connection_connect function is called! If the function is called after the connect
 * the behavior is undefined.
 *
 * \param self CS104_Connection instance
 * \param parameters the application layer parameters
 */
void
CS104_Connection_setAppLayerParameters(CS104_Connection self, const CS101_AppLayerParameters parameters);

/**
 * \brief Return the currently used application layer parameter
 *
 * NOTE: The application layer parameters are required to create CS101_ASDU objects.
 *
 * \param self CS104_Connection instance
 *
 * \return the currently used CS101_AppLayerParameters object
 */
CS101_AppLayerParameters
CS104_Connection_getAppLayerParameters(CS104_Connection self);

/**
 * \brief Sets the timeout for connecting to the server (in ms)
 * 
 * \deprecated Function has no effect! Set T0 parameter instead.
 *
 * \param self
 * \param millies timeout value in ms
 */
void
CS104_Connection_setConnectTimeout(CS104_Connection self, int millies);

/**
 * \brief non-blocking connect.
 *
 * Invokes a connection establishment to the server and returns immediately.
 *
 * \param self CS104_Connection instance
 */
void
CS104_Connection_connectAsync(CS104_Connection self);

/**
 * \brief blocking connect
 *
 * Establishes a connection to a server. This function is blocking and will return
 * after the connection is established or the connect timeout elapsed.
 *
 * \param self CS104_Connection instance
 * \return true when connected, false otherwise
 */
bool
CS104_Connection_connect(CS104_Connection self);

/**
 * \brief start data transmission on this connection
 *
 * After issuing this command the client (master) will receive spontaneous
 * (unsolicited) messages from the server (slave).
 */
void
CS104_Connection_sendStartDT(CS104_Connection self);

/**
 * \brief stop data transmission on this connection
 */
void
CS104_Connection_sendStopDT(CS104_Connection self);

/**
 * \brief Check if the transmit (send) buffer is full. If true the next send command will fail.
 *
 * The transmit buffer is full when the slave/server didn't confirm the last k sent messages.
 * In this case the next message can only be sent after the next confirmation (by I or S messages)
 * that frees part of the sent messages buffer.
 */
bool
CS104_Connection_isTransmitBufferFull(CS104_Connection self);

/**
 * \brief send an interrogation command
 *
 * \param cot cause of transmission
 * \param ca Common address of the slave/server
 * \param qoi qualifier of interrogation (20 for station interrogation)
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, QualifierOfInterrogation qoi);

/**
 * \brief send a counter interrogation command
 *
 * \param cot cause of transmission
 * \param ca Common address of the slave/server
 * \param qcc
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendCounterInterrogationCommand(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, uint8_t qcc);

/**
 * \brief  Sends a read command (C_RD_NA_1 typeID: 102)
 *
 * This will send a read command C_RC_NA_1 (102) to the slave/outstation. The COT is always REQUEST (5).
 * It is used to implement the cyclical polling of data application function.
 *
 * \param ca Common address of the slave/server
 * \param ioa Information object address of the data point to read
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendReadCommand(CS104_Connection self, int ca, int ioa);

/**
 * \brief Sends a clock synchronization command (C_CS_NA_1 typeID: 103)
 *
 * \param ca Common address of the slave/server
 * \param newTime new system time for the slave/server
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendClockSyncCommand(CS104_Connection self, int ca, CP56Time2a newTime);

/**
 * \brief Send a test command (C_TS_NA_1 typeID: 104)
 *
 * Note: This command is not supported by IEC 60870-5-104
 *
 * \param ca Common address of the slave/server
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendTestCommand(CS104_Connection self, int ca);

/**
 * \brief Send a test command with timestamp (C_TS_TA_1 typeID: 107)
 *
 * \param ca Common address of the slave/server
 * \param tsc test sequence counter
 * \param timestamp CP56Time2a timestamp
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendTestCommandWithTimestamp(CS104_Connection self, int ca, uint16_t tsc, CP56Time2a timestamp);

/**
 * \brief Send a process command to the controlled (or other) station
 *
 * \deprecated Use \ref CS104_Connection_sendProcessCommandEx instead
 *
 * \param typeId the type ID of the command message to send or 0 to use the type ID of the information object
 * \param cot the cause of transmission (should be ACTIVATION to select/execute or ACT_TERM to cancel the command)
 * \param ca the common address of the information object
 * \param command the command information object (e.g. SingleCommand or DoubleCommand)
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendProcessCommand(CS104_Connection self, TypeID typeId, CS101_CauseOfTransmission cot,
        int ca, InformationObject command);

/**
 * \brief Send a process command to the controlled (or other) station
 *
 * \param cot the cause of transmission (should be ACTIVATION to select/execute or ACT_TERM to cancel the command)
 * \param ca the common address of the information object
 * \param command the command information object (e.g. SingleCommand or DoubleCommand)
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendProcessCommandEx(CS104_Connection self, CS101_CauseOfTransmission cot, int ca, InformationObject sc);


/**
 * \brief Send a user specified ASDU
 *
 * \param asdu the ASDU to send
 *
 * \return true if message was sent, false otherwise
 */
bool
CS104_Connection_sendASDU(CS104_Connection self, CS101_ASDU asdu);

/**
 * \brief Register a callback handler for received ASDUs
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS104_Connection_setASDUReceivedHandler(CS104_Connection self, CS101_ASDUReceivedHandler handler, void* parameter);

typedef enum {
    CS104_CONNECTION_OPENED = 0,
    CS104_CONNECTION_CLOSED = 1,
    CS104_CONNECTION_STARTDT_CON_RECEIVED = 2,
    CS104_CONNECTION_STOPDT_CON_RECEIVED = 3,
    CS104_CONNECTION_FAILED = 4
} CS104_ConnectionEvent;

/**
 * \brief Handler that is called when the connection is established or closed
 * 
 * \note Calling \ref CS104_Connection_destroy or \ref CS104_Connection_close inside
 * of the callback causes a memory leak!
 *
 * \param parameter user provided parameter
 * \param connection the connection object
 * \param event event type
 */
typedef void (*CS104_ConnectionHandler) (void* parameter, CS104_Connection connection, CS104_ConnectionEvent event);

/**
 * \brief Set the connection event handler
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS104_Connection_setConnectionHandler(CS104_Connection self, CS104_ConnectionHandler handler, void* parameter);


/**
 * \brief Set the raw message callback (called when a message is sent or received)
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS104_Connection_setRawMessageHandler(CS104_Connection self, IEC60870_RawMessageHandler handler, void* parameter);

/**
 * \brief Close the connection
 */
void
CS104_Connection_close(CS104_Connection self);

/**
 * \brief Close the connection and free all related resources
 */
void
CS104_Connection_destroy(CS104_Connection self);

/*! @} */

/*! @} */

/**
 * \private
 *
 * this function is only intended to be used by test cases and is not part of the API!
 *
 */
int
CS104_Connection_sendMessage(CS104_Connection self, uint8_t* message, int messageSize);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_CS104_CONNECTION_H_ */
