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

#ifndef SRC_INC_INTERNAL_LIB60870_INTERNAL_H_
#define SRC_INC_INTERNAL_LIB60870_INTERNAL_H_

#include "lib60870_config.h"

void
lib60870_debug_print(const char *format, ...);

#if (CONFIG_DEBUG_OUTPUT == 1)
#define DEBUG_PRINT(...)  do{ lib60870_debug_print(__VA_ARGS__ ); } while( false )
#else
#define DEBUG_PRINT(...) do{ } while ( false )
#endif

#define IEC60870_5_104_MAX_ASDU_LENGTH 249
#define IEC60870_5_104_APCI_LENGTH 6

#define UNUSED_PARAMETER(x) (void)(x)

#endif /* SRC_INC_INTERNAL_LIB60870_INTERNAL_H_ */
