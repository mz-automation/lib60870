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


#ifndef SRC_INC_INTERNAL_CS101_ASDU_INTERNAL_H_
#define SRC_INC_INTERNAL_CS101_ASDU_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "iec60870_common.h"
#include "information_objects_internal.h"
#include "apl_types_internal.h"
#include "lib_memory.h"
#include "lib60870_internal.h"

struct sCS101_ASDU {
    CS101_AppLayerParameters parameters;
    uint8_t* asdu;
    int asduHeaderLength;
    uint8_t* payload;
    int payloadSize;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Create a new ASDU instance from a buffer containing the raw ASDU message bytes
 *
 * \param asdu pointer to the statically allocated data structure or NULL to allocate ASDU dynamically
 * \param parameters the application layer parameters used to encode the ASDU
 * \param msg the buffer containing the raw ASDU message bytes
 * \param msgLength the length of the message
 *
 * \return the created ASDU instance or NULL if the message is invalid
 */
CS101_ASDU
CS101_ASDU_createFromBufferEx(CS101_ASDU asdu, CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength);

#ifdef __cplusplus
}
#endif

#endif /* SRC_INC_INTERNAL_CS101_ASDU_INTERNAL_H_ */
