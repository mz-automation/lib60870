/*
 *  cs101_master.h
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

/**
 * \file cs101_master.h
 * \brief Functions for CS101_Master ADT.
 * Can be used to implement a balanced or unbalanced CS 101 master.
 */

#ifndef SRC_INC_API_CS101_MASTER_H_
#define SRC_INC_API_CS101_MASTER_H_

#include "iec60870_master.h"
#include "link_layer_parameters.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup MASTER Master related functions
 *
 * @{
 */

/**
 * @defgroup CS101_MASTER CS 101 master related functions
 *
 * @{
 */

/**
 * \brief CS101_Master type
 */
typedef struct sCS101_Master* CS101_Master;

/**
 * \brief Create a new master instance
 *
 * \param port the serial port to use
 * \param llParameters the link layer parameters to use
 * \param alParameters the application layer parameters to use
 * \param mode the link layer mode (either IEC60870_LINK_LAYER_BALANCED or IEC60870_LINK_LAYER_UNBALANCED)
 *
 * \return the new CS101_Master instance
 */
CS101_Master
CS101_Master_create(SerialPort port, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode mode);

/**
 * \brief Create a new master instance and specify message queue size (for balanced mode)
 *
 * \param port the serial port to use
 * \param llParameters the link layer parameters to use
 * \param alParameters the application layer parameters to use
 * \param mode the link layer mode (either IEC60870_LINK_LAYER_BALANCED or IEC60870_LINK_LAYER_UNBALANCED)
 * \param queueSize set the message queue size (only for balanced mode)
 *
 * \return the new CS101_Master instance
 */
CS101_Master
CS101_Master_createEx(SerialPort serialPort, const LinkLayerParameters llParameters, const CS101_AppLayerParameters alParameters, IEC60870_LinkLayerMode linkLayerMode,
        int queueSize);

/**
 * \brief Receive a new message and run the protocol state machine(s).
 *
 * NOTE: This function has to be called frequently in order to send and
 * receive messages to and from slaves.
 */
void
CS101_Master_run(CS101_Master self);

/**
 * \brief Start a background thread that handles the link layer connections
 *
 * NOTE: This requires threads. If you don't want to use a separate thread
 * for the master instance you have to call the \ref CS101_Master_run function
 * periodically.
 *
 * \param self CS101_Master instance
 */
void
CS101_Master_start(CS101_Master self);

/**
 * \brief Stops the background thread that handles the link layer connections
 *
 * \param self CS101_Master instance
 */
void
CS101_Master_stop(CS101_Master self);

/**
 * \brief Add a new slave connection
 *
 * This function creates and starts a new link layer state machine
 * to be used for communication with the slave. It has to be called
 * before any application data can be send/received to/from the slave.
 *
 * \param address link layer address of the slave
 */
void
CS101_Master_addSlave(CS101_Master self, int address);


/**
 * \brief Poll a slave (only unbalanced mode)
 *
 * NOTE: This command will instruct the unbalanced link layer to send a
 * request for class 2 data. It is required to frequently call this
 * message for each slave in order to receive application layer data from
 * the slave
 *
 * \param address the link layer address of the slave
 */
void
CS101_Master_pollSingleSlave(CS101_Master self, int address);

/**
 * \brief Destroy the master instance and release all resources
 */
void
CS101_Master_destroy(CS101_Master self);

/**
 * \brief Set the value of the DIR bit when sending messages (only balanced mode)
 *
 * NOTE: Default value is true (controlling station). In the case of two equivalent stations
 * the value is defined by agreement.
 *
 * \param dir the value of the DIR bit when sending messages
 */
void
CS101_Master_setDIR(CS101_Master self, bool dir);

/**
 * \brief Set the own link layer address (only balanced mode)
 *
 * \param address the link layer address to use
 */
void
CS101_Master_setOwnAddress(CS101_Master self, int address);

/**
 * \brief Set the slave address for the following send functions
 *
 * NOTE: This is always required in unbalanced mode. Some balanced slaves
 * also check the link layer address. In this case the slave address
 * has also to be set in balanced mode.
 *
 * \param address the link layer address of the slave to address
 */
void
CS101_Master_useSlaveAddress(CS101_Master self, int address);



/**
 * \brief Returns the application layer parameters object of this master instance
 *
 * \return the CS101_AppLayerParameters instance used by this master
 */
CS101_AppLayerParameters
CS101_Master_getAppLayerParameters(CS101_Master self);

/**
 * \brief Returns the link layer parameters object of this master instance
 *
 * \return the LinkLayerParameters instance used by this master
 */
LinkLayerParameters
CS101_Master_getLinkLayerParameters(CS101_Master self);

/**
 * \brief Is the channel ready to transmit an ASDU (only unbalanced mode)
 *
 * The function will return true when the channel (slave) transmit buffer
 * is empty.
 *
 * \param address slave address of the recipient
 *
 * \return true, if channel ready to send a new ASDU, false otherwise
 */
bool
CS101_Master_isChannelReady(CS101_Master self, int address);

/**
 * \brief Manually send link layer test function.
 *
 * Together with the IEC60870_LinkLayerStateChangedHandler this function can
 * be used to ensure that the link is working correctly
 */
void
CS101_Master_sendLinkLayerTestFunction(CS101_Master self);

/**
 * \brief send an interrogation command
 *
 * \param cot cause of transmission
 * \param ca Common address of the slave/server
 * \param qoi qualifier of interrogation (20 for station interrogation)
 */
void
CS101_Master_sendInterrogationCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, QualifierOfInterrogation qoi);

