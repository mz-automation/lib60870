/*
 *  CauseOfTransmission.cs
 *
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870.NET
 *
 *  lib60870.NET is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870.NET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870.NET.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

using System;

using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace lib60870
{
	public enum CauseOfTransmission {
		PERIODIC = 1, 
		BACKGROUND_SCAN = 2, 
		SPONTANEOUS = 3, 
		INITIALIZED = 4, 
		REQUEST = 5, 
		ACTIVATION = 6, 
		ACTIVATION_CON = 7, 
		DEACTIVATION = 8,
		DEACTIVATION_CON = 9, 
		ACTIVATION_TERMINATION = 10, 
		RETURN_INFO_REMOTE = 11, 
		RETURN_INFO_LOCAL = 12, 
		FILE_TRANSFER =	13,
		AUTHENTICATION = 14,
		MAINTENANCE_OF_AUTH_SESSION_KEY = 15,
		MAINTENANCE_OF_USER_ROLE_AND_UPDATE_KEY = 16,
		INTERROGATED_BY_STATION = 20, 
		INTERROGATED_BY_GROUP_1 = 21, 
		INTERROGATED_BY_GROUP_2 = 22, 
		INTERROGATED_BY_GROUP_3 = 23,
		INTERROGATED_BY_GROUP_4 = 24, 
		INTERROGATED_BY_GROUP_5 = 25, 
		INTERROGATED_BY_GROUP_6 = 26, 
		INTERROGATED_BY_GROUP_7 = 27,
		INTERROGATED_BY_GROUP_8 = 28, 
		INTERROGATED_BY_GROUP_9 = 29, 
		INTERROGATED_BY_GROUP_10 = 30, 
		INTERROGATED_BY_GROUP_11 = 31, 
		INTERROGATED_BY_GROUP_12 = 32, 
		INTERROGATED_BY_GROUP_13 = 33, 
		INTERROGATED_BY_GROUP_14 = 34, 
		INTERROGATED_BY_GROUP_15 = 35, 
		INTERROGATED_BY_GROUP_16 = 36, 
		REQUESTED_BY_GENERAL_COUNTER = 37, 
		REQUESTED_BY_GROUP_1_COUNTER = 38, 
		REQUESTED_BY_GROUP_2_COUNTER = 39,
		REQUESTED_BY_GROUP_3_COUNTER = 40, 
		REQUESTED_BY_GROUP_4_COUNTER = 41, 
		UNKNOWN_TYPE_ID = 44, 
		UNKNOWN_CAUSE_OF_TRANSMISSION =	45,
		UNKNOWN_COMMON_ADDRESS_OF_ASDU = 46, 
		UNKNOWN_INFORMATION_OBJECT_ADDRESS = 47
	}
	
}
