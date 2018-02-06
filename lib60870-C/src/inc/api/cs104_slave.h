/*
 *  cs104_slave.h
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
    CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP
} CS104_ServerMode;

/**
 * \brief Connection request handler is called when a client tries to connect to the server.
 *
 * \param parameter user provided parameter
 * \param ipAddress string containing IP address and TCP port number (e.g. "192.168.1.1:34521")
 *
 * \return true to accept the connection request, false to deny
 */
typedef bool (*CS104_ConnectionRequestHandler) (void* parameter, const char* ipAddress);

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

void
CS104_Slave_setServerMode(CS104_Slave self, CS104_ServerMode serverMode);

void
CS104_Slave_setConnectionRequestHandler(CS104_Slave self, CS104_ConnectionRequestHandler handler, void* parameter);

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

CS104_APCIParameters
CS104_Slave_getConnectionParameters(CS104_Slave self);

CS101_AppLayerParameters
CS104_Slave_getAppLayerParameters(CS104_Slave self);

/**
 * \brief State the CS 104 slave. The slave (server) will listen on the configured TCP/IP port
 *
 * \param self CS104_Slave instance
 */
void
CS104_Slave_start(CS104_Slave self);

bool
CS104_Slave_isRunning(CS104_Slave self);

void
CS104_Slave_stop(CS104_Slave self);

/**
 * \brief Add an ASDU to the low-priority queue of the slave (use for periodic and spontaneous messages)
 *
 * \param asdu the ASDU to add
 */
void
CS104_Slave_enqueueASDU(CS104_Slave self, CS101_ASDU asdu);

void
CS104_Slave_destroy(CS104_Slave self);

/**
 * @}
 */

/**
 * @}
 */

//TODO move to internal
typedef struct sMasterConnection* MasterConnection;

//TODO move to internal
void
MasterConnection_close(MasterConnection self);

//TODO move to internal
void
MasterConnection_deactivate(MasterConnection self);

#ifdef __cplusplus
}
#endif


#endif /* SRC_INC_API_CS104_SLAVE_H_ */
