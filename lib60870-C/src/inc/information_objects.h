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

int
InformationObject_getObjectAddress(InformationObject self);

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

void
SinglePointInformation_initialize(SinglePointInformation self);

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

void
SinglePointWithCP24Time2a_destroy(SinglePointWithCP24Time2a self);

void
SinglePointWithCP24Time2a_initialize(SinglePointWithCP24Time2a self);

CP24Time2a
SinglePointWithCP24Time2a_getTimestamp(SinglePointWithCP24Time2a self);

/********************************************************
 *  SinglePointWithCP56Time2a (:SinglePointInformation)
 ********************************************************/

typedef struct sSinglePointWithCP56Time2a* SinglePointWithCP56Time2a;

void
SinglePointWithCP56Time2a_destroy(SinglePointWithCP56Time2a self);

void
SinglePointWithCP56Time2a_initialize(SinglePointWithCP56Time2a self);

CP56Time2a
SinglePointWithCP56Time2a_getTimestamp(SinglePointWithCP56Time2a self);


/************************************************
 * DoublePointInformation (:InformationObject)
 ************************************************/

typedef struct sDoublePointInformation* DoublePointInformation;

void
DoublePointInformation_destroy(DoublePointInformation self);

void
DoublePointInformation_initialize(DoublePointInformation self);

DoublePointValue
DoublePointInformation_getValue(DoublePointInformation self);

QualityDescriptor
DoublePointInformation_getQuality(DoublePointInformation self);

/********************************************************
 *  DoublePointWithCP24Time2a (:DoublePointInformation)
 ********************************************************/

typedef struct sDoublePointWithCP24Time2a* DoublePointWithCP24Time2a;

void
DoublePointWithCP24Time2a_destroy(DoublePointWithCP24Time2a self);

void
DoublePointWithCP24Time2a_initialize(DoublePointWithCP24Time2a self);

CP24Time2a
DoublePointWithCP24Time2a_getTimestamp(DoublePointWithCP24Time2a self);

/********************************************************
 *  DoublePointWithCP56Time2a (:DoublePointInformation)
 ********************************************************/

typedef struct sDoublePointWithCP56Time2a* DoublePointWithCP56Time2a;

void
DoublePointWithCP56Time2a_destroy(DoublePointWithCP56Time2a self);

void
DoublePointWithCP56Time2a_initialize(DoublePointWithCP56Time2a self);

CP56Time2a
DoublePointWithCP56Time2a_getTimestamp(DoublePointWithCP56Time2a self);

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

CP56Time2a
Bitstring32WithCP56Time2a_getTimestamp(Bitstring32WithCP56Time2a self);

/**********************************************
 * MeasuredValueNormalized
 **********************************************/

typedef struct sMeasuredValueNormalized* MeasuredValueNormalized;

void
MeasuredValueNormalized_initialize(MeasuredValueNormalized self);

void
MeasuredValueNormalized_destroy(MeasuredValueNormalized self);

float
MeasuredValueNormalized_getValue(MeasuredValueNormalized self);

void
MeasuredValueNormalized_setValue(MeasuredValueNormalized self, float value);

QualityDescriptor
MeasuredValueNormalized_getQuality(MeasuredValueNormalized self);

/***********************************************************************
 * MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
 ***********************************************************************/

typedef struct sMeasuredValueNormalizedWithCP24Time2a* MeasuredValueNormalizedWithCP24Time2a;

void
MeasuredValueNormalizedWithCP24Time2a_initialize(MeasuredValueNormalizedWithCP24Time2a self);

void
MeasuredValueNormalizedWithCP24Time2a_destroy(MeasuredValueNormalizedWithCP24Time2a self);

CP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getTimestamp(MeasuredValueNormalizedWithCP24Time2a self);

void
MeasuredValueNormalizedWithCP24Time2a_setTimestamp(MeasuredValueNormalizedWithCP24Time2a self, CP24Time2a value);

/***********************************************************************
 * MeasuredValueNormalizedWithCP56Time2a : MeasuredValueNormalized
 ***********************************************************************/

typedef struct sMeasuredValueNormalizedWithCP56Time2a* MeasuredValueNormalizedWithCP56Time2a;

