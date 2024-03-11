/*
 *  cs104_slave.h
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

#ifndef SRC_INC_API_CS104_SLAVE_H_
#define SRC_INC_API_CS104_SLAVE_H_

#include "iec60870_slave.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file cs104_slave.h
 * \brief CS 104 slave side definitions
 */

/**
 * @addtogroup SLAVE Slave related functions
 *
 * @{
 */

/**
 * @defgroup CS104_SLAVE CS 104 slave (TCP/IP server) related functions
 *
 * @{
 */

typedef struct sCS104_Slave* CS104_Slave;

typedef enum {
    CS104_MODE_SINGLE_REDUNDANCY_GROUP,
    CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP,
    CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS
} CS104_ServerMode;

typedef enum
{
    IP_ADDRESS_TYPE_IPV4,
    IP_ADDRESS_TYPE_IPV6
} eCS104_IPAddressType;

typedef struct sCS104_RedundancyGroup* CS104_RedundancyGroup;

/**
 * \brief Connection request handler is called when a client tries to connect to the server.
 *
 * \param parameter user provided parameter
 * \param ipAddress string containing IP address and TCP port number (e.g. "192.168.1.1:34521")
 *
 * \return true to accept the connection request, false to deny
 */
typedef bool (*CS104_ConnectionRequestHandler) (void* parameter, const char* ipAddress);

typedef enum {
    CS104_CON_EVENT_CONNECTION_OPENED = 0,
    CS104_CON_EVENT_CONNECTION_CLOSED = 1,
    CS104_CON_EVENT_ACTIVATED = 2,
    CS104_CON_EVENT_DEACTIVATED = 3
} CS104_PeerConnectionEvent;


/**
 * \brief Handler that is called when a peer connection is established or closed, or START_DT/STOP_DT is issued
 *
 * \param parameter user provided parameter
 * \param connection the connection object
 * \param event event type
 */
typedef void (*CS104_ConnectionEventHandler) (void* parameter, IMasterConnection connection, CS104_PeerConnectionEvent event);

/**
 * \brief Callback handler for sent and received messages
 *
 * This callback handler provides access to the raw message buffer of received or sent
 * messages. It can be used for debugging purposes. Usually it is not used nor required
 * for applications.
 *
 * \param parameter user provided parameter
 * \param connection the connection that sent or received the message
 * \param msg the message buffer
 * \param msgSize size of the message
 * \param sent indicates if the message was sent or received
 */
typedef void (*CS104_SlaveRawMessageHandler) (void* parameter, IMasterConnection connection, uint8_t* msg, int msgSize, bool send);


/**
 * \brief Create a new instance of a CS104 slave (server)
 *
 * \param maxLowPrioQueueSize the maximum size of the event queue
 * \param maxHighPrioQueueSize the maximum size of the high-priority queue
 *
 * \return the new slave instance
 */
CS104_Slave
CS104_Slave_create(int maxLowPrioQueueSize, int maxHighPrioQueueSize);

/**
 * \brief Create a new instance of a CS104 slave (server) with TLS enabled
 *
 * \param maxLowPrioQueueSize the maximum size of the event queue
 * \param maxHighPrioQueueSize the maximum size of the high-priority queue
 * \param tlsConfig the TLS configuration object (containing configuration parameters, keys, and certificates)
 *
 * \return the new slave instance
 */
CS104_Slave
CS104_Slave_createSecure(int maxLowPrioQueueSize, int maxHighPrioQueueSize, TLSConfiguration tlsConfig);

void
CS104_Slave_addPlugin(CS104_Slave self, CS101_SlavePlugin plugin);

/**
 * \brief Set the local IP address to bind the server
 * use "0.0.0.0" to bind to all interfaces
 *
 * \param self the slave instance
 * \param ipAddress the IP address string or hostname
 */
void
CS104_Slave_setLocalAddress(CS104_Slave self, const char* ipAddress);

/**
 * \brief Set the local TCP port to bind the server
 *
 * \param self the slave instance
 * \param tcpPort the TCP port to use (default is 2404)
 */
void
CS104_Slave_setLocalPort(CS104_Slave self, int tcpPort);

/**
 * \brief Get the number of connected clients
 *
 * \param self the slave instance
 */
int
CS104_Slave_getOpenConnections(CS104_Slave self);

/**
 * \brief set the maximum number of open client connections allowed
 *
 * NOTE: the number cannot be larger than the static maximum defined in
 *
 * \param self the slave instance
 * \param maxOpenConnections the maximum number of open client connections allowed
 */
void
CS104_Slave_setMaxOpenConnections(CS104_Slave self, int maxOpenConnections);

/**
 * \brief Set one of the server modes
 *
 * \param self the slave instance
 * \param serverMode the server mode (see \ref CS104_ServerMode) to use
 */
void
CS104_Slave_setServerMode(CS104_Slave self, CS104_ServerMode serverMode);

/**
 * \brief Set the connection request handler
 *
 * The connection request handler is called whenever a client/master is trying to connect.
 * This handler can be used to implement access control mechanisms as it allows the user to decide
 * if the new connection is accepted or not.
 *
 * \param self the slave instance
 * \param handler the callback function to be used
 * \param parameter user provided context parameter that will be passed to the callback function (or NULL if not required).
 */
void
CS104_Slave_setConnectionRequestHandler(CS104_Slave self, CS104_ConnectionRequestHandler handler, void* parameter);

/**
 * \brief Set the connection event handler
 *
 * The connection request handler is called whenever a connection event happens. A connection event
 * can be when a client connects or disconnects, or when a START_DT or STOP_DT message is received.
 *
 * \param self the slave instance
 * \param handler the callback function to be used
 * \param parameter user provided context parameter that will be passed to the callback function (or NULL if not required).
 */
