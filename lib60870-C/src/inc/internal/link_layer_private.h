/*
 *  link_layer_private.h
 *
 *  Copyright 2017-2024 Michael Zillgith
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

#ifndef SRC_IEC60870_LINK_LAYER_LINK_LAYER_PRIVATE_H_
#define SRC_IEC60870_LINK_LAYER_LINK_LAYER_PRIVATE_H_


/* primary to secondary function codes */
#define LL_FC_00_RESET_REMOTE_LINK 0
#define LL_FC_01_RESET_USER_PROCESS 1
#define LL_FC_02_TEST_FUNCTION_FOR_LINK 2
#define LL_FC_03_USER_DATA_CONFIRMED 3
#define LL_FC_04_USER_DATA_NO_REPLY 4
#define LL_FC_07_RESET_FCB 7
#define LL_FC_08_REQUEST_FOR_ACCESS_DEMAND 8
#define LL_FC_09_REQUEST_LINK_STATUS 9
#define LL_FC_10_REQUEST_USER_DATA_CLASS_1 10
#define LL_FC_11_REQUEST_USER_DATA_CLASS_2 11

/* secondary to primary function codes */
#define LL_FC_00_ACK 0
#define LL_FC_01_NACK 1
#define LL_FC_08_RESP_USER_DATA 8
#define LL_FC_09_RESP_NACK_NO_DATA 9
#define LL_FC_11_STATUS_OF_LINK_OR_ACCESS_DEMAND 11
#define LL_FC_14_SERVICE_NOT_FUNCTIONING 14
#define LL_FC_15_SERVICE_NOT_IMPLEMENTED 15

#endif /* SRC_IEC60870_LINK_LAYER_LINK_LAYER_PRIVATE_H_ */
