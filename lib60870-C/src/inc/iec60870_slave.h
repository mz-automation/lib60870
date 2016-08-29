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

typedef struct sMasterConnection* MasterConnection;

typedef struct sMaster* Master;
typedef struct sT104Master* T104Master;

typedef struct sIEC60750_Server* IEC60750_Server;

typedef struct sConnectionParameters* ConnectionParameters;

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
typedef bool (*CounterInterrogationHandler) (void* parameter, MasterConnection connection, ASDU asdu, uint8_t qoi);

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

Master
T104Master_create(ConnectionParameters parameters);

int
T104Master_getActiveConnections(T104Master self);

void
Master_setInterrogationHandler(Master self, InterrogationHandler handler, void*  parameter);

/**
 * \brief set handler for read request (C_RD_NA_1 - 102)
 */
void
Master_setReadHandler(Master self, ReadHandler handler, void* parameter);

void
Master_setASDUHandler(Master self, ASDUHandler handler, void* parameter);

void
Master_start(Master self);

bool
Master_isRunning(Master self);

void
Master_stop(Master self);

void
Master_enqueueASDU(Master self, ASDU asdu);

void
Master_destroy(Master self);


void
MasterConnection_sendASDU(MasterConnection self, ASDU asdu);




#endif /* SRC_IEC60750_SLAVE_H_ */