/**
 * \brief send a counter interrogation command
 *
 * \param cot cause of transmission
 * \param ca Common address of the slave/server
 * \param qcc
 */
void
CS101_Master_sendCounterInterrogationCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, uint8_t qcc);

/**
 * \brief  Sends a read command (C_RD_NA_1 typeID: 102)
 *
 * This will send a read command C_RC_NA_1 (102) to the slave/outstation. The COT is always REQUEST (5).
 * It is used to implement the cyclical polling of data application function.
 *
 * \param ca Common address of the slave/server
 * \param ioa Information object address of the data point to read
 */
void
CS101_Master_sendReadCommand(CS101_Master self, int ca, int ioa);

/**
 * \brief Sends a clock synchronization command (C_CS_NA_1 typeID: 103)
 *
 * \param ca Common address of the slave/server
 * \param time new system time for the slave/server
 */
void
CS101_Master_sendClockSyncCommand(CS101_Master self, int ca, CP56Time2a time);

/**
 * \brief Send a test command (C_TS_NA_1 typeID: 104)
 *
 * Note: This command is not supported by IEC 60870-5-104
 *
 * \param ca Common address of the slave/server
 */
void
CS101_Master_sendTestCommand(CS101_Master self, int ca);

/**
 * \brief Send a process command to the controlled (or other) station
 *
 * \param cot the cause of transmission (should be ACTIVATION to select/execute or ACT_TERM to cancel the command)
 * \param ca the common address of the information object
 * \param command the command information object (e.g. SingleCommand or DoubleCommand)
 *
 */
void
CS101_Master_sendProcessCommand(CS101_Master self, CS101_CauseOfTransmission cot, int ca, InformationObject command);


/**
 * \brief Send a user specified ASDU
 *
 * This function can be used for any kind of ASDU types. It can
 * also be used for monitoring messages in reverse direction.
 *
 * NOTE: The ASDU is put into a message queue and will be sent whenever
 * the link layer state machine is able to transmit the ASDU. The ASDUs will
 * be sent in the order they are put into the queue.
 *
 * \param asdu the ASDU to send
 */
void
CS101_Master_sendASDU(CS101_Master self, CS101_ASDU asdu);

/**
 * \brief Register a callback handler for received ASDUs
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS101_Master_setASDUReceivedHandler(CS101_Master self, CS101_ASDUReceivedHandler handler, void* parameter);

/**
 * \brief Set a callback handler for link layer state changes
 */
void
CS101_Master_setLinkLayerStateChanged(CS101_Master self, IEC60870_LinkLayerStateChangedHandler handler, void* parameter);

/**
 * \brief Set the raw message callback (called when a message is sent or received)
 *
 * \param handler user provided callback handler function
 * \param parameter user provided parameter that is passed to the callback handler
 */
void
CS101_Master_setRawMessageHandler(CS101_Master self, IEC60870_RawMessageHandler handler, void* parameter);

/**
 * \brief Set the idle timeout (only for balanced mode)
 *
 * Time with no activity after which the connection is considered
 * in idle (LL_STATE_IDLE) state.
 *
 * \param timeoutInMs the timeout value in milliseconds
 */
void
CS101_Master_setIdleTimeout(CS101_Master self, int timeoutInMs);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_API_CS101_MASTER_H_ */
