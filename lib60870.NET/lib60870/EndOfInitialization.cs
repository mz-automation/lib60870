/*
 *  EndOfInitialization.cs
 *
 *  Copyright 2017 MZ Automation GmbH
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

namespace lib60870
{
    public class EndOfInitialization : InformationObject
    {
        private byte coi;

        /// <summary>
        /// Cause of Initialization (COI)
        /// </summary>
        public byte COI
        {
            get
            {
                return coi;
            }
            set
            {
                coi = value;
            }
        }

        override public int GetEncodedSize()
        {
            return 1;
        }

        override public TypeID Type
        {
            get
            {
                return TypeID.M_EI_NA_1;
            }
        }

        override public bool SupportsSequence
        {
            get
            {
                return false;
            }
        }

        public EndOfInitialization(byte coi)
            : base(0)
        {
            this.coi = coi;
        }


        internal EndOfInitialization(ConnectionParameters parameters, byte[] msg, int startIndex) :
            base(parameters, msg, startIndex, false)
        {
            startIndex += parameters.SizeOfIOA; /* skip IOA */

            coi = msg[startIndex];
        }

        internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence)
        {
            base.Encode(frame, parameters, isSequence);

            frame.SetNextByte(coi);
        }
    }

}
