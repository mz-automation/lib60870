/*
 *  Copyright 2016 MZ Automation GmbH
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

#ifndef SRC_IEC60750_SLAVE_H_
#define SRC_IEC60750_SLAVE_H_

#include <stdint.h>
#include <stdbool.h>
#include "iec60870_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sMasterConnection* MasterConnection;

typedef struct sSlave* Slave;
typedef struct sT104Slave* T104Slave;

typedef struct sIEC60750_Server* IEC60750_Server;

/**
 * Callback handlers for master requests handling
 */

/**
 * \brief Handler for interrogation command (C_IC_NA_1 - 100).
 */
typedef bool (*InterrogationHandler) (void* parameter, MasterConnection connection, ASDU asdu, uint8_t qoi);

/**
 * \brief Handler for counter interrogation command (C_CI_NA_1 - 101).
 */
typedef bool (*CounterInterrogationHandler) (void* parameter, MasterConnection connection, ASDU asdu, QualifierOfCIC qcc);

/**
 * \brief Handler for read command (C_RD_NA_1 - 102)
 */
typedef bool (*ReadHandler) (void* parameter, MasterConnection connection, ASDU asdu, int ioa);

/**
 * \brief Handler for clock synchronization command (C_CS_NA_1 - 103)
 */
typedef bool (*ClockSynchronizationHandler) (void* parameter, MasterConnection connection, ASDU asdu, CP56Time2a newTime);

/**
 * \brief Handler for reset process command (C_RP_NA_1 - 105)
 */
typedef bool (*ResetProcessHandler ) (void* parameter, MasterConnection connection, ASDU asdu, uint8_t qrp);

/**
 * \brief Handler for delay acquisition command (C_CD_NA:1 - 106)
 */
typedef bool (*DelayAcquisitionHandler) (void* parameter, MasterConnection connection, ASDU asdu, CP16Time2a delayTime);

/**
 * \brief Handler for ASDUs that are not handled by other handlers (default handler)
 */
typedef bool (*ASDUHandler) (void* parameter, MasterConnection connection, ASDU asdu);


Slave
T104Slave_create(ConnectionParameters parameters, int maxQueueSize, int maxHighPrioQueueSize);

/**
 * \brief Set the local IP address to bind the server
 * use "0.0.0.0" to bind to all interfaces
 *
 * \param self the slave instance
 * \param ipAddress the IP address string or hostname
 */
void
T104Slave_setLocalAddress(Slave self, const char* ipAddress);

/**
 * \brief Set the local TCP port to bind the server
 *
 * \param self the slave instance
 * \param tcpPort the TCP port to use (default is 2404)
 */
void
T104Slave_setLocalPort(Slave self, int tcpPort);

int
T104Slave_getOpenConnections(Slave self);

void
Slave_setInterrogationHandler(Slave self, InterrogationHandler handler, void*  parameter);

void
Slave_setCounterInterrogationHandler(Slave self, CounterInterrogationHandler handler, void*  parameter);

/**
 * \brief set handler for read request (C_RD_NA_1 - 102)
 */
void
Slave_setReadHandler(Slave self, ReadHandler handler, void* parameter);

void
Slave_setASDUHandler(Slave self, ASDUHandler handler, void* parameter);

void
Slave_setClockSyncHandler(Slave self, ClockSynchronizationHandler handler, void* parameter);

ConnectionParameters
Slave_getConnectionParameters(Slave self);

void
Slave_start(Slave self);

bool
Slave_isRunning(Slave self);

void
Slave_stop(Slave self);

/**
 * \brief Add an ASDU to the low-priority queue of the slave (use for periodic and spontaneous messages)
 *
 * \param asdu the ASDU to add
 */
void
Slave_enqueueASDU(Slave self, ASDU asdu);

void
Slave_destroy(Slave self);

/**
 * \brief Send an ASDU to the client/master
 *
 * The ASDU will be released by this function after the message is sent.
 * You should not call the ASDU_destroy function for the given ASDU after
 * calling this function!
 *
 * \param self the connection object (this is usually received as a parameter of a callback function)
 * \param asdu the ASDU to send to the client/master
 *
 * \return true if message was sent (queued), false otherwise (queue full or connection not active)
 */
bool
MasterConnection_sendASDU(MasterConnection self, ASDU asdu);

bool
MasterConnection_sendACT_CON(MasterConnection self, ASDU asdu, bool negative);

bool
MasterConnection_sendACT_TERM(MasterConnection self, ASDU asdu);

#ifdef __cplusplus
}
#endif


#endif /* SRC_IEC60750_SLAVE_H_ */
