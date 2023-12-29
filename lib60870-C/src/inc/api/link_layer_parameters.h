/*
 *  link_layer_parameters.h
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

#ifndef SRC_IEC60870_LINK_LAYER_LINK_LAYER_PARAMETERS_H_
#define SRC_IEC60870_LINK_LAYER_LINK_LAYER_PARAMETERS_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file link_layer_parameters.h
 *
 * \brief Parameters for serial link layers
 */

/** \brief Parameters for the IEC 60870-5 link layer */
typedef struct sLinkLayerParameters* LinkLayerParameters;

struct sLinkLayerParameters {
    int addressLength; /** Length of link layer address (1 or 2 byte) */
    int timeoutForAck; /** timeout for link layer ACK in ms */
    int timeoutRepeat; /** timeout for repeated message transmission when no ACK received in ms */
    bool useSingleCharACK; /** use single char ACK for ACK (FC=0) or RESP_NO_USER_DATA (FC=9) */
    int timeoutLinkState; /** interval to repeat request status of link (FC=9) after response timeout */
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_IEC60870_LINK_LAYER_LINK_LAYER_PARAMETERS_H_ */
