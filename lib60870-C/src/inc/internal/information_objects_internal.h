/*
 *  Copyright 2016-2025 Michael Zillgith
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

#ifndef SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_
#define SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_

#include "cs101_information_objects.h"
#include "frame.h"

typedef struct sInformationObjectVFT* InformationObjectVFT;

bool
InformationObject_encode(InformationObject self, Frame frame, CS101_AppLayerParameters parameters, bool isSequence);

void
InformationObject_setObjectAddress(InformationObject self, int ioa);

int
InformationObject_ParseObjectAddress(CS101_AppLayerParameters parameters, const uint8_t* msg, int startIndex);

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                     int msgSize, int startIndex, bool isSequence);

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self,
                                                CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                int startIndex, bool isSequence);

StepPositionInformation
StepPositionInformation_getFromBuffer(StepPositionInformation self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                      int msgSize, int startIndex, bool isSequence);

StepPositionWithCP56Time2a
StepPositionWithCP56Time2a_getFromBuffer(StepPositionWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                         uint8_t* msg, int msgSize, int startIndex, bool isSequence);

StepPositionWithCP24Time2a
StepPositionWithCP24Time2a_getFromBuffer(StepPositionWithCP24Time2a self, CS101_AppLayerParameters parameters,
                                         uint8_t* msg, int msgSize, int startIndex, bool isSequence);

DoublePointInformation
DoublePointInformation_getFromBuffer(DoublePointInformation self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                     int msgSize, int startIndex, bool isSequence);

DoublePointWithCP24Time2a
DoublePointWithCP24Time2a_getFromBuffer(DoublePointWithCP24Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

DoublePointWithCP56Time2a
DoublePointWithCP56Time2a_getFromBuffer(DoublePointWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

SinglePointWithCP24Time2a
SinglePointWithCP24Time2a_getFromBuffer(SinglePointWithCP24Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

SinglePointWithCP56Time2a
SinglePointWithCP56Time2a_getFromBuffer(SinglePointWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

BitString32
BitString32_getFromBuffer(BitString32 self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                          int startIndex, bool isSequence);

Bitstring32WithCP24Time2a
Bitstring32WithCP24Time2a_getFromBuffer(Bitstring32WithCP24Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

Bitstring32WithCP56Time2a
Bitstring32WithCP56Time2a_getFromBuffer(Bitstring32WithCP56Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalized
MeasuredValueNormalized_getFromBuffer(MeasuredValueNormalized self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                      int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalizedWithCP24Time2a
MeasuredValueNormalizedWithCP24Time2a_getFromBuffer(MeasuredValueNormalizedWithCP24Time2a self,
                                                    CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                    int startIndex, bool isSequence);

MeasuredValueNormalizedWithCP56Time2a
MeasuredValueNormalizedWithCP56Time2a_getFromBuffer(MeasuredValueNormalizedWithCP56Time2a self,
                                                    CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                    int startIndex, bool isSequence);

MeasuredValueScaled
MeasuredValueScaled_getFromBuffer(MeasuredValueScaled self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                  int msgSize, int startIndex, bool isSequence);

MeasuredValueScaledWithCP24Time2a
MeasuredValueScaledWithCP24Time2a_getFromBuffer(MeasuredValueScaledWithCP24Time2a self,
                                                CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                int startIndex, bool isSequence);

MeasuredValueShort
MeasuredValueShort_getFromBuffer(MeasuredValueShort self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                 int msgSize, int startIndex, bool isSequence);

MeasuredValueShortWithCP24Time2a
MeasuredValueShortWithCP24Time2a_getFromBuffer(MeasuredValueShortWithCP24Time2a self,
                                               CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                               int startIndex, bool isSequence);

MeasuredValueShortWithCP56Time2a
MeasuredValueShortWithCP56Time2a_getFromBuffer(MeasuredValueShortWithCP56Time2a self,
                                               CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                               int startIndex, bool isSequence);

IntegratedTotals
IntegratedTotals_getFromBuffer(IntegratedTotals self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                               int startIndex, bool isSequence);

IntegratedTotalsWithCP24Time2a
IntegratedTotalsWithCP24Time2a_getFromBuffer(IntegratedTotalsWithCP24Time2a self, CS101_AppLayerParameters parameters,
                                             uint8_t* msg, int msgSize, int startIndex, bool isSequence);

IntegratedTotalsWithCP56Time2a
IntegratedTotalsWithCP56Time2a_getFromBuffer(IntegratedTotalsWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                             uint8_t* msg, int msgSize, int startIndex, bool isSequence);

EventOfProtectionEquipment
EventOfProtectionEquipment_getFromBuffer(EventOfProtectionEquipment self, CS101_AppLayerParameters parameters,
                                         uint8_t* msg, int msgSize, int startIndex, bool isSequence);

PackedStartEventsOfProtectionEquipment
PackedStartEventsOfProtectionEquipment_getFromBuffer(PackedStartEventsOfProtectionEquipment self,
                                                     CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                     int startIndex, bool isSequence);

PackedOutputCircuitInfo
PackedOutputCircuitInfo_getFromBuffer(PackedOutputCircuitInfo self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                      int msgSize, int startIndex, bool isSequence);

PackedSinglePointWithSCD
PackedSinglePointWithSCD_getFromBuffer(PackedSinglePointWithSCD self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                       int msgSize, int startIndex, bool isSequence);

MeasuredValueNormalizedWithoutQuality
MeasuredValueNormalizedWithoutQuality_getFromBuffer(MeasuredValueNormalizedWithoutQuality self,
                                                    CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                    int startIndex, bool isSequence);

EventOfProtectionEquipmentWithCP56Time2a
EventOfProtectionEquipmentWithCP56Time2a_getFromBuffer(EventOfProtectionEquipmentWithCP56Time2a self,
                                                       CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                       int startIndex, bool isSequence);

PackedStartEventsOfProtectionEquipmentWithCP56Time2a
PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getFromBuffer(
    PackedStartEventsOfProtectionEquipmentWithCP56Time2a self, CS101_AppLayerParameters parameters, uint8_t* msg,
    int msgSize, int startIndex, bool isSequence);

PackedOutputCircuitInfoWithCP56Time2a
PackedOutputCircuitInfoWithCP56Time2a_getFromBuffer(PackedOutputCircuitInfoWithCP56Time2a self,
                                                    CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                    int startIndex, bool isSequence);

SingleCommand
SingleCommand_getFromBuffer(SingleCommand self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                            int startIndex);

SingleCommandWithCP56Time2a
SingleCommandWithCP56Time2a_getFromBuffer(SingleCommandWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                          uint8_t* msg, int msgSize, int startIndex);

DoubleCommand
DoubleCommand_getFromBuffer(DoubleCommand self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                            int startIndex);

StepCommand
StepCommand_getFromBuffer(StepCommand self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                          int startIndex);

SetpointCommandNormalized
SetpointCommandNormalized_getFromBuffer(SetpointCommandNormalized self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandScaled
SetpointCommandScaled_getFromBuffer(SetpointCommandScaled self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                    int msgSize, int startIndex);

SetpointCommandShort
SetpointCommandShort_getFromBuffer(SetpointCommandShort self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                   int msgSize, int startIndex);

Bitstring32Command
Bitstring32Command_getFromBuffer(Bitstring32Command self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                 int msgSize, int startIndex);

ReadCommand
ReadCommand_getFromBuffer(ReadCommand self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                          int startIndex);

ClockSynchronizationCommand
ClockSynchronizationCommand_getFromBuffer(ClockSynchronizationCommand self, CS101_AppLayerParameters parameters,
                                          uint8_t* msg, int msgSize, int startIndex);

InterrogationCommand
InterrogationCommand_getFromBuffer(InterrogationCommand self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                   int msgSize, int startIndex);

ParameterNormalizedValue
ParameterNormalizedValue_getFromBuffer(ParameterNormalizedValue self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                       int msgSize, int startIndex);

ParameterScaledValue
ParameterScaledValue_getFromBuffer(ParameterScaledValue self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                   int msgSize, int startIndex);

ParameterFloatValue
ParameterFloatValue_getFromBuffer(ParameterFloatValue self, CS101_AppLayerParameters parameters, uint8_t* msqg,
                                  int msgSize, int startIndex);

ParameterActivation
ParameterActivation_getFromBuffer(ParameterActivation self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                  int msgSize, int startIndex);

EndOfInitialization
EndOfInitialization_getFromBuffer(EndOfInitialization self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                  int msgSize, int startIndex);

DoubleCommandWithCP56Time2a
DoubleCommandWithCP56Time2a_getFromBuffer(DoubleCommandWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                          uint8_t* msg, int msgSize, int startIndex);

StepCommandWithCP56Time2a
StepCommandWithCP56Time2a_getFromBuffer(StepCommandWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex);

SetpointCommandNormalizedWithCP56Time2a
SetpointCommandNormalizedWithCP56Time2a_getFromBuffer(SetpointCommandNormalizedWithCP56Time2a self,
                                                      CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                      int startIndex);

SetpointCommandScaledWithCP56Time2a
SetpointCommandScaledWithCP56Time2a_getFromBuffer(SetpointCommandScaledWithCP56Time2a self,
                                                  CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                  int startIndex);

SetpointCommandShortWithCP56Time2a
SetpointCommandShortWithCP56Time2a_getFromBuffer(SetpointCommandShortWithCP56Time2a self,
                                                 CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                                 int startIndex);

Bitstring32CommandWithCP56Time2a
Bitstring32CommandWithCP56Time2a_getFromBuffer(Bitstring32CommandWithCP56Time2a self,
                                               CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                                               int startIndex);

CounterInterrogationCommand
CounterInterrogationCommand_getFromBuffer(CounterInterrogationCommand self, CS101_AppLayerParameters parameters,
                                          uint8_t* msg, int msgSize, int startIndex);

TestCommand
TestCommand_getFromBuffer(TestCommand self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                          int startIndex);

TestCommandWithCP56Time2a
TestCommandWithCP56Time2a_getFromBuffer(TestCommandWithCP56Time2a self, CS101_AppLayerParameters parameters,
                                        uint8_t* msg, int msgSize, int startIndex);

ResetProcessCommand
ResetProcessCommand_getFromBuffer(ResetProcessCommand self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                  int msgSize, int startIndex);

DelayAcquisitionCommand
DelayAcquisitionCommand_getFromBuffer(DelayAcquisitionCommand self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                      int msgSize, int startIndex);

FileReady
FileReady_getFromBuffer(FileReady self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize, int startIndex);

SectionReady
SectionReady_getFromBuffer(SectionReady self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                           int startIndex);

FileCallOrSelect
FileCallOrSelect_getFromBuffer(FileCallOrSelect self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                               int startIndex);

FileLastSegmentOrSection
FileLastSegmentOrSection_getFromBuffer(FileLastSegmentOrSection self, CS101_AppLayerParameters parameters, uint8_t* msg,
                                       int msgSize, int startIndex);

FileACK
FileACK_getFromBuffer(FileACK self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize, int startIndex);

FileSegment
FileSegment_getFromBuffer(FileSegment self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                          int startIndex);

FileDirectory
FileDirectory_getFromBuffer(FileDirectory self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize,
                            int startIndex, bool isSequence);

QueryLog
QueryLog_getFromBuffer(QueryLog self, CS101_AppLayerParameters parameters, uint8_t* msg, int msgSize, int startIndex);

/********************************************
 * static InformationObject type definitions
 ********************************************/