void
MeasuredValueNormalizedWithCP56Time2a_initialize(MeasuredValueNormalizedWithCP56Time2a self);

void
MeasuredValueNormalizedWithCP56Time2a_destroy(MeasuredValueNormalizedWithCP56Time2a self);

CP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getTimestamp(MeasuredValueNormalizedWithCP56Time2a self);

void
MeasuredValueNormalizedWithCP56Time2a_setTimestamp(MeasuredValueNormalizedWithCP56Time2a self, CP56Time2a value);


/*******************************************
 * MeasuredValueScaled : InformationObject
 *******************************************/

typedef struct sMeasuredValueScaled* MeasuredValueScaled;

void
MeasuredValueScaled_initialize(MeasuredValueScaled self);

void
MeasuredValueScaled_destroy(MeasuredValueScaled self);

int
MeasuredValueScaled_getValue(MeasuredValueScaled self);

void
MeasuredValueScaled_setValue(MeasuredValueScaled self, int value);

QualityDescriptor
MeasuredValueScaled_getQuality(MeasuredValueScaled self);

/***********************************************************************
 * MeasuredValueScaledWithCP24Time2a : MeasuredValueScaled
 ***********************************************************************/

typedef struct sMeasuredValueScaledWithCP24Time2a* MeasuredValueScaledWithCP24Time2a;

void
MeasuredValueScaledWithCP24Time2a_initialize(MeasuredValueScaledWithCP24Time2a self);

void
MeasuredValueScaledWithCP24Time2a_destroy(MeasuredValueScaledWithCP24Time2a self);

CP24Time2a
MeasuredValueScaledWithCP24Time2a_getTimestamp(MeasuredValueScaledWithCP24Time2a self);

void
MeasuredValueScaledWithCP24Time2a_setTimestamp(MeasuredValueScaledWithCP24Time2a self, CP24Time2a value);

/***********************************************************************
 * MeasuredValueScaledWithCP56Time2a : MeasuredValueScaled
 ***********************************************************************/

typedef struct sMeasuredValueScaledWithCP56Time2a* MeasuredValueScaledWithCP56Time2a;

void
MeasuredValueScaledWithCP56Time2a_initialize(MeasuredValueScaledWithCP56Time2a self);

void
MeasuredValueScaledWithCP56Time2a_destroy(MeasuredValueScaledWithCP56Time2a self);

CP56Time2a
MeasuredValueScaledWithCP56Time2a_getTimestamp(MeasuredValueScaledWithCP56Time2a self);

void
MeasuredValueScaledWithCP56Time2a_setTimestamp(MeasuredValueScaledWithCP56Time2a self, CP56Time2a value);

/*******************************************
 * MeasuredValueShort : InformationObject
 *******************************************/

typedef struct sMeasuredValueShort* MeasuredValueShort;

void
MeasuredValueShort_initialize(MeasuredValueShort self);

void
MeasuredValueShort_destroy(MeasuredValueShort self);

float
MeasuredValueShort_getValue(MeasuredValueShort self);

void
MeasuredValueShort_setValue(MeasuredValueShort self, float value);

QualityDescriptor
MeasuredValueShort_getQuality(MeasuredValueShort self);

/***********************************************************************
 * MeasuredValueShortWithCP24Time2a : MeasuredValueShort
 ***********************************************************************/

typedef struct sMeasuredValueShortWithCP24Time2a* MeasuredValueShortWithCP24Time2a;

void
MeasuredValueShortWithCP24Time2a_initialize(MeasuredValueShortWithCP24Time2a self);

void
MeasuredValueShortWithCP24Time2a_destroy(MeasuredValueShortWithCP24Time2a self);

CP24Time2a
MeasuredValueShortWithCP24Time2a_getTimestamp(MeasuredValueShortWithCP24Time2a self);

void
MeasuredValueShortWithCP24Time2a_setTimestamp(MeasuredValueShortWithCP24Time2a self,
        CP24Time2a value);

/***********************************************************************
 * MeasuredValueShortWithCP56Time2a : MeasuredValueShort
 ***********************************************************************/

typedef struct sMeasuredValueShortWithCP56Time2a* MeasuredValueShortWithCP56Time2a;

void
MeasuredValueShortWithCP56Time2a_initialize(MeasuredValueShortWithCP56Time2a self);

