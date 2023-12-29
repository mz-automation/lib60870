/*
 *  cs101_slave.h
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

#ifndef SRC_IEC60870_CS101_CS101_SLAVE_H_
#define SRC_IEC60870_CS101_CS101_SLAVE_H_

/**
 * \file cs101_slave.h
 * \brief Functions for CS101_Slave ADT.
 * Can be used to implement a balanced or unbalanced CS 101 slave.
 */

#include "hal_serial.h"
#include "iec60870_common.h"
#include "iec60870_slave.h"
#include "link_layer_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup SLAVE Slave related functions
 *
 * @{
 */


/**
 * @defgroup CS101_SLAVE CS 101 slave (serial link layer) related functions
 *
 * @{
 */


/**
 * \brief CS101_Slave type
 */
typedef struct sCS101_Slave* CS101_Slave;

/**
 * \brief Create a new balanced or unbalanced CS101 slave
 *
 * NOTE: The CS101_Slave instance has two separate data queues for class 1 and class 2 data.
 * This constructor uses the default max queue size for both queues.
 *
 * \param serialPort the serial port to be used
 * \param llParameters the link layer parameters to be used
 * \param alParameters the CS101 application layer parameters
 * \param linkLayerMode the link layer mode (either BALANCED or UNBALANCED)
 *
 * \return the new slave instance
 */
CS101_Slave
CS101_Slave_create(SerialPort serialPort, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode);

/**
 * \brief Create a new balanced or unbalanced CS101 slave
 *
 * NOTE: The CS101_Slave instance has two separate data queues for class 1 and class 2 data.
 *
 * \param serialPort the serial port to be used
 * \param llParameters the link layer parameters to be used
 * \param alParameters the CS101 application layer parameters
 * \param linkLayerMode the link layer mode (either BALANCED or UNBALANCED)
 * \param class1QueueSize size of the class1 data queue
 * \param class2QueueSize size of the class2 data queue
 *
 * \return the new slave instance
 */
CS101_Slave
CS101_Slave_createEx(SerialPort serialPort, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode,
        int class1QueueSize, int class2QueueSize);

/**
 * \brief Destroy the slave instance and cleanup all resources
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_destroy(CS101_Slave self);

/**
 * \brief Set the value of the DIR bit when sending messages (only balanced mode)
 *
 * NOTE: Default value is false (controlled station). In the case of two equivalent stations
 * the value is defined by agreement.
 *
 * \param dir the value of the DIR bit when sending messages
 */
void
CS101_Slave_setDIR(CS101_Slave self, bool dir);

/**
 * \brief Register a plugin instance with this slave instance
 *
 * \param the plugin instance.
 */
void
CS101_Slave_addPlugin(CS101_Slave self, CS101_SlavePlugin plugin);

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
 * \brief Set the link layer address of the remote station
 *
 * \param self CS101_Slave instance
 * \param address the link layer address (can be either 1 or 2 byte wide).
 */
void
CS101_Slave_setLinkLayerAddressOtherStation(CS101_Slave self, int address);

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
 * NOTE: Has to be called frequently, when the start/stop functions are
 * not used. Otherwise it will be called by the background thread.
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_run(CS101_Slave self);

/**
 * \brief Start a background thread that handles the link layer connections
 *
 * NOTE: This requires threads. If you don't want to use a separate thread
 * for the slave instance you have to call the \ref CS101_Slave_run function
 * periodically.
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_start(CS101_Slave self);

/**
 * \brief Stops the background thread that handles the link layer connections
 *
 * \param self CS101_Slave instance
 */
void
CS101_Slave_stop(CS101_Slave self);

/**
 * \brief Returns the application layer parameters object of this slave instance
 *
 * \param self CS101_Slave instance
 *
 * \return the CS101_AppLayerParameters instance used by this slave
 */
CS101_AppLayerParameters
CS101_Slave_getAppLayerParameters(CS101_Slave self);

/**
 * \brief Returns the link layer parameters object of this slave instance
 *
 * \param self CS101_Slave instance
 *
 * \return the LinkLayerParameters instance used by this slave
 */
LinkLayerParameters
CS101_Slave_getLinkLayerParameters(CS101_Slave self);

/**
 * \brief Set the handler for the reset CU (communication unit) message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setResetCUHandler(CS101_Slave self, CS101_ResetCUHandler handler, void* parameter);

/**
 * \brief Set the handler for the general interrogation message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setInterrogationHandler(CS101_Slave self, CS101_InterrogationHandler handler, void*  parameter);

/**
 * \brief Set the handler for the counter interrogation message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setCounterInterrogationHandler(CS101_Slave self, CS101_CounterInterrogationHandler handler, void*  parameter);

/**
 * \brief Set the handler for the read message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setReadHandler(CS101_Slave self, CS101_ReadHandler handler, void* parameter);

/**
 * \brief Set the handler for the clock synchronization message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setClockSyncHandler(CS101_Slave self, CS101_ClockSynchronizationHandler handler, void* parameter);

/**
 * \brief Set the handler for the reset process message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setResetProcessHandler(CS101_Slave self, CS101_ResetProcessHandler handler, void* parameter);

/**
 * \brief Set the handler for the delay acquisition message
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setDelayAcquisitionHandler(CS101_Slave self, CS101_DelayAcquisitionHandler handler, void* parameter);

/**
 * \brief Set the handler for a received ASDU
 *
 * NOTE: This a generic handler that will only be called when the ASDU has not been handled by
 * one of the other callback handlers.
 *
 * \param handler the callback handler function
 * \param parameter user provided parameter to be passed to the callback handler
 */
void
CS101_Slave_setASDUHandler(CS101_Slave self, CS101_ASDUHandler handler, void* parameter);

/**
 * \brief Set the raw message callback (called when a message is sent or received)
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS101_Slave_setRawMessageHandler(CS101_Slave self, IEC60870_RawMessageHandler handler, void* parameter);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* SRC_IEC60870_CS101_CS101_SLAVE_H_ */