void
CS104_Slave_setConnectionEventHandler(CS104_Slave self, CS104_ConnectionEventHandler handler, void* parameter);

void
CS104_Slave_setInterrogationHandler(CS104_Slave self, CS101_InterrogationHandler handler, void*  parameter);

void
CS104_Slave_setCounterInterrogationHandler(CS104_Slave self, CS101_CounterInterrogationHandler handler, void*  parameter);

/**
 * \brief set handler for read request (C_RD_NA_1 - 102)
 */
void
CS104_Slave_setReadHandler(CS104_Slave self, CS101_ReadHandler handler, void* parameter);

void
CS104_Slave_setASDUHandler(CS104_Slave self, CS101_ASDUHandler handler, void* parameter);

void
CS104_Slave_setClockSyncHandler(CS104_Slave self, CS101_ClockSynchronizationHandler handler, void* parameter);

/**
 * \brief Set the raw message callback (called when a message is sent or received)
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS104_Slave_setRawMessageHandler(CS104_Slave self, CS104_SlaveRawMessageHandler handler, void* parameter);

/**
 * \brief Get the APCI parameters instance. APCI parameters are CS 104 specific parameters.
 */
CS104_APCIParameters
CS104_Slave_getConnectionParameters(CS104_Slave self);

/**
 * \brief Get the application layer parameters instance..
 */
CS101_AppLayerParameters
CS104_Slave_getAppLayerParameters(CS104_Slave self);

/**
 * \brief Start the CS 104 slave. The slave (server) will listen on the configured TCP/IP port
 *
 * NOTE: This function will start a thread that handles the incoming client connections.
 * This function requires CONFIG_USE_THREADS = 1 and CONFIG_USE_SEMAPHORES == 1 in lib60870_config.h
 *
 * \param self CS104_Slave instance
 */
void
CS104_Slave_start(CS104_Slave self);

/**
 * \brief Check if slave is running
 *
 * \param self CS104_Slave instance
 *
 * \return true when slave is running, false otherwise
 */
bool
CS104_Slave_isRunning(CS104_Slave self);

/**
 * \brief Stop the server.
 *
 * Stop listening to incoming TCP/IP connections and close all open connections.
 * Event buffers will be deactivated.
 */
void
CS104_Slave_stop(CS104_Slave self);

/**
 * \brief Start the slave (server) in non-threaded mode.
 *
 * Start listening to incoming TCP/IP connections.
 *
 * NOTE: Server should only be started after all configuration is done.
 */
void
CS104_Slave_startThreadless(CS104_Slave self);

/**
 * \brief Stop the server in non-threaded mode
 *
 * Stop listening to incoming TCP/IP connections and close all open connections.
 * Event buffers will be deactivated.
 */
void
CS104_Slave_stopThreadless(CS104_Slave self);

/**
 * \brief Protocol stack tick function for non-threaded mode.
 *
 * Handle incoming connection requests and messages, send buffered events, and
 * handle periodic tasks.
 *
 * NOTE: This function has to be called periodically by the application.
 */
void
CS104_Slave_tick(CS104_Slave self);

/*
 * \brief Gets the number of ASDU in the low-priority queue
 *
 * NOTE: Mode CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP is not supported by this function.
 *
 * \param redGroup the redundancy group to use or NULL for single redundancy mode
 *
 * \return the number of ASDU in the low-priority queue
 */
int
CS104_Slave_getNumberOfQueueEntries(CS104_Slave self, CS104_RedundancyGroup redGroup);

/**
 * \brief Add an ASDU to the low-priority queue of the slave (use for periodic and spontaneous messages)
 *
 * \param asdu the ASDU to add
 */
void
CS104_Slave_enqueueASDU(CS104_Slave self, CS101_ASDU asdu);

/**
 * \brief Add a new redundancy group to the server.
 *
 * A redundancy group is a group of clients that share the same event queue. This function can
 * only be used with server mode CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS.
 *
 * NOTE: Has to be called before the server is started!
 *
 * \param redundancyGroup the new redundancy group
 */
void
CS104_Slave_addRedundancyGroup(CS104_Slave self, CS104_RedundancyGroup redundancyGroup);

/**
 * \brief Delete the slave instance. Release all resources.
 */
void
CS104_Slave_destroy(CS104_Slave self);

/**
 * \brief Create a new redundancy group.
 *
 * A redundancy group is a group of clients that share the same event queue. Redundancy groups can
 * only be used with server mode CS104_MODE_MULTIPLE_REDUNDANCY_GROUPS.
 */
CS104_RedundancyGroup
CS104_RedundancyGroup_create(const char* name);

/**
 * \brief Add an allowed client to the redundancy group
 *
 * \param ipAddress the IP address of the client as C string (can be IPv4 or IPv6 address).
 */
void
CS104_RedundancyGroup_addAllowedClient(CS104_RedundancyGroup self, const char* ipAddress);

/**
 * \brief Add an allowed client to the redundancy group
 *
 * \param ipAddress the IP address as byte buffer (4 byte for IPv4, 16 byte for IPv6)
 * \param addressType type of the IP address (either IP_ADDRESS_TYPE_IPV4 or IP_ADDRESS_TYPE_IPV6)
 */
void
CS104_RedundancyGroup_addAllowedClientEx(CS104_RedundancyGroup self, const uint8_t* ipAddress, eCS104_IPAddressType addressType);

/**
 * \brief Destroy the instance and release all resources.
 *
 * NOTE: This function will be called by \ref CS104_Slave_destroy. After using
 * the \ref CS104_Slave_addRedundancyGroup function the redundancy group object must
 * not be destroyed manually.
 */
void
CS104_RedundancyGroup_destroy(CS104_RedundancyGroup self);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_API_CS104_SLAVE_H_ */
