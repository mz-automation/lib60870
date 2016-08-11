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


#include <stdlib.h>

#include "iec60870_slave.h"
#include "hal_socket.h"

struct sMaster {
    struct sConnectionParameters parameters;
};

Master
T104Master_create(ConnectionParameters parameters)
{
    return NULL;
}

int
T104Master_getActiveConnections(T104Master self);

void
Master_setInterrogationHandler(Master self, InterrogationHandler handler, void*  parameter);

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

Master_destroy(Master self);
