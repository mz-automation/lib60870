/*
 *  Copyright 2016-2018 MZ Automation GmbH
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
#include "tls_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file iec60870_slave.h
 * \brief Common slave side definitions for IEC 60870-5-101/104
 * These types are used by CS101/CS104 slaves
 */

/**
 * @addtogroup SLAVE Slave related functions
 *
 * @{
 */

/**
 * @defgroup COMMON_SLAVE Common slave related functions and interfaces
 *
 * These definitions are used by both the CS 101 and CS 104 slave implementations.
 *
 * @{
 */

/**
 * @defgroup IMASTER_CONNECTION IMasterConnection interface
 *
 * @{
 */

/**
 * \brief Interface to send messages to the master (used by slave)
 */
typedef struct sIMasterConnection* IMasterConnection;

struct sIMasterConnection {
    bool (*isReady) (IMasterConnection self);
    bool (*sendASDU) (IMasterConnection self, CS101_ASDU asdu);
    bool (*sendACT_CON) (IMasterConnection self, CS101_ASDU asdu, bool negative);
    bool (*sendACT_TERM) (IMasterConnection self, CS101_ASDU asdu);
    void (*close) (IMasterConnection self);
    int (*getPeerAddress) (IMasterConnection self, char* addrBuf, int addrBufSize);
    CS101_AppLayerParameters (*getApplicationLayerParameters) (IMasterConnection self);
    void* object;
};

/*
 * \brief Check if the connection is ready to send an ASDU.
 *
 * \deprecated Use one of the send functions (e.g. \ref IMasterConnection_sendASDU) and evaluate
 * the return value.
 *
 * NOTE: The functions returns true when the connection is activated, the ASDU can be immediately sent,
 * or the queue has enough space to store another ASDU.
 *
 * \param self the connection object (this is usually received as a parameter of a callback function)
 *
 * \returns true if the connection is ready to send an ASDU, false otherwise
 */
bool
IMasterConnection_isReady(IMasterConnection self);

/**
 * \brief Send an ASDU to the client/master
 *
 * NOTE: ASDU instance has to be released by the caller!
 *
 * \param self the connection object (this is usually received as a parameter of a callback function)
 * \param asdu the ASDU to send to the client/master
 *
 * \return true when the ASDU has been sent or queued for transmission, false otherwise
 */
bool
IMasterConnection_sendASDU(IMasterConnection self, CS101_ASDU asdu);

/**
 * \brief Send an ACT_CON ASDU to the client/master
 *
 * ACT_CON is used for a command confirmation (positive or negative)
 *
 * \param asdu the ASDU to send to the client/master
 * \param negative value of the negative flag
 *
 * \return true when the ASDU has been sent or queued for transmission, false otherwise
 */
bool
IMasterConnection_sendACT_CON(IMasterConnection self, CS101_ASDU asdu, bool negative);

/**
 * \brief Send an ACT_TERM ASDU to the client/master
 *
 * ACT_TERM is used to indicate that the command execution is complete.
 *
 * \param asdu the ASDU to send to the client/master
 *
 * \return true when the ASDU has been sent or queued for transmission, false otherwise
 */
bool
IMasterConnection_sendACT_TERM(IMasterConnection self, CS101_ASDU asdu);

/**
 * \brief Get the peer address of the master (only for CS 104)
 *
 * \param addrBuf buffer where to store the IP address as string
 * \param addrBufSize the size of the buffer where to store the IP address
 *
 * \return the number of bytes written to the buffer, 0 if function not supported
 */
int
IMasterConnection_getPeerAddress(IMasterConnection self, char* addrBuf, int addrBufSize);

/**
 * \brief Close the master connection (only for CS 104)
 *
 * Allows the slave to actively close a master connection (e.g. when some exception occurs)
 */
void
IMasterConnection_close(IMasterConnection self);

/**
 * \brief Get the application layer parameters used by this connection
 */
CS101_AppLayerParameters
IMasterConnection_getApplicationLayerParameters(IMasterConnection self);

/**
 * @}
 */


/**
 * @defgroup SLAVE_PLUGIN Slave plugin interface
 *
 * Plugin interface to add functionality to the slave (e.g. file server)
 */

typedef enum
{
    CS101_PLUGIN_RESULT_NOT_HANDLED = 0,
    CS101_PLUGIN_RESULT_HANDLED = 1,
    CS101_PLUGIN_RESULT_INVALID_ASDU = 2
} CS101_SlavePlugin_Result;

/**
 * \brief Plugin interface for CS101 or CS104 slaves
 */
typedef struct sCS101_SlavePlugin* CS101_SlavePlugin;

struct sCS101_SlavePlugin
{
    CS101_SlavePlugin_Result (*handleAsdu) (void* parameter, IMasterConnection connection, CS101_ASDU asdu);
    void (*runTask) (void* parameter, IMasterConnection connection);

    void* parameter;
};

/**
 * @}
 */

/**
 * @defgroup CALLBACK_HANDLERS Slave callback handlers
 *
 * Callback handlers to handle events in the slave
 */

/**
 * \brief Handler will be called when a link layer reset CU (communication unit) message is received
 *
 * NOTE: Can be used to empty the ASDU queues
 *
 * \param parameter a user provided parameter
 */
typedef void (*CS101_ResetCUHandler) (void* parameter);

/**
 * \brief Handler for interrogation command (C_IC_NA_1 - 100).
 */
typedef bool (*CS101_InterrogationHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi);

/**
 * \brief Handler for counter interrogation command (C_CI_NA_1 - 101).
 */
typedef bool (*CS101_CounterInterrogationHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, QualifierOfCIC qcc);

/**
 * \brief Handler for read command (C_RD_NA_1 - 102)
 */
typedef bool (*CS101_ReadHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, int ioa);

/**
 * \brief Handler for clock synchronization command (C_CS_NA_1 - 103)
 *
 * This handler will be called whenever a time synchronization command is received.
 * NOTE: The \ref CS104_Slave instance will automatically send an ACT-CON message for the received time sync command.
 *
 * \param[in] parameter user provided parameter
 * \param[in] connection represents the (TCP) connection that received the time sync command
 * \param[in] asdu the received ASDU
 * \param[in,out] the time received with the time sync message. The user can update this time for the ACT-CON message
 *
 * \return true when time synchronization has been successful, false otherwise
 */
typedef bool (*CS101_ClockSynchronizationHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, CP56Time2a newTime);

/**
 * \brief Handler for reset process command (C_RP_NA_1 - 105)
 */
typedef bool (*CS101_ResetProcessHandler ) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qrp);

/**
 * \brief Handler for delay acquisition command (C_CD_NA:1 - 106)
 */
typedef bool (*CS101_DelayAcquisitionHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu, CP16Time2a delayTime);

/**
 * \brief Handler for ASDUs that are not handled by other handlers (default handler)
 */
typedef bool (*CS101_ASDUHandler) (void* parameter, IMasterConnection connection, CS101_ASDU asdu);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */


#ifdef __cplusplus
}
#endif


#endif /* SRC_IEC60750_SLAVE_H_ */
