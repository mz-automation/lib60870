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

#include "iec60870_common.h"
#include "lib60870_internal.h"

#include <stdio.h>
#include <stdarg.h>

#if (CONFIG_DEBUG_OUTPUT == 1)
static bool debugOutputEnabled = 1;
static Lib60870_DebugHandler debugHandler = NULL;
#endif

void
lib60870_debug_print(const char *format, ...)
{
#if (CONFIG_DEBUG_OUTPUT == 1)
    if (debugOutputEnabled)
    {
        char buffer[CONFIG_DEBUG_OUTPUT_BUFFER_SIZE] = "";
        va_list ap;
        va_start(ap, format);
        vsnprintf(buffer, CONFIG_DEBUG_OUTPUT_BUFFER_SIZE, format, ap);
        va_end(ap);

        if (debugHandler)
        {
            debugHandler("DEBUG_LIB60870: ", buffer);
        }
        else
        {
            printf("DEBUG_LIB60870: %s", buffer);
        }
    }
#else
    UNUSED_PARAMETER(format);
#endif
}

void
Lib60870_enableDebugOutput(bool value)
{
#if (CONFIG_DEBUG_OUTPUT == 1)
    debugOutputEnabled = value;
#else
    UNUSED_PARAMETER(value);
#endif
}

void
Lib60870_setDebugOutputHanlder(const Lib60870_DebugHandler handler)
{
#if (CONFIG_DEBUG_OUTPUT == 1)
    debugHandler = handler;
#else
    UNUSED_PARAMETER(handler);
#endif
}

Lib60870VersionInfo
Lib60870_getLibraryVersionInfo()
{
    Lib60870VersionInfo versionInfo;

    versionInfo.major = LIB60870_VERSION_MAJOR;
    versionInfo.minor = LIB60870_VERSION_MINOR;
    versionInfo.patch = LIB60870_VERSION_PATCH;

    return versionInfo;
}