struct sSinglePointInformation
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;
};

struct sStepPositionInformation
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;
};

struct sStepPositionWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sStepPositionWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t vti;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sDoublePointInformation
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;
};

struct sDoublePointWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sDoublePointWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    DoublePointValue value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sSinglePointWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sSinglePointWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    bool value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sBitString32
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;
};

struct sBitstring32WithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sBitstring32WithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sMeasuredValueNormalized
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

struct sMeasuredValueNormalizedWithoutQuality
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];
};

struct sMeasuredValueNormalizedWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sMeasuredValueNormalizedWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sMeasuredValueScaled
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;
};

struct sMeasuredValueScaledWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sMeasuredValueScaledWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sMeasuredValueShort
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;
};

struct sMeasuredValueShortWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP24Time2a timestamp;
};

struct sMeasuredValueShortWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    QualityDescriptor quality;

    struct sCP56Time2a timestamp;
};

struct sIntegratedTotals
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;
};

struct sIntegratedTotalsWithCP24Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP24Time2a timestamp;
};

struct sIntegratedTotalsWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sBinaryCounterReading totals;

    struct sCP56Time2a timestamp;
};

struct sEventOfProtectionEquipment
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tSingleEvent event;

    struct sCP16Time2a elapsedTime;

    struct sCP24Time2a timestamp;
};

struct sEventOfProtectionEquipmentWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tSingleEvent event;

    struct sCP16Time2a elapsedTime;

    struct sCP56Time2a timestamp;
};

struct sPackedStartEventsOfProtectionEquipment
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    StartEvent event;

    QualityDescriptorP qdp;

    struct sCP16Time2a elapsedTime;

    struct sCP24Time2a timestamp;
};

struct sPackedStartEventsOfProtectionEquipmentWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    StartEvent event;

    QualityDescriptorP qdp;

    struct sCP16Time2a elapsedTime;

    struct sCP56Time2a timestamp;
};

struct sPackedOutputCircuitInfo
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    OutputCircuitInfo oci;

    QualityDescriptorP qdp;

    struct sCP16Time2a operatingTime;

    struct sCP24Time2a timestamp;
};

struct sPackedOutputCircuitInfoWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    OutputCircuitInfo oci;

    QualityDescriptorP qdp;

    struct sCP16Time2a operatingTime;

    struct sCP56Time2a timestamp;
};

struct sPackedSinglePointWithSCD
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    tStatusAndStatusChangeDetection scd;

    QualityDescriptor qds;
};

struct sSingleCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;
};

struct sSingleCommandWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t sco;

    struct sCP56Time2a timestamp;
};

struct sDoubleCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

