/*
 * apl_types_internal.h
 *
 *  Created on: Aug 5, 2016
 *      Author: mzillgit
 */

#ifndef SRC_INC_APL_TYPES_INTERNAL_H_
#define SRC_INC_APL_TYPES_INTERNAL_H_

#include <stdint.h>

struct sCP16Time2a {
    uint8_t encodedValue[2];
};

struct sCP24Time2a {
    uint8_t encodedValue[3];
};

struct sCP56Time2a {
    uint8_t encodedValue[7];
};

#endif /* SRC_INC_APL_TYPES_INTERNAL_H_ */
