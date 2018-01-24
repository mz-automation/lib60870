/*
 *  Copyright 2016, 2017 MZ Automation GmbH
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
#include "tls_api.h"

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
    void (*sendASDU) (IMasterConnection self, CS101_ASDU asdu);
    void (*sendACT_CON) (IMasterConnection self, CS101_ASDU asdu, bool negative);
    void (*sendACT_TERM) (IMasterConnection self, CS101_ASDU asdu);
    CS101_AppLayerParameters (*getApplicationLayerParameters) (IMasterConnection self);
    void* object;
};

/**
 * \brief Send an ASDU to the client/master
 *
 * The ASDU will be released by this function after the message is sent.
 * You should not call the ASDU_destroy function for the given ASDU after
 * calling this function!
 *
 * \param self the connection object (this is usually received as a parameter of a callback function)
 * \param asdu the ASDU to send to the client/master
 */
void
IMasterConnection_sendASDU(IMasterConnection self, CS101_ASDU asdu);

/**
 * \brief Send an ACT_CON ASDU to the client/master
 *
 * ACT_CON is used for a command confirmation (positive or negative)
 *
 * \param asdu the ASDU to send to the client/master
 * \param negative value of the negative flag
 */
void
IMasterConnection_sendACT_CON(IMasterConnection self, CS101_ASDU asdu, bool negative);

/**
 * \brief Send an ACT_TERM ASDU to the client/master
 *
 * ACT_TERM is used to indicate that the command execution is complete.
 *
 * \param asdu the ASDU to send to the client/master
 */
void
IMasterConnection_sendACT_TERM(IMasterConnection self, CS101_ASDU asdu);

/**
 * \brief Get the application layer parameters used by this connection
 */
CS101_AppLayerParameters
IMasterConnection_getApplicationLayerParameters(IMasterConnection self);

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