struct sDoubleCommandWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;

    struct sCP56Time2a timestamp;
};

struct sStepCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;
};

struct sStepCommandWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t dcq;

    struct sCP56Time2a timestamp;
};

struct sSetpointCommandNormalized
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

struct sSetpointCommandNormalizedWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

struct sSetpointCommandScaled
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */
};

struct sSetpointCommandScaledWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t encodedValue[2];

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

struct sSetpointCommandShort
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    uint8_t qos; /* Qualifier of setpoint command */
};

struct sSetpointCommandShortWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    float value;

    uint8_t qos; /* Qualifier of setpoint command */

    struct sCP56Time2a timestamp;
};

struct sBitstring32Command
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;
};

struct sBitstring32CommandWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint32_t value;

    struct sCP56Time2a timestamp;
};

struct sReadCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;
};

struct sClockSynchronizationCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sCP56Time2a timestamp;
};

struct sInterrogationCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t qoi;
};

struct sCounterInterrogationCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t qcc;
};

struct sTestCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t byte1;
    uint8_t byte2;
};

struct sTestCommandWithCP56Time2a
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t tsc;

    struct sCP56Time2a timestamp;
};

struct sResetProcessCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    QualifierOfRPC qrp;
};

struct sDelayAcquisitionCommand
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    struct sCP16Time2a delay;
};

struct sParameterActivation
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    QualifierOfParameterActivation qpa;
};

struct sEndOfInitialization
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint8_t coi;
};

struct sFileReady
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint32_t lengthOfFile;

    uint8_t frq; /* file ready qualifier */
};

struct sSectionReady
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint32_t lengthOfSection;

    uint8_t srq; /* section ready qualifier */
};

struct sFileCallOrSelect
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t scq; /* select and call qualifier */
};

struct sFileLastSegmentOrSection
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t lsq; /* last section or segment qualifier */

    uint8_t chs; /* checksum of section or segment */
};

struct sFileACK
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t afq; /* AFQ (acknowledge file or section qualifier) */
};

struct sFileSegment
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint8_t nameOfSection;

    uint8_t los; /* length of segment */

    uint8_t* data; /* user data buffer - file payload */
};

struct sFileDirectory
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    uint32_t lengthOfFile; /* LOF */

    uint8_t sof; /* state of file */

    struct sCP56Time2a creationTime;
};

struct sQueryLog
{
    int objectAddress;

    TypeID type;

    InformationObjectVFT virtualFunctionTable;

    uint16_t nof; /* name of file */

    struct sCP56Time2a rangeStartTime;
    struct sCP56Time2a rangeStopTime;
};

union uInformationObject {
    struct sSinglePointInformation m1;
    struct sStepPositionInformation m2;
    struct sStepPositionWithCP24Time2a m3;
    struct sStepPositionWithCP56Time2a m4;
    struct sDoublePointInformation m5;
    struct sDoublePointWithCP24Time2a m6;
    struct sDoublePointWithCP56Time2a m7;
    struct sSinglePointWithCP24Time2a m8;
    struct sSinglePointWithCP56Time2a m9;
    struct sBitString32 m10;
    struct sBitstring32WithCP24Time2a m11;
    struct sBitstring32WithCP56Time2a m12;
    struct sMeasuredValueNormalized m13;
    struct sMeasuredValueNormalizedWithCP24Time2a m14;
    struct sMeasuredValueNormalizedWithCP56Time2a m15;
    struct sMeasuredValueScaled m16;
    struct sMeasuredValueScaledWithCP24Time2a m17;
    struct sMeasuredValueScaledWithCP56Time2a m18;
    struct sMeasuredValueShort m19;
    struct sMeasuredValueShortWithCP24Time2a m20;
    struct sMeasuredValueShortWithCP56Time2a m21;
    struct sIntegratedTotals m22;
    struct sIntegratedTotalsWithCP24Time2a m23;
    struct sIntegratedTotalsWithCP56Time2a m24;
    struct sSingleCommand m25;
    struct sSingleCommandWithCP56Time2a m26;
    struct sDoubleCommand m27;
    struct sStepCommand m28;
    struct sSetpointCommandNormalized m29;
    struct sSetpointCommandScaled m30;
    struct sSetpointCommandShort m31;
    struct sBitstring32Command m32;
    struct sReadCommand m33;
    struct sClockSynchronizationCommand m34;
    struct sInterrogationCommand m35;
    struct sParameterActivation m36;
    struct sEventOfProtectionEquipmentWithCP56Time2a m37;
    struct sStepCommandWithCP56Time2a m38;
    struct sFileDirectory m39;
    struct sQueryLog m40;
};

#endif /* SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_ */
