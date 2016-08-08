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

void
InformationObject_encode(InformationObject self, Frame frame, ConnectionParameters parameters);

SinglePointInformation
SinglePointInformation_getFromBuffer(SinglePointInformation self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

MeasuredValueScaledWithCP56Time2a
MeasuredValueScaledWithCP56Time2a_getFromBuffer(MeasuredValueScaledWithCP56Time2a self, ConnectionParameters parameters,
        uint8_t* msg, int msgSize, int startIndex);

#endif /* SRC_INC_INFORMATION_OBJECTS_INTERNAL_H_ */
