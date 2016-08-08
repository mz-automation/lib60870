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

#ifndef SRC_IEC60750_SERVER_H_
#define SRC_IEC60750_SERVER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct sServerConnection* ServerConnection;

typedef struct sIEC60750_Server* IEC60750_Server;

typedef struct sConnectionParameters* ConnectionParameters;

/**
 *\brief default handler for ASDUs received by a client connection
 */
typedef bool (*ASDUHandler) (void* parameter, ServerConnection connection, ASDU asdu);

IEC60750_Server
IEC60750_5_104_Server_create(ConnectionParameters connectionParameters);

void
IEC60750_Server_setASDUHandler(IEC60750_Server self, ASDUHandler asduHandler, void* parameter);

void
IEC60750_Server_start(IEC60750_Server self);

void
IEC60750_Server_stop(IEC60750_Server self);


#endif /* SRC_IEC60750_SERVER_H_ */
