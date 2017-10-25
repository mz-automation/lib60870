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

	/// <summary>
	/// This class represents an application layer message. It contains some generic message information and
	/// one or more InformationObject instances of the same type. It is used to send and receive messages.
	/// </summary>
	public class ASDU
	{

		public const int IEC60870_5_104_MAX_ASDU_LENGTH = 249;

		private ConnectionParameters parameters;

		private TypeID typeId;
		private bool hasTypeId;

		private byte vsq; /* variable structure qualifier */

		private CauseOfTransmission cot; /* cause */
		private byte oa; /* originator address */
		private bool isTest; /* is message a test message */
		private bool isNegative; /* is message a negative confirmation */

		private int ca; /* Common address */
		private int spaceLeft = 0;

		private byte[] payload = null;
		private List<InformationObject> informationObjects = null;


		public ASDU(ConnectionParameters parameters, CauseOfTransmission cot, bool isTest, bool isNegative, byte oa, int ca, bool isSequence) 
			: this(parameters, TypeID.M_SP_NA_1, cot, isTest, isNegative, oa, ca, isSequence) {
			this.hasTypeId = false;
		}
			
		[Obsolete("use constructor with ConnectionParameters and without TypeID instead")]
		public ASDU(CauseOfTransmission cot, bool isTest, bool isNegative, byte oa, int ca, bool isSequence) 
			: this(new ConnectionParameters(), TypeID.M_SP_NA_1, cot, isTest, isNegative, oa, ca, isSequence) {
			this.hasTypeId = false;
		}

		[Obsolete("use constructor with ConnectionParameters and without TypeID instead")]
		public ASDU(TypeID typeId, CauseOfTransmission cot, bool isTest, bool isNegative, byte oa, int ca, bool isSequence) 
			: this(new ConnectionParameters(), typeId, cot, isTest, isNegative, oa, ca, isSequence) {
		}
			
		[Obsolete("use constructor with ConnectionParameters and without TypeID instead")]
		public ASDU(ConnectionParameters parameters, TypeID typeId, CauseOfTransmission cot, bool isTest, bool isNegative, byte oa, int ca, bool isSequence) {
			this.parameters = parameters;
			this.typeId = typeId;
			this.cot = cot;
			this.isTest = isTest;
			this.isNegative = isNegative;
			this.oa = oa;
			this.ca = ca;
			this.spaceLeft = IEC60870_5_104_MAX_ASDU_LENGTH - 
				parameters.SizeOfTypeId - parameters.SizeOfVSQ - parameters.SizeOfCA - parameters.SizeOfCOT;

			if (isSequence)
				this.vsq = 0x80;
			else
				this.vsq = 0;

			this.hasTypeId = true;
		}

		/// <summary>
		/// Adds an information object to the ASDU.
		/// </summary>
		/// This function add an information object (InformationObject) to the ASDU. NOTE: that all information objects
		/// have to be of the same type. Otherwise an ArgumentException will be thrown.
		/// The function returns true when the information object has been added to the ASDU. The function returns false if
		/// there is no space left in the ASDU to add the information object, or when object cannot be added to a sequence
		/// because the IOA does not match.
		/// <returns><c>true</c>, if information object was added, <c>false</c> otherwise.</returns>
		/// <param name="io">The information object to add</param>
		public bool AddInformationObject(InformationObject io) 
		{
			if (informationObjects == null)
				informationObjects = new List<InformationObject> ();

			if (hasTypeId) {
				if (io.Type != typeId)
					throw new ArgumentException ("Invalid information object type: expected " + typeId.ToString () + " was " + io.Type.ToString ());
			} else {
				typeId = io.Type;
				hasTypeId = true;
			}

			if (informationObjects.Count >= 0x7f)
				return false;

			int objectSize = io.GetEncodedSize ();

			if (IsSequence == false)
				objectSize += parameters.SizeOfIOA;
			else {
				if (informationObjects.Count == 0) // is first object?
					objectSize += parameters.SizeOfIOA;
				else {
					if (io.ObjectAddress !=  (informationObjects[0].ObjectAddress + informationObjects.Count))
						return false;
				}
			}
				
			if (objectSize <= spaceLeft) {

				spaceLeft -= objectSize;
				informationObjects.Add (io);

				vsq = (byte)((vsq & 0x80) | informationObjects.Count);

				return true;
			} else
				return false;
		}

		public ASDU (ConnectionParameters parameters, byte[] msg, int bufPos, int msgLength)
		{
			this.parameters = parameters;

			int asduHeaderSize = 2 + parameters.SizeOfCOT + parameters.SizeOfCA;

			if ((msgLength - bufPos) < asduHeaderSize)
				throw new ASDUParsingException ("Message header too small");

			typeId = (TypeID) msg [bufPos++];
			vsq = msg [bufPos++];

			this.hasTypeId = true;

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

			//TODO add plausibility check for payload length (using TypeID, SizeOfIOA, and VSQ)

			payload = new byte[payloadSize];

			/* save payload */
			Buffer.BlockCopy (msg, bufPos, payload, 0, payloadSize);
		}

		internal void Encode(Frame frame, ConnectionParameters parameters) {
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

				bool isFirst = true;

				foreach (InformationObject io in informationObjects) {

					if (isFirst) {
						io.Encode (frame, parameters, false);
						isFirst = false;
					} else {
						if (IsSequence)
							io.Encode (frame, parameters, true);
						else
							io.Encode (frame, parameters, false);
					}

				}
			}
		}

		public byte[] AsByteArray()
		{
			int expectedSize = IEC60870_5_104_MAX_ASDU_LENGTH - spaceLeft;

			BufferFrame frame = new BufferFrame (new byte[expectedSize], 0);

			Encode (frame, parameters);

			if (frame.GetMsgSize () == expectedSize)
				return frame.GetBuffer ();
			else
				return null;
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

		public bool IsSequence {
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

				elementSize = 1;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new SinglePointInformation (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else 
					retVal = new SinglePointInformation (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
					
				break;

			case TypeID.M_SP_TA_1: /* 2 */
				
				elementSize = 4;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new SinglePointWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;
				} else
					retVal = new SinglePointWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_DP_NA_1: /* 3 */

				elementSize = 1;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new DoublePointInformation (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new DoublePointInformation (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_DP_TA_1: /* 4 */

				elementSize = 4;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new DoublePointWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new DoublePointWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ST_NA_1: /* 5 */

				elementSize = 2;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new StepPositionInformation (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new StepPositionInformation (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ST_TA_1: /* 6 */

				elementSize = 5;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new StepPositionWithCP24Time2a (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new StepPositionWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_BO_NA_1: /* 7 */

				elementSize = 5;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new Bitstring32 (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new Bitstring32 (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_BO_TA_1: /* 8 */

				elementSize = 8;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new Bitstring32WithCP24Time2a (parameters, payload,  parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new Bitstring32WithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ME_NA_1: /* 9 */

				elementSize = 3;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueNormalized (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueNormalized (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
					
				break;

			case TypeID.M_ME_TA_1: /* 10 */

				elementSize = 6;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueNormalizedWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueNormalizedWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ME_NB_1: /* 11 */

				elementSize = 3;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueScaled (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueScaled (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ME_TB_1: /* 12 */

				elementSize = 6;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueScaledWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueScaledWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;


			case TypeID.M_ME_NC_1: /* 13 */

				elementSize = 5;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueShort (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueShort (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				
				break;

			case TypeID.M_ME_TC_1: /* 14 */

				elementSize = 8;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueShortWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueShortWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_IT_NA_1: /* 15 */

				elementSize = 5;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new IntegratedTotals (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new IntegratedTotals (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				
				break;

			case TypeID.M_IT_TA_1: /* 16 */

				elementSize = 8;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new IntegratedTotalsWithCP24Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new IntegratedTotalsWithCP24Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_EP_TA_1: /* 17 */

				elementSize = 3;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new EventOfProtectionEquipment (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new EventOfProtectionEquipment (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_EP_TB_1: /* 18 */

				elementSize = 7;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new PackedStartEventsOfProtectionEquipment (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new PackedStartEventsOfProtectionEquipment (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_EP_TC_1: /* 19 */

				elementSize = 7;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new PackedOutputCircuitInfo (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new PackedOutputCircuitInfo (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_PS_NA_1: /* 20 */

				elementSize = 5;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new PackedSinglePointWithSCD (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new PackedSinglePointWithSCD (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				

				break;

			case TypeID.M_ME_ND_1: /* 21 */

				elementSize = 2;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueNormalizedWithoutQuality (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueNormalizedWithoutQuality (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			/* 22 - 29 reserved */

			case TypeID.M_SP_TB_1: /* 30 */

				elementSize = 8;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new SinglePointWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new SinglePointWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_DP_TB_1: /* 31 */

				elementSize = 8;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new DoublePointWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new DoublePointWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ST_TB_1: /* 32 */

				elementSize = 9;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new StepPositionWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new StepPositionWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_BO_TB_1: /* 33 */

				elementSize = 12;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new Bitstring32WithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new Bitstring32WithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ME_TD_1: /* 34 */

				elementSize = 10;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueNormalizedWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueNormalizedWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_ME_TE_1: /* 35 */

				elementSize = 10;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueScaledWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueScaledWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				
				break;

			case TypeID.M_ME_TF_1: /* 36 */

				elementSize = 12;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new MeasuredValueShortWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new MeasuredValueShortWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_IT_TB_1: /* 37 */

				elementSize = 12;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new IntegratedTotalsWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new IntegratedTotalsWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

				break;

			case TypeID.M_EP_TD_1: /* 38 */

				elementSize = 10;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new EventOfProtectionEquipmentWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new EventOfProtectionEquipmentWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				
				break;

			case TypeID.M_EP_TE_1: /* 39 */

				elementSize = 11;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new PackedStartEventsOfProtectionEquipmentWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new PackedStartEventsOfProtectionEquipmentWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);
				
				break;

			case TypeID.M_EP_TF_1: /* 40 */

				elementSize = 11;

				if (IsSequence) {
					int ioa = InformationObject.ParseInformationObjectAddress (parameters, payload, 0);

					retVal = new PackedOutputCircuitInfoWithCP56Time2a (parameters, payload, parameters.SizeOfIOA + (index * elementSize), true);

					retVal.ObjectAddress = ioa + index;

				} else
					retVal = new PackedOutputCircuitInfoWithCP56Time2a (parameters, payload, index * (parameters.SizeOfIOA + elementSize), false);

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

				retVal = new SetpointCommandShort (parameters, payload, index * elementSize);

				break;

			case TypeID.C_BO_NA_1: /* 51 - Bitstring command */

				elementSize = parameters.SizeOfIOA + 4;

				retVal = new Bitstring32Command (parameters, payload, index * elementSize);

				break;

			/* 52 - 57 reserved */

			case TypeID.C_SC_TA_1: /* 58 - Single command with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new SingleCommandWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_DC_TA_1: /* 59 - Double command with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new DoubleCommandWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_RC_TA_1: /* 60 - Step command with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 8;

				retVal = new StepCommandWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_TA_1: /* 61 - Setpoint command, normalized value with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 10;

				retVal = new SetpointCommandNormalizedWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_TB_1: /* 62 - Setpoint command, scaled value with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 10;

				retVal = new SetpointCommandScaledWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_SE_TC_1: /* 63 - Setpoint command, short value with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 12;

				retVal = new SetpointCommandShortWithCP56Time2a (parameters, payload, index * elementSize);

				break;

			case TypeID.C_BO_TA_1: /* 64 - Bitstring command with CP56Time2a */

				elementSize = parameters.SizeOfIOA + 11;

				retVal = new Bitstring32CommandWithCP56Time2a (parameters, payload, index * elementSize);

				break;

                /* 65 - 69 reserved */

            case TypeID.M_EI_NA_1: /* 70 - End of initialization */
                elementSize = parameters.SizeOfCA + 1;

                retVal = new EndOfInitialization(parameters, payload, index * elementSize);

                break;

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

            case TypeID.C_TS_NA_1: /* 104 - Test command */

                elementSize = parameters.SizeOfIOA + 2;

                retVal = new TestCommand(parameters, payload, index * elementSize);

                break;

            case TypeID.C_RP_NA_1: /* 105 - Reset process command */

				elementSize = parameters.SizeOfIOA + 1;

				retVal = new ResetProcessCommand (parameters, payload, index * elementSize);

				break;

			case TypeID.C_CD_NA_1: /* 106 - Delay acquisition command */

				elementSize = parameters.SizeOfIOA + 2;

				retVal = new DelayAcquisitionCommand (parameters, payload, index * elementSize);

				break;

            case TypeID.C_TS_TA_1: /* 107 - Test command with CP56Time2a */

                elementSize = parameters.SizeOfIOA + 9;

                retVal = new TestCommandWithCP56Time2a(parameters, payload, index * elementSize);

                break;

				/* C_TS_TA_1 (107) is handled by the stack automatically */

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

			if (IsSequence)
				ret += " [SEQ]";

			ret += " elements: " + NumberOfElements;

			ret += " CA: " + ca;

			return ret;
		}
	}
}

