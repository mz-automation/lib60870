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

#ifndef SRC_INC_INFORMATION_OBJECTS_H_
#define SRC_INC_INFORMATION_OBJECTS_H_

typedef uint8_t QualityDescriptor;


typedef enum  {
    IEC60870_DOUBLE_POINT_INTERMEDIATE = 0,
    IEC60870_DOUBLE_POINT_OFF = 1,
    IEC60870_DOUBLE_POINT_ON = 2,
    IEC60870_DOUBLE_POINT_INDETERMINATE = 3
} DoublePointValue;

/************************************************
 * InformationObject
 ************************************************/

typedef struct sInformationObject* InformationObject;

/**
 * \brief Destroy object - free all related resources
 *
 * This is a virtual function that calls the destructor from the implementation class
 *
 * \self the InformationObject instance
 */
void
InformationObject_destroy(InformationObject self);

/************************************************
 * SinglePointInformation (:InformationObject)
 ************************************************/

typedef struct sSinglePointInformation* SinglePointInformation;

int
SinglePointInformation_getObjectAddress(SinglePointInformation self);

bool
SinglePointInformation_getValue(SinglePointInformation self);

QualityDescriptor
SinglePointInformation_getQuality(SinglePointInformation self);

void
SinglePointInformation_destroy(SinglePointInformation self);

/********************************************************
 *  SinglePointWithCP24Time2a (:SinglePointInformation)
 ********************************************************/

typedef struct sSinglePointWithCP24Time2a* SinglePointWithCP24Time2a;

/********************************************************
 *  SinglePointWithCP56Time2a (:SinglePointInformation)
 ********************************************************/

typedef struct sSinglePointWithCP56Time2a* SinglePointWithCP56Time2a;


/************************************************
 * DoublePointInformation (:InformationObject)
 ************************************************/

typedef struct sDoublePointInformation* DoublePointInformation;

/********************************************************
 *  DoublePointWithCP24Time2a (:DoublePointInformation)
 ********************************************************/

typedef struct sDoublePointWithCP24Time2a* DoublePointWithCP24Time2a;

/********************************************************
 *  DoublePointWithCP56Time2a (:DoublePointInformation)
 ********************************************************/

typedef struct sDoublePointWithCP56Time2a* DoublePointWithCP56Time2a;

/************************************************
 * StepPositionInformation (:InformationObject)
 ************************************************/

typedef struct sStepPositionInformation* StepPositionInformation;

void
StepPositionInformation_initialize(StepPositionInformation self);

void
StepPositionInformation_destroy(StepPositionInformation self);

int
StepPositionInformation_getObjectAddress(StepPositionInformation self);

/**
 * \brief Step position (range -64 ... +63)
 */
int
StepPositionInformation_getValue(StepPositionInformation self);

bool
StepPositionInformation_isTransient(StepPositionInformation self);

QualityDescriptor
StepPositionInformation_getQuality(StepPositionInformation self);

/*********************************************************
 * StepPositionWithCP24Time2a (:StepPositionInformation)
 *********************************************************/

typedef struct sStepPositionWithCP24Time2a* StepPositionWithCP24Time2a;

void
StepPositionWithCP24Time2a_initialize(StepPositionWithCP24Time2a self);

void
StepPositionWithCP24Time2a_destroy(StepPositionWithCP24Time2a self);

CP24Time2a
StepPositionWithCP24Time2a_getTimestamp(StepPositionWithCP24Time2a self);


/*********************************************************
 * StepPositionWithCP56Time2a (:StepPositionInformation)
 *********************************************************/

typedef struct sStepPositionWithCP56Time2a* StepPositionWithCP56Time2a;

void
StepPositionWithCP56Time2a_initialize(StepPositionWithCP56Time2a self);

void
StepPositionWithCP56Time2a_destroy(StepPositionWithCP56Time2a self);

CP56Time2a
StepPositionWithCP56Time2a_getTimestamp(StepPositionWithCP56Time2a self);

/**********************************************
 * MeasuredValueScaledWithCP56Time2a
 **********************************************/

typedef struct sMeasuredValueScaledWithCP56Time2a* MeasuredValueScaledWithCP56Time2a;

int
MeasuredValueScaledWithCP56Time2a_getObjectAddress(MeasuredValueScaledWithCP56Time2a self);

int
MeasuredValueScaledWithCP56Time2a_getScaledValue(MeasuredValueScaledWithCP56Time2a self);

QualityDescriptor
MeasuredValueScaledWithCP56Time2a_getQuality(MeasuredValueScaledWithCP56Time2a self);

CP56Time2a
MeasuredValueScaledWithCP56Time2a_getTimestamp(MeasuredValueScaledWithCP56Time2a self);

void
MeasuredValueScaledWithCP56Time2a_destroy(MeasuredValueScaledWithCP56Time2a self);

/**********************************************
 * BitString32 (:InformationObject)
 **********************************************/

typedef struct sBitString32* BitString32;

void
BitString32_initialize(BitString32 self);

void
BitString32_destroy(BitString32 self);

uint32_t
BitString32_getValue(BitString32 self);

QualityDescriptor
BitString32_getQuality(BitString32 self);

/**********************************************
 * Bitstring32WithCP24Time2a (:BitString32)
 **********************************************/

typedef struct sBitstring32WithCP24Time2a* Bitstring32WithCP24Time2a;

void
Bitstring32WithCP24Time2a_initialize(Bitstring32WithCP24Time2a self);

void
Bitstring32WithCP24Time2a_destroy(Bitstring32WithCP24Time2a self);

CP24Time2a
Bitstring32WithCP24Time2a_getTimestamp(Bitstring32WithCP24Time2a self);

/**********************************************
 * Bitstring32WithCP56Time2a (:BitString32)
 **********************************************/

typedef struct sBitstring32WithCP56Time2a* Bitstring32WithCP56Time2a;

void
Bitstring32WithCP56Time2a_initialize(Bitstring32WithCP56Time2a self);

void
Bitstring32WithCP56Time2a_destroy(Bitstring32WithCP56Time2a self);

CP24Time2a
Bitstring32WithCP56Time2a_getTimestamp(Bitstring32WithCP56Time2a self);

#endif /* SRC_INC_INFORMATION_OBJECTS_H_ */
