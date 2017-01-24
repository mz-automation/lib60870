/*
 * information_objects_internal.h
 *
 *  Created on: Aug 5, 2016
 *      Author: mzillgit
 */

#ifndef SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_
#define SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_

#include "information_objects.h"
#include "frame.h"

bool
InformationObject_encode(InformationObject self, Frame frame, ConnectionParameters parameters, bool isSequence);

void
InformationObject_setObjectAddress(InformationObject self, int ioa);

int
InformationObject_ParseObjectAddress(ConnectionParameters parameters, uint8_t* msg, int startIndex);

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

StepPositionInformation
StepPositionInformation_getFromBuffer(StepPositionInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_getFromBuffer(StepPositionWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_getFromBuffer(StepPositionWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

DoublePointInformation
DoublePointInformation_getFromBuffer(DoublePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_getFromBuffer(DoublePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_getFromBuffer(DoublePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_getFromBuffer(SinglePointWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_getFromBuffer(SinglePointWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

BitString32
BitString32_getFromBuffer(BitString32 self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_getFromBuffer(Bitstring32WithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_getFromBuffer(Bitstring32WithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalized
MeasuredValueNormalized_getFromBuffer(MeasuredValueNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(MeasuredValueNormalizedWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getFromBuffer(MeasuredValueNormalizedWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueScaled
MeasuredValueScaled_getFromBuffer(MeasuredValueScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_getFromBuffer(MeasuredValueScaledWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueShort
MeasuredValueShort_getFromBuffer(MeasuredValueShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_getFromBuffer(MeasuredValueShortWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_getFromBuffer(MeasuredValueShortWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

IntegratedTotals
IntegratedTotals_getFromBuffer(IntegratedTotals self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_getFromBuffer(IntegratedTotalsWithCP24Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_getFromBuffer(IntegratedTotalsWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

EventOfProtectionEquipment
EventOfProtectionEquipment_getFromBuffer(EventOfProtectionEquipment self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedStartEventsOfProtectionEquipment
PackedStartEventsOfProtectionEquipment_getFromBuffer(PackedStartEventsOfProtectionEquipment self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedOutputCircuitInfo
PackedOutputCircuitInfo_getFromBuffer(PackedOutputCircuitInfo self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedSinglePointWithSCD
PackedSinglePointWithSCD_getFromBuffer(PackedSinglePointWithSCD self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalizedWithoutQuality
MeasuredValueNormalizedWithoutQuality_getFromBuffer(MeasuredValueNormalizedWithoutQuality self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

EventOfProtectionEquipmentWithCP56Time2a
EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer(EventOfProtectionEquipmentWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedStartEventsOfProtectionEquipmentWithCP56Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer(PackedStartEventsOfProtectionEquipmentWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedOutputCircuitInfoWithCP56Time2a
PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer(PackedOutputCircuitInfoWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

SingleCommand
SingleCommand_getFromBuffer(SingleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_getFromBuffer(SingleCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

DoubleCommand
DoubleCommand_getFromBuffer(DoubleCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

StepCommand
StepCommand_getFromBuffer(StepCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandNormalized
SetpointCommandNormalized_getFromBuffer(SetpointCommandNormalized self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandScaled
SetpointCommandScaled_getFromBuffer(SetpointCommandScaled self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandShort
SetpointCommandShort_getFromBuffer(SetpointCommandShort self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

Bitstring32Command
Bitstring32Command_getFromBuffer(Bitstring32Command self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ReadCommand
ReadCommand_getFromBuffer(ReadCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ClockSynchronizationCommand
ClockSynchronizationCommand_getFromBuffer(ClockSynchronizationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

InterrogationCommand
InterrogationCommand_getFromBuffer(InterrogationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ParameterNormalizedValue
ParameterNormalizedValue_getFromBuffer(ParameterNormalizedValue self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ParameterScaledValue
ParameterScaledValue_getFromBuffer(ParameterScaledValue self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ParameterFloatValue
ParameterFloatValue_getFromBuffer(ParameterFloatValue self, ConnectionParameters parameters,
        uint8_t* msqg, int msgSize, int startIndex);

ParameterActivation
ParameterActivation_getFromBuffer(ParameterActivation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

EndOfInitialization
EndOfInitialization_getFromBuffer(EndOfInitialization self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

DoubleCommandWithCP56Time2a
DoubleCommandWithCP56Time2a_getFromBuffer(DoubleCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

StepCommandWithCP56Time2a
StepCommandWithCP56Time2a_getFromBuffer(StepCommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandNormalizedWithCP56Time2a
SetpointCommandNormalizedWithCP56Time2a_getFromBuffer(SetpointCommandNormalizedWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandScaledWithCP56Time2a
SetpointCommandScaledWithCP56Time2a_getFromBuffer(SetpointCommandScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandShortWithCP56Time2a
SetpointCommandShortWithCP56Time2a_getFromBuffer(SetpointCommandShortWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

Bitstring32CommandWithCP56Time2a
Bitstring32CommandWithCP56Time2a_getFromBuffer(Bitstring32CommandWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

CounterInterrogationCommand
CounterInterrogationCommand_getFromBuffer(CounterInterrogationCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

ResetProcessCommand
ResetProcessCommand_getFromBuffer(ResetProcessCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

DelayAcquisitionCommand
DelayAcquisitionCommand_getFromBuffer(DelayAcquisitionCommand self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

#endif /* SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_ */
