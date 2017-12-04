/*
 *  cs101_slave.h
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

#ifndef SRC_IEC60870_CS101_CS101_SLAVE_H_
#define SRC_IEC60870_CS101_CS101_SLAVE_H_

#include "hal_serial.h"
#include "iec60870_common.h"
#include "iec60870_slave.h"
#include "link_layer_parameters.h"

typedef struct sCS101_Slave* CS101_Slave;

/**
 * \brief Create a new balanced or unbalanced CS101 slave
 *
 * The CS101_Slave instance has two separate data queues for class 1 and class 2 data.
 *
 * \param serialPort the serial port to be used
 * \param llParameters the link layer parameters to be used
 * \param alParameters the CS101 application layer parameters
 * \param linkLayerMode the link layer mode (either BALANCED or UNBALANCED)
 *
 * \return the new slave instance
 */
CS101_Slave
CS101_Slave_create(SerialPort serialPort, LinkLayerParameters llParameters, CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode);

/**
 * \brief Destroy the slave instance and cleanup all resources
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_destroy(CS101_Slave self);

/**
 * \brief Set the idle timeout
 *
 * Time with no activity after which the connection is considered
 * in idle (LL_STATE_IDLE) state.
 *
 * \param timeoutInMs the timeout value in milliseconds
 */
void
CS101_Slave_setIdleTimeout(CS101_Slave self, int timeoutInMs);

/**
 * \brief Set a callback handler for link layer state changes
 */
void
CS101_Slave_setLinkLayerStateChanged(CS101_Slave self, IEC60870_LinkLayerStateChangedHandler handler, void* parameter);


/**
 * \brief Set the local link layer address
 *
 * \param self CS101_Slave instance
 * \param address the link layer address (can be either 1 or 2 byte wide).
 */
void
CS101_Slave_setLinkLayerAddress(CS101_Slave self, int address);

/**
 * \brief Check if the class 1 ASDU is full
 *
 * \param self CS101_Slave instance
 *
 * \return true when the queue is full, false otherwise
 */
bool
CS101_Slave_isClass1QueueFull(CS101_Slave self);

/**
 * \brief Enqueue an ASDU into the class 1 data queue
 *
 * \param self CS101_Slave instance
 * \param asdu the ASDU instance to enqueue
 */
void
CS101_Slave_enqueueUserDataClass1(CS101_Slave self, CS101_ASDU asdu);

/**
 * \brief Check if the class 2 ASDU is full
 *
 * \param self CS101_Slave instance
 *
 * \return true when the queue is full, false otherwise
 */
bool
CS101_Slave_isClass2QueueFull(CS101_Slave self);

/**
 * \brief Enqueue an ASDU into the class 2 data queue
 *
 * \param self CS101_Slave instance
 * \param asdu the ASDU instance to enqueue
 */
void
CS101_Slave_enqueueUserDataClass2(CS101_Slave self, CS101_ASDU asdu);

/**
 * \brief Remove all ASDUs from the class 1/2 data queues
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_flushQueues(CS101_Slave self);

/**
 * \brief Receive a new message and run the link layer state machines
 *
 * NOTE: has to be called frequently
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_run(CS101_Slave self);

/**
 * \brief Returns the application layer parameters object of this slave instance
 *
 * \param self CS101_Slave instance
 *
 * \return the CS101_AppLayerParameters instance used by this slave
 */
CS101_AppLayerParameters
CS101_Slave_getAppLayerParameters(CS101_Slave self);

void
CS101_Slave_setResetCUHandler(CS101_Slave self, CS101_ResetCUHandler handler, void* parameter);

void
CS101_Slave_setInterrogationHandler(CS101_Slave self, CS101_InterrogationHandler handler, void*  parameter);

void
CS101_Slave_setCounterInterrogationHandler(CS101_Slave self, CS101_CounterInterrogationHandler handler, void*  parameter);

void
CS101_Slave_setReadHandler(CS101_Slave self, CS101_ReadHandler handler, void* parameter);

void
CS101_Slave_setASDUHandler(CS101_Slave self, CS101_ASDUHandler handler, void* parameter);

void
CS101_Slave_setClockSyncHandler(CS101_Slave self, CS101_ClockSynchronizationHandler handler, void* parameter);

void
CS101_Slave_setResetProcessHandler(CS101_Slave self, CS101_ResetProcessHandler handler, void* parameter);

void
CS101_Slave_setDelayAcquisitionHandler(CS101_Slave self, CS101_DelayAcquisitionHandler handler, void* parameter);

#endif /* SRC_IEC60870_CS101_CS101_SLAVE_H_ */
