/*
 *  ASDU.cs
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
using System.Collections.Generic;

namespace lib60870
{

	public class ASDUParsingException : Exception
	{
	

		public ASDUParsingException (string message) : base(message)
		{
		
		}
	}

	public class ASDU
	{
		private ConnectionParameters parameters;

		private TypeID typeId;

		private byte vsq; /* variable structure qualifier */

		private CauseOfTransmission cot; /* cause */
		private byte oa; /* originator address */
		private bool isTest; /* is message a test message */
		private bool isNegative; /* is message a negative confirmation */

		private int ca; /* Common address */

		private byte[] payload = null;
		private List<InformationObject> informationObjects = null;

		public ASDU(TypeID typeId, CauseOfTransmission cot, bool isTest, bool isNegative, byte oa, int ca, bool isSequence) {
			this.typeId = typeId;
			this.cot = cot;
			this.isTest = isTest;
			this.isNegative = isNegative;
			this.oa = oa;
			this.ca = ca;

			if (isSequence)
				this.vsq = 0x80;
			else
				this.vsq = 0;
		}

		public void AddInformationObject(InformationObject io) {
			if (informationObjects == null)
				informationObjects = new List<InformationObject> ();

			informationObjects.Add (io);

			vsq = (byte) ((vsq & 0x80) | informationObjects.Count);
		}

		public ASDU (ConnectionParameters parameters, byte[] msg, int msgLength)
		{
			this.parameters = parameters;

			int bufPos = 6;

			typeId = (TypeID) msg [bufPos++];
			vsq = msg [bufPos++];

			byte cotByte = msg [bufPos++];

			if ((cotByte & 0x80) != 0)
				isTest = true;
			else
				isTest = false;

			if ((cotByte & 0x40) != 0)
				isNegative = true;
			else
				isNegative = false;

			cot = (CauseOfTransmission) (cotByte & 0x3f);

			if (parameters.SizeOfCOT == 2)
				oa = msg [bufPos++];

			ca = msg [bufPos++];

			if (parameters.SizeOfCA > 1) 
				ca += (msg [bufPos++] * 0x100);

			int payloadSize = msgLength - bufPos;

			payload = new byte[payloadSize];

			/* save payload */
			Buffer.BlockCopy (msg, bufPos, payload, 0, payloadSize);
		}

		public void Encode(Frame frame, ConnectionParameters parameters) {
			frame.SetNextByte ((byte)typeId);
			frame.SetNextByte (vsq);

			byte cotByte = (byte) cot;

			if (isTest)
				cotByte = (byte) (cotByte | 0x80);

			if (isNegative)
				cotByte = (byte) (cotByte | 0x40);

			frame.SetNextByte (cotByte);

			if (parameters.SizeOfCOT == 2)
				frame.SetNextByte ((byte)oa);

			frame.SetNextByte ((byte)(ca % 256));

			if (parameters.SizeOfCA > 1) 
				frame.SetNextByte((byte)(ca / 256));

			if (payload != null)
				frame.AppendBytes (payload);
			else {

				foreach (InformationObject io in informationObjects) {
					io.Encode (frame, parameters);
				}
			}

		}

		public TypeID TypeId {
			get {
				return this.typeId;
			}
		}

		public CauseOfTransmission Cot {
			get {
				return this.cot;
			}
			set {
				this.cot = value;
			}
		}

		public byte Oa {
			get {
				return this.oa;
			}
		}

		public bool IsTest {
			get {
				return this.isTest;
			}
		}

		public bool IsNegative {
			get {
				return this.isNegative;
			}
			set {
				isNegative = value;
			}
		}



		public int Ca {
			get {
				return this.ca;
			}
		}

		public bool IsSquence {
			get {
				if ((vsq & 0x80) != 0)
					return true;
				else
					return false;
			}
		}

		public int NumberOfElements {
			get {
				return (vsq & 0x7f);
			}
		}

		public InformationObject GetElement(int index)
		{
			InformationObject retVal = null;

			int elementSize;

			switch (typeId) {

			case TypeID.M_SP_NA_1: /* 1 */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new SinglePointInformation(parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_SP_TA_1: /* 2 */

				elementSize = parameters.SizeOfIOA + 4;

				retVal = new SinglePointWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_DP_NA_1: /* 3 */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new DoublePointInformation (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_DP_TA_1: /* 4 */

				elementSize = parameters.SizeOfIOA + 4;

				retVal = new DoublePointWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ST_NA_1: /* 5 */

				elementSize = parameters.SizeOfIOA + 2;

				retVal = new StepPositionInformation (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_ST_TA_1: /* 6 */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new StepPositionWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_BO_NA_1: /* 7 */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new Bitstring32 (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_BO_TA_1: /* 8 */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new Bitstring32WithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ME_NA_1: /* 9 */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new MeasuredValueNormalized (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_ME_TA_1: /* 10 */

				elementSize = parameters.SizeOfIOA + 6;

				retVal = new MeasuredValueNormalizedWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ME_NB_1: /* 11 */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new MeasuredValueScaled (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_ME_TB_1: /* 12 */

				elementSize = parameters.SizeOfIOA + 6;

				retVal = new MeasuredValueScaledWithCP24Time2a (parameters, payload, index * elementSize);

				break;


			case TypeID.M_ME_NC_1: /* 13 */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new MeasuredValueShort (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_ME_TC_1: /* 14 */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new MeasuredValueShortWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_IT_NA_1: /* 15 */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new IntegratedTotals (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_IT_TA_1: /* 16 */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new IntegratedTotalsWithCP24Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TA_1: /* 17 */

				elementSize = parameters.SizeOfIOA + 6;

				retVal = new EventOfProtectionEquipment (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TB_1: /* 18 */

				elementSize = parameters.SizeOfIOA + 7;

				retVal = new PackedStartEventsOfProtectionEquipment (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TC_1: /* 19 */

				elementSize = parameters.SizeOfIOA + 7;

				retVal = new PacketOutputCircuitInfo (parameters, payload, index * elementSize);

				break;

			case TypeID.M_PS_NA_1: /* 20 */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new PackedSinglePointWithSCD (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			case TypeID.M_ME_ND_1: /* 21 */

				elementSize = parameters.SizeOfIOA + 2;

				retVal = new MeasuredValueNormalizedWithoutQuality (parameters, payload, index * elementSize);

				//TODO add support for Sequence of elements in a single information object (sq = 1)

				break;

			/* 22 - 29 reserved */

			case TypeID.M_SP_TB_1: /* 30 */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new SinglePointWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_DP_TB_1: /* 31 */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new DoublePointWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ST_TB_1: /* 32 */

				elementSize = parameters.SizeOfIOA + 9;

				retVal = new StepPositionWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_BO_TB_1: /* 33 */

				elementSize = parameters.SizeOfIOA + 12;

				retVal = new Bitstring32WithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ME_TD_1: /* 34 */

				elementSize = parameters.SizeOfIOA + 10;

				retVal = new MeasuredValueNormalizedWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ME_TE_1: /* 35 */

				elementSize = parameters.SizeOfIOA + 10;

				retVal = new MeasuredValueScaledWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_ME_TF_1: /* 36 */

				elementSize = parameters.SizeOfIOA + 12;

				retVal = new MeasuredValueShortWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_IT_TB_1: /* 37 */

				elementSize = parameters.SizeOfIOA + 12;

				retVal = new IntegratedTotalsWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TD_1: /* 38 */

				elementSize = parameters.SizeOfIOA + 10;

				retVal = new EventOfProtectionEquipmentWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TE_1: /* 39 */

				elementSize = parameters.SizeOfIOA + 11;

				retVal = new PackedStartEventsOfProtectionEquipmentWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.M_EP_TF_1: /* 40 */

				elementSize = parameters.SizeOfIOA + 11;

				retVal = new PacketOutputCircuitInfoWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			/* 41 - 44 reserved */

			case TypeID.C_SC_NA_1: /* 45 */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new SingleCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_DC_NA_1: /* 46 */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new DoubleCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_RC_NA_1: /* 47 */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new StepCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_NA_1: /* 48 - Set-point command, normalized value */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new SetpointCommandNormalized (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_NB_1: /* 49 - Set-point command, scaled value */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new SetpointCommandScaled (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_NC_1: /* 50 - Set-point command, short floating point number */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new SetpointCommandShortFloat (parameters, payload, index * elementSize);

				break;

			case TypeID.C_BO_NA_1: /* 51 - Bitstring command */

				elementSize = parameters.SizeOfIOA + 4;

				retVal = new Bitstring32Command (parameters, payload, index * elementSize);

				break;

			/* 52 - 57 reserved */

				/* TODO */

			/* 65 - 69 reserved */

			case TypeID.C_IC_NA_1: /* 100 - Interrogation command */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new InterrogationCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_CI_NA_1: /* 101 - Counter interrogation command */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new CounterInterrogationCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_RD_NA_1: /* 102 - Read command */

				elementSize = parameters.SizeOfIOA;

				retVal = new ReadCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_CS_NA_1: /* 103 - Clock synchronization command */

				elementSize = parameters.SizeOfIOA + 7;

				retVal = new ClockSynchronizationCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_RP_NA_1: /* 105 - Reset process command */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new ResetProcessCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_CD_NA_1: /* 106 - Delay acquisition command */

				elementSize = parameters.SizeOfIOA + 2;

				retVal = new DelayAcquisitionCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.P_ME_NA_1: /* 110 - Parameter of measured values, normalized value */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new ParameterNormalizedValue (parameters, payload, index * elementSize);

				break;

			case TypeID.P_ME_NB_1: /* 111 - Parameter of measured values, scaled value */

				elementSize = parameters.SizeOfIOA + 3;

				retVal = new ParameterScaledValue (parameters, payload, index * elementSize);

				break;

			case TypeID.P_ME_NC_1: /* 112 - Parameter of measured values, short floating point number */

				elementSize = parameters.SizeOfIOA + 5;

				retVal = new ParameterFloatValue (parameters, payload, index * elementSize);

				break;

			case TypeID.P_AC_NA_1: /* 113 - Parameter for activation */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new ParameterActivation (parameters, payload, index * elementSize);

				break;

			/* 114 - 119 reserved */

			default:
				throw new ASDUParsingException ("Unknown ASDU type id:" + typeId);
			}

			return retVal;
		}


		public override string ToString()
		{
			string ret;

			ret = "TypeID: " + typeId.ToString() + " COT: " + cot.ToString ();

			if (parameters.SizeOfCOT == 2)
				ret += " OA: " + oa;

			if (isTest)
				ret += " [TEST]";

			if (isNegative)
				ret += " [NEG]";

			if (IsSquence)
				ret += " [SEQ]";

			ret += " elements: " + NumberOfElements;

			ret += " CA: " + ca;

			return ret;
		}
	}
}