void
MeasuredValueShortWithCP56Time2a_destroy(MeasuredValueShortWithCP56Time2a self);

CP56Time2a
MeasuredValueShortWithCP56Time2a_getTimestamp(MeasuredValueShortWithCP56Time2a self);

void
MeasuredValueShortWithCP56Time2a_setTimestamp(MeasuredValueShortWithCP56Time2a self,
        CP56Time2a value);

/*******************************************
 * IntegratedTotals : InformationObject
 *******************************************/

typedef struct sIntegratedTotals* IntegratedTotals;

void
IntegratedTotals_initialize(IntegratedTotals self);

void
IntegratedTotals_destroy(IntegratedTotals self);

BinaryCounterReading
IntegratedTotals_getBCR(IntegratedTotals self);

void
IntegratedTotals_setBCR(IntegratedTotals self, BinaryCounterReading value);

/***********************************************************************
 * IntegratedTotalsWithCP24Time2a : IntegratedTotals
 ***********************************************************************/

typedef struct sIntegratedTotalsWithCP24Time2a* IntegratedTotalsWithCP24Time2a;

void
IntegratedTotalsWithCP24Time2a_initialize(IntegratedTotalsWithCP24Time2a self);

void
IntegratedTotalsWithCP24Time2a_destroy(IntegratedTotalsWithCP24Time2a self);

CP24Time2a
IntegratedTotalsWithCP24Time2a_getTimestamp(IntegratedTotalsWithCP24Time2a self);

void
IntegratedTotalsWithCP24Time2a_setTimestamp(IntegratedTotalsWithCP24Time2a self,
        CP24Time2a value);

/***********************************************************************
 * IntegratedTotalsWithCP56Time2a : IntegratedTotals
 ***********************************************************************/

typedef struct sIntegratedTotalsWithCP56Time2a* IntegratedTotalsWithCP56Time2a;

void
IntegratedTotalsWithCP56Time2a_initialize(IntegratedTotalsWithCP56Time2a self);

void
IntegratedTotalsWithCP56Time2a_destroy(IntegratedTotalsWithCP56Time2a self);

CP56Time2a
IntegratedTotalsWithCP56Time2a_getTimestamp(IntegratedTotalsWithCP56Time2a self);

void
IntegratedTotalsWithCP56Time2a_setTimestamp(IntegratedTotalsWithCP56Time2a self,
        CP56Time2a value);


/*******************************************
 * SingleCommand
 *******************************************/

typedef struct sSingleCommand* SingleCommand;

void
SingleCommand_initialize(SingleCommand self);

SingleCommand
SingleCommand_create(SingleCommand self, int ioa, bool command, bool selectCommand, int qu);

void
SingleCommand_destroy(SingleCommand self);

int
SingleCommand_getQU(SingleCommand self);

bool
SingleCommand_getState(SingleCommand self);

bool
SingleCommand_isSelect(SingleCommand self);

/***********************************************************************
 * SingleCommandWithCP56Time2a : SingleCommand
 ***********************************************************************/

typedef struct sSingleCommandWithCP56Time2a* SingleCommandWithCP56Time2a;

void
SingleCommandWithCP56Time2a_initialize(SingleCommandWithCP56Time2a self);

void
SingleCommandWithCP56Time2a_destroy(SingleCommandWithCP56Time2a self);

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_create(SingleCommandWithCP56Time2a self, int ioa, bool command, bool selectCommand, int qu, CP56Time2a timestamp);

CP56Time2a
SingleCommandWithCP56Time2a_getTimestamp(SingleCommandWithCP56Time2a self);

/*******************************************
 * DoubleCommand : InformationObject
 *******************************************/

typedef struct sDoubleCommand* DoubleCommand;

void
DoubleCommand_initialize(DoubleCommand self);

void
DoubleCommand_destroy(DoubleCommand self);

DoubleCommand
DoubleCommand_create(DoubleCommand self, int ioa, int command, bool selectCommand, int qu);

int
DoubleCommand_getQU(DoubleCommand self);

int
DoubleCommand_getState(DoubleCommand self);

bool
DoubleCommand_isSelect(DoubleCommand self);

#endif /* SRC_INC_INFORMATION_OBJECTS_H_ */
