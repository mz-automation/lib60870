#include "unity.h"
#include "iec60870_common.h"
#include "cs104_slave.h"
#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "buffer_frame.h"
#include <string.h>
#include <stdlib.h>

#ifndef CONFIG_CS104_SUPPORT_TLS
#define CONFIG_CS104_SUPPORT_TLS 0
#endif

#if WIN32
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0) 
#endif

void setUp(void) { }
void tearDown(void) {}

static struct sCS101_AppLayerParameters defaultAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249
};

typedef struct sCS104_IPAddress* CS104_IPAddress;

struct sCS104_IPAddress
{
    uint8_t address[16];
    eCS104_IPAddressType type;
};

/* declaration of library internal function */
void
InformationObject_setObjectAddress(InformationObject self, int ioa);

static bool
CS104_IPAddress_setFromString(CS104_IPAddress self, const char* ipAddrStr)
{
    if (strchr(ipAddrStr, '.') != NULL) {
        /* parse IPv4 string */
        self->type = IP_ADDRESS_TYPE_IPV4;

        int i;

        for (i = 0; i < 4; i++) {
            uint32_t val = strtoul(ipAddrStr, NULL, 10);

            if (val > UINT8_MAX)
                return false;

            self->address[i] = val;

            ipAddrStr = strchr(ipAddrStr, '.');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }

        return true;
    }
    else if (strchr(ipAddrStr, ':') != NULL) {
        self->type = IP_ADDRESS_TYPE_IPV6;

        /* has "::" ? */
        char* doubleSep = (char*) strstr(ipAddrStr, "::");

        int elementsBefore = 0;
        int elementsAfter = 8;
        int elementsSkipped = 0;

        if (doubleSep) {
            /* count number of elements before double separator */
            char* curPos = (char*) ipAddrStr;

            if (curPos != doubleSep) {
                elementsBefore = 1;

                while (curPos < doubleSep) {
                    if (*curPos == ':')
                        elementsBefore++;
                    curPos++;
                }
            }

            /* count number of elements after double separator */
            elementsAfter = 0;

            curPos = doubleSep + 2;

            if (*curPos != 0) {

                elementsAfter = 1;

                while (*curPos != 0) {
                    if (*curPos == ':')
                        elementsAfter++;
                    curPos++;
                }
            }

            elementsSkipped = 8 - elementsBefore - elementsAfter;
        }

        int i;

        for (i = 0; i < elementsBefore; i++) {
            uint32_t val = strtoul(ipAddrStr, NULL, 16);

            if (val > UINT16_MAX)
                return false;

            self->address[i * 2] = val / 0x100;
            self->address[i * 2 + 1] = val % 0x100;

            ipAddrStr = strchr(ipAddrStr, ':');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }

        for (i = elementsBefore; i < elementsBefore + elementsSkipped; i++) {
            self->address[i * 2] = 0;
            self->address[i * 2 +1] = 0;
        }

        if (doubleSep)
            ipAddrStr = doubleSep + 2;

        for (i = elementsBefore + elementsSkipped; i < 8; i++) {
            uint32_t val = strtoul(ipAddrStr, NULL, 16);

            if (val > UINT16_MAX)
                return false;

            self->address[i * 2] = val / 0x100;
            self->address[i * 2 + 1] = val % 0x100;

            ipAddrStr = strchr(ipAddrStr, ':');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }

        return true;
    }
    else {
        return false;
    }
}

static bool
CS104_IPAddress_equals(CS104_IPAddress self, CS104_IPAddress other)
{
    if (self->type != other->type)
        return false;

    int size;

    if (self->type == IP_ADDRESS_TYPE_IPV4)
        size = 4;
    else
        size = 16;

    int i;

    for (i = 0; i < size; i++) {
        if (self->address[i] != other->address[i])
            return false;
    }

    return true;
}

#ifdef __cplusplus
extern "C" {
#endif

void
CS101_ASDU_encode(CS101_ASDU self, Frame frame);

#ifdef __cplusplus
}
#endif

void
test_CP56Time2a(void)
{
    struct sCP56Time2a currentTime;

    uint64_t currentTimestamp = Hal_getTimeInMs();

    CP56Time2a_createFromMsTimestamp(&currentTime, currentTimestamp);

    uint64_t convertedTimestamp = CP56Time2a_toMsTimestamp(&currentTime);

    TEST_ASSERT_EQUAL_UINT64(currentTimestamp, convertedTimestamp);
}

void
test_CP56Time2aToMsTimestamp(void)
{
    struct sCP56Time2a timeval;

    timeval.encodedValue[0] = 0x85;
    timeval.encodedValue[1] = 0x49;
    timeval.encodedValue[2] = 0x0c;
    timeval.encodedValue[3] = 0x09;
    timeval.encodedValue[4] = 0x55;
    timeval.encodedValue[5] = 0x03;
    timeval.encodedValue[6] = 0x11;

    uint64_t convertedTimeval = CP56Time2a_toMsTimestamp(&timeval);

    TEST_ASSERT_EQUAL_UINT64((uint64_t) 1490087538821, convertedTimeval);
}

void
test_CP56Time2aConversionFunctions(void)
{
    uint64_t currentTime = Hal_getTimeInMs();

    struct sCP56Time2a timeval;

    CP56Time2a_setFromMsTimestamp(&timeval, currentTime);

    uint64_t convertedTime = CP56Time2a_toMsTimestamp(&timeval);

    TEST_ASSERT_EQUAL_UINT64(currentTime, convertedTime);
}

void
test_StepPositionInformation(void)
{
    StepPositionInformation spi1;
    StepPositionInformation spi2;
    StepPositionInformation spi3;
    StepPositionInformation spi4;
    StepPositionInformation spi5;
    StepPositionInformation spi6;
    StepPositionInformation spi7;
    StepPositionInformation spi8;

    spi1 = StepPositionInformation_create(NULL, 101, 0, true, IEC60870_QUALITY_GOOD);
    spi2 = StepPositionInformation_create(NULL, 102, 63, false, IEC60870_QUALITY_OVERFLOW);
    spi3 = StepPositionInformation_create(NULL, 103, 62, false, IEC60870_QUALITY_RESERVED);
    spi4 = StepPositionInformation_create(NULL, 104, 61, false, IEC60870_QUALITY_ELAPSED_TIME_INVALID);
    spi5 = StepPositionInformation_create(NULL, 105, -61, false, IEC60870_QUALITY_BLOCKED);
    spi6 = StepPositionInformation_create(NULL, 106, -62, false, IEC60870_QUALITY_SUBSTITUTED);
    spi7 = StepPositionInformation_create(NULL, 107, -63, false, IEC60870_QUALITY_NON_TOPICAL);
    spi8 = StepPositionInformation_create(NULL, 108, 0, false, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue(spi1));
    TEST_ASSERT_TRUE(StepPositionInformation_isTransient(spi1));

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi2));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi2));

    TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue(spi3));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi3));

    TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue(spi4));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi4));

    TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue(spi5));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi5));

    TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue(spi6));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi6));

    TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue(spi7));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi7));

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue(spi8));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality(spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality(spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality(spi3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality(spi4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality(spi5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality(spi6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality(spi7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality(spi8));


	uint8_t buffer[256];

	struct sBufferFrame bf;

	Frame f = BufferFrame_initialize(&bf, buffer, 0);

	CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi4);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi5);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi6);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi7);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi8);

	StepPositionInformation_destroy(spi1);
	StepPositionInformation_destroy(spi2);
	StepPositionInformation_destroy(spi3);
	StepPositionInformation_destroy(spi4);
	StepPositionInformation_destroy(spi5);
	StepPositionInformation_destroy(spi6);
	StepPositionInformation_destroy(spi7);
	StepPositionInformation_destroy(spi8);

	CS101_ASDU_encode(asdu, f);

	TEST_ASSERT_EQUAL_INT(46, Frame_getMsgSize(f));

	CS101_ASDU_destroy(asdu);

	CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

	TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

	StepPositionInformation spi1_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 0);
	StepPositionInformation spi2_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 1);
	StepPositionInformation spi3_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 2);
	StepPositionInformation spi4_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 3);
	StepPositionInformation spi5_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 4);
	StepPositionInformation spi6_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 5);
	StepPositionInformation spi7_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 6);
	StepPositionInformation spi8_dec = (StepPositionInformation) CS101_ASDU_getElement(asdu2, 7);

	TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
	TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
	TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));
	TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject)spi4_dec));
	TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject)spi5_dec));
	TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject)spi6_dec));
	TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject)spi7_dec));
	TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject)spi8_dec));

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality(spi1_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality(spi2_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality(spi3_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality(spi4_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality(spi5_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality(spi6_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality(spi7_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality(spi8_dec));

	TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue(spi1_dec));
	TEST_ASSERT_TRUE(StepPositionInformation_isTransient(spi1_dec));

	TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi2_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi2_dec));

	TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue(spi3_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi3_dec));

	TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue(spi4_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi4_dec));

	TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue(spi5_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi5_dec));

	TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue(spi6_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi6_dec));

	TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue(spi7_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi7_dec));

	TEST_ASSERT_EQUAL_INT(-0, StepPositionInformation_getValue(spi8_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi8_dec));

    StepPositionInformation_destroy(spi1_dec);
    StepPositionInformation_destroy(spi2_dec);
    StepPositionInformation_destroy(spi3_dec);
    StepPositionInformation_destroy(spi4_dec);
    StepPositionInformation_destroy(spi5_dec);
    StepPositionInformation_destroy(spi6_dec);
    StepPositionInformation_destroy(spi7_dec);
    StepPositionInformation_destroy(spi8_dec);
	CS101_ASDU_destroy(asdu2);
}
void
test_StepPositionWithCP24Time2a(void)
{
	StepPositionWithCP24Time2a spi1;
	StepPositionWithCP24Time2a spi2;
	StepPositionWithCP24Time2a spi3;
	StepPositionWithCP24Time2a spi4;
	StepPositionWithCP24Time2a spi5;
	StepPositionWithCP24Time2a spi6;
	StepPositionWithCP24Time2a spi7;
	StepPositionWithCP24Time2a spi8;

    uint64_t time1 = Hal_getTimeInMs();
	uint64_t time2 = time1 + 1000;
	uint64_t time3 = time2 + 1000;
	uint64_t time4 = time3 + 1000;
	uint64_t time5 = time4 + 1000;
	uint64_t time6 = time5 + 1000;
	uint64_t time7 = time6 + 1000;
	uint64_t time8 = time7 + 1000;

	struct sCP24Time2a cpTime1;
	struct sCP24Time2a cpTime2;
	struct sCP24Time2a cpTime3;
	struct sCP24Time2a cpTime4;
	struct sCP24Time2a cpTime5;
	struct sCP24Time2a cpTime6;
	struct sCP24Time2a cpTime7;
	struct sCP24Time2a cpTime8;

	bzero(&cpTime1, sizeof(struct sCP24Time2a));
	bzero(&cpTime2, sizeof(struct sCP24Time2a));
	bzero(&cpTime3, sizeof(struct sCP24Time2a));
	bzero(&cpTime4, sizeof(struct sCP24Time2a));
	bzero(&cpTime5, sizeof(struct sCP24Time2a));
	bzero(&cpTime6, sizeof(struct sCP24Time2a));
	bzero(&cpTime7, sizeof(struct sCP24Time2a));
	bzero(&cpTime8, sizeof(struct sCP24Time2a));

	CP24Time2a_setMinute(&cpTime1, 12);
	CP24Time2a_setMillisecond(&cpTime1, 24123);

	CP24Time2a_setMinute(&cpTime2, 54);
	CP24Time2a_setMillisecond(&cpTime2, 12345);

	CP24Time2a_setMinute(&cpTime3, 00);
	CP24Time2a_setMillisecond(&cpTime3, 00001);

	CP24Time2a_setMinute(&cpTime4, 12);
	CP24Time2a_setMillisecond(&cpTime4, 24123);

	CP24Time2a_setMinute(&cpTime5, 12);
	CP24Time2a_setMillisecond(&cpTime5, 24123);

	CP24Time2a_setMinute(&cpTime6, 12);
	CP24Time2a_setMillisecond(&cpTime6, 24123);

	CP24Time2a_setMinute(&cpTime7, 12);
	CP24Time2a_setMillisecond(&cpTime7, 24123);

	CP24Time2a_setMinute(&cpTime8, 12);
	CP24Time2a_setMillisecond(&cpTime8, 24123);

    spi1 = StepPositionWithCP24Time2a_create(NULL, 101, 0, true, IEC60870_QUALITY_GOOD, &cpTime1);
    spi2 = StepPositionWithCP24Time2a_create(NULL, 102, 63, false, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    spi3 = StepPositionWithCP24Time2a_create(NULL, 103, 62, false, IEC60870_QUALITY_RESERVED, &cpTime3);
    spi4 = StepPositionWithCP24Time2a_create(NULL, 104, 61, false, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    spi5 = StepPositionWithCP24Time2a_create(NULL, 105, -61, false, IEC60870_QUALITY_BLOCKED, &cpTime5);
    spi6 = StepPositionWithCP24Time2a_create(NULL, 106, -62, false, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    spi7 = StepPositionWithCP24Time2a_create(NULL, 107, -63, false, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    spi8 = StepPositionWithCP24Time2a_create(NULL, 108, 0, false, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi1));
    TEST_ASSERT_TRUE(StepPositionInformation_isTransient((StepPositionInformation)spi1));

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue((StepPositionInformation)spi2));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi2));

    TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue((StepPositionInformation)spi3));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi3));

    TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue((StepPositionInformation)spi4));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi4));

    TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue((StepPositionInformation)spi5));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi5));

    TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue((StepPositionInformation)spi6));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi6));

    TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue((StepPositionInformation)spi7));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi7));

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi8));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality((StepPositionInformation)spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality((StepPositionInformation)spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality((StepPositionInformation)spi3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality((StepPositionInformation)spi5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality((StepPositionInformation)spi6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality((StepPositionInformation)spi7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi8));


	uint8_t buffer[256];

	struct sBufferFrame bf;

	Frame f = BufferFrame_initialize(&bf, buffer, 0);

	CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi4);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi5);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi6);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi7);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi8);

	StepPositionWithCP24Time2a_destroy(spi1);
	StepPositionWithCP24Time2a_destroy(spi2);
	StepPositionWithCP24Time2a_destroy(spi3);
	StepPositionWithCP24Time2a_destroy(spi4);
	StepPositionWithCP24Time2a_destroy(spi5);
	StepPositionWithCP24Time2a_destroy(spi6);
	StepPositionWithCP24Time2a_destroy(spi7);
	StepPositionWithCP24Time2a_destroy(spi8);

	CS101_ASDU_encode(asdu, f);

	TEST_ASSERT_EQUAL_INT(70, Frame_getMsgSize(f));

	CS101_ASDU_destroy(asdu);

	CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

	TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

	StepPositionWithCP24Time2a spi1_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
	StepPositionWithCP24Time2a spi2_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
	StepPositionWithCP24Time2a spi3_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);
	StepPositionWithCP24Time2a spi4_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 3);
	StepPositionWithCP24Time2a spi5_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 4);
	StepPositionWithCP24Time2a spi6_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 5);
	StepPositionWithCP24Time2a spi7_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 6);
	StepPositionWithCP24Time2a spi8_dec = (StepPositionWithCP24Time2a) CS101_ASDU_getElement(asdu2, 7);

	TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
	TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
	TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));
	TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject)spi4_dec));
	TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject)spi5_dec));
	TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject)spi6_dec));
	TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject)spi7_dec));
	TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject)spi8_dec));

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality((StepPositionInformation)spi1_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality((StepPositionInformation)spi2_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality((StepPositionInformation)spi3_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi4_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality((StepPositionInformation)spi5_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality((StepPositionInformation)spi6_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality((StepPositionInformation)spi7_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi8_dec));

	TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi1_dec));
	TEST_ASSERT_TRUE(StepPositionInformation_isTransient((StepPositionInformation)spi1_dec));

	TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue((StepPositionInformation)spi2_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi2_dec));

	TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue((StepPositionInformation)spi3_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi3_dec));

	TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue((StepPositionInformation)spi4_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi4_dec));

	TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue((StepPositionInformation)spi5_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi5_dec));

	TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue((StepPositionInformation)spi6_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi6_dec));

	TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue((StepPositionInformation)spi7_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi7_dec));

	TEST_ASSERT_EQUAL_INT(-0, StepPositionInformation_getValue((StepPositionInformation)spi8_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi8_dec));

	CP24Time2a time1_dec = StepPositionWithCP24Time2a_getTimestamp(spi1_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

	CP24Time2a time2_dec = StepPositionWithCP24Time2a_getTimestamp(spi2_dec);
	TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
	TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

	CP24Time2a time3_dec = StepPositionWithCP24Time2a_getTimestamp(spi3_dec);
	TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
	TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
	TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

	CP24Time2a time4_dec = StepPositionWithCP24Time2a_getTimestamp(spi4_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time4_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time4_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time4_dec));

	CP24Time2a time5_dec = StepPositionWithCP24Time2a_getTimestamp(spi5_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time5_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time5_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time5_dec));

	CP24Time2a time6_dec = StepPositionWithCP24Time2a_getTimestamp(spi6_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time6_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time6_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time6_dec));

	CP24Time2a time7_dec = StepPositionWithCP24Time2a_getTimestamp(spi7_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time7_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time7_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time7_dec));

	CP24Time2a time8_dec = StepPositionWithCP24Time2a_getTimestamp(spi8_dec);
	TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time8_dec));
	TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time8_dec));
	TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time8_dec));

	StepPositionWithCP24Time2a_destroy(spi1_dec);
	StepPositionWithCP24Time2a_destroy(spi2_dec);
	StepPositionWithCP24Time2a_destroy(spi3_dec);
	StepPositionWithCP24Time2a_destroy(spi4_dec);
	StepPositionWithCP24Time2a_destroy(spi5_dec);
	StepPositionWithCP24Time2a_destroy(spi6_dec);
	StepPositionWithCP24Time2a_destroy(spi7_dec);
	StepPositionWithCP24Time2a_destroy(spi8_dec);
	CS101_ASDU_destroy(asdu2);
}
void
test_StepPositionWithCP56Time2a(void)
{
	StepPositionWithCP56Time2a spi1;
	StepPositionWithCP56Time2a spi2;
	StepPositionWithCP56Time2a spi3;
	StepPositionWithCP56Time2a spi4;
	StepPositionWithCP56Time2a spi5;
	StepPositionWithCP56Time2a spi6;
	StepPositionWithCP56Time2a spi7;
	StepPositionWithCP56Time2a spi8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;
    struct sCP56Time2a cpTime4;
    struct sCP56Time2a cpTime5;
    struct sCP56Time2a cpTime6;
    struct sCP56Time2a cpTime7;
    struct sCP56Time2a cpTime8;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);
    CP56Time2a_createFromMsTimestamp(&cpTime4, time4);
    CP56Time2a_createFromMsTimestamp(&cpTime5, time5);
    CP56Time2a_createFromMsTimestamp(&cpTime6, time6);
    CP56Time2a_createFromMsTimestamp(&cpTime7, time7);
    CP56Time2a_createFromMsTimestamp(&cpTime8, time8);

    spi1 = StepPositionWithCP56Time2a_create(NULL, 101, 0, true, IEC60870_QUALITY_GOOD, &cpTime1);
    spi2 = StepPositionWithCP56Time2a_create(NULL, 102, 63, false, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    spi3 = StepPositionWithCP56Time2a_create(NULL, 103, 62, false, IEC60870_QUALITY_RESERVED, &cpTime3);
    spi4 = StepPositionWithCP56Time2a_create(NULL, 104, 61, false, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    spi5 = StepPositionWithCP56Time2a_create(NULL, 105, -61, false, IEC60870_QUALITY_BLOCKED, &cpTime5);
    spi6 = StepPositionWithCP56Time2a_create(NULL, 106, -62, false, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    spi7 = StepPositionWithCP56Time2a_create(NULL, 107, -63, false, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    spi8 = StepPositionWithCP56Time2a_create(NULL, 108, 0, false, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi1));
    TEST_ASSERT_TRUE(StepPositionInformation_isTransient((StepPositionInformation)spi1));

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue((StepPositionInformation)spi2));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi2));

    TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue((StepPositionInformation)spi3));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi3));

    TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue((StepPositionInformation)spi4));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi4));

    TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue((StepPositionInformation)spi5));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi5));

    TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue((StepPositionInformation)spi6));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi6));

    TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue((StepPositionInformation)spi7));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi7));

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi8));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality((StepPositionInformation)spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality((StepPositionInformation)spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality((StepPositionInformation)spi3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality((StepPositionInformation)spi5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality((StepPositionInformation)spi6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality((StepPositionInformation)spi7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi8));


	uint8_t buffer[256];

	struct sBufferFrame bf;

	Frame f = BufferFrame_initialize(&bf, buffer, 0);

	CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi4);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi5);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi6);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi7);
	CS101_ASDU_addInformationObject(asdu, (InformationObject) spi8);

	StepPositionWithCP56Time2a_destroy(spi1);
	StepPositionWithCP56Time2a_destroy(spi2);
	StepPositionWithCP56Time2a_destroy(spi3);
	StepPositionWithCP56Time2a_destroy(spi4);
	StepPositionWithCP56Time2a_destroy(spi5);
	StepPositionWithCP56Time2a_destroy(spi6);
	StepPositionWithCP56Time2a_destroy(spi7);
	StepPositionWithCP56Time2a_destroy(spi8);

	CS101_ASDU_encode(asdu, f);

	TEST_ASSERT_EQUAL_INT(102, Frame_getMsgSize(f));

	CS101_ASDU_destroy(asdu);

	CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

	TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

	StepPositionWithCP56Time2a spi1_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
	StepPositionWithCP56Time2a spi2_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
	StepPositionWithCP56Time2a spi3_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);
	StepPositionWithCP56Time2a spi4_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 3);
	StepPositionWithCP56Time2a spi5_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 4);
	StepPositionWithCP56Time2a spi6_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 5);
	StepPositionWithCP56Time2a spi7_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 6);
	StepPositionWithCP56Time2a spi8_dec = (StepPositionWithCP56Time2a) CS101_ASDU_getElement(asdu2, 7);

	TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
	TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
	TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));
	TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject)spi4_dec));
	TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject)spi5_dec));
	TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject)spi6_dec));
	TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject)spi7_dec));
	TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject)spi8_dec));

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, StepPositionInformation_getQuality((StepPositionInformation)spi1_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, StepPositionInformation_getQuality((StepPositionInformation)spi2_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, StepPositionInformation_getQuality((StepPositionInformation)spi3_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi4_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, StepPositionInformation_getQuality((StepPositionInformation)spi5_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, StepPositionInformation_getQuality((StepPositionInformation)spi6_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, StepPositionInformation_getQuality((StepPositionInformation)spi7_dec));
	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, StepPositionInformation_getQuality((StepPositionInformation)spi8_dec));

	TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue((StepPositionInformation)spi1_dec));
	TEST_ASSERT_TRUE(StepPositionInformation_isTransient((StepPositionInformation)spi1_dec));

	TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue((StepPositionInformation)spi2_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi2_dec));

	TEST_ASSERT_EQUAL_INT(62, StepPositionInformation_getValue((StepPositionInformation)spi3_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi3_dec));

	TEST_ASSERT_EQUAL_INT(61, StepPositionInformation_getValue((StepPositionInformation)spi4_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi4_dec));

	TEST_ASSERT_EQUAL_INT(-61, StepPositionInformation_getValue((StepPositionInformation)spi5_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi5_dec));

	TEST_ASSERT_EQUAL_INT(-62, StepPositionInformation_getValue((StepPositionInformation)spi6_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi6_dec));

	TEST_ASSERT_EQUAL_INT(-63, StepPositionInformation_getValue((StepPositionInformation)spi7_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi7_dec));

	TEST_ASSERT_EQUAL_INT(-0, StepPositionInformation_getValue((StepPositionInformation)spi8_dec));
	TEST_ASSERT_FALSE(StepPositionInformation_isTransient((StepPositionInformation)spi8_dec));

	TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi1_dec)));
	TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi2_dec)));
	TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi3_dec)));
	TEST_ASSERT_EQUAL_UINT64(time4, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi4_dec)));
	TEST_ASSERT_EQUAL_UINT64(time5, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi5_dec)));
	TEST_ASSERT_EQUAL_UINT64(time6, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi6_dec)));
	TEST_ASSERT_EQUAL_UINT64(time7, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi7_dec)));
	TEST_ASSERT_EQUAL_UINT64(time8, CP56Time2a_toMsTimestamp(StepPositionWithCP56Time2a_getTimestamp(spi8_dec)));

	StepPositionWithCP56Time2a_destroy(spi1_dec);
	StepPositionWithCP56Time2a_destroy(spi2_dec);
	StepPositionWithCP56Time2a_destroy(spi3_dec);
	StepPositionWithCP56Time2a_destroy(spi4_dec);
	StepPositionWithCP56Time2a_destroy(spi5_dec);
	StepPositionWithCP56Time2a_destroy(spi6_dec);
	StepPositionWithCP56Time2a_destroy(spi7_dec);
	StepPositionWithCP56Time2a_destroy(spi8_dec);
	CS101_ASDU_destroy(asdu2);
}

void
test_addMaxNumberOfIOsToASDU(void)
{
    struct sCS101_AppLayerParameters salParameters;

    salParameters.maxSizeOfASDU = 100;
    salParameters.originatorAddress = 0;
    salParameters.sizeOfCA = 2;
    salParameters.sizeOfCOT = 2;
    salParameters.sizeOfIOA = 3;
    salParameters.sizeOfTypeId = 1;
    salParameters.sizeOfVSQ = 1;

    CS101_ASDU asdu = CS101_ASDU_create(&salParameters, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    int ioa = 100;

    bool added = false;

    do {
        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, ioa, true, IEC60870_QUALITY_GOOD);

        added = CS101_ASDU_addInformationObject(asdu, io);

        InformationObject_destroy(io);

        ioa++;
    }
    while (added);

    CS101_ASDU_destroy(asdu);

    TEST_ASSERT_EQUAL_INT(124, ioa);
}

void
test_SingleEventType(void)
{
    tSingleEvent singleEvent = 0;

    EventState eventState = SingleEvent_getEventState(&singleEvent);

    TEST_ASSERT_EQUAL_INT(IEC60870_EVENTSTATE_INDETERMINATE_0, eventState);

    QualityDescriptorP qdp = SingleEvent_getQDP(&singleEvent);

    TEST_ASSERT_EQUAL_INT(0, qdp);
}

void
test_EventOfProtectionEquipmentWithTime(void)
{
#ifndef _WIN32
    tSingleEvent singleEvent = 0;
    struct sCP16Time2a elapsedTime;
    struct sCP56Time2a timestamp;

    EventOfProtectionEquipmentWithCP56Time2a e = EventOfProtectionEquipmentWithCP56Time2a_create(NULL, 1, &singleEvent, &elapsedTime, &timestamp);

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) e);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) e);

    CS101_ASDU_encode(asdu, f);

    InformationObject_destroy((InformationObject) e);
    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    InformationObject io = CS101_ASDU_getElement(asdu2, 1);

    TEST_ASSERT_NOT_NULL(io);

    EventOfProtectionEquipmentWithCP56Time2a e2 = (EventOfProtectionEquipmentWithCP56Time2a) io;

    SingleEvent se = EventOfProtectionEquipmentWithCP56Time2a_getEvent(e2);

    QualityDescriptorP qdp = SingleEvent_getQDP(se);

    InformationObject_destroy(io);
    CS101_ASDU_destroy(asdu2);

    TEST_ASSERT_EQUAL_INT(0, qdp);
#endif
}

struct test_CS104SlaveConnectionIsRedundancyGroup_Info
{
    bool running;
    CS104_Slave slave;
};

static void*
test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction(void* parameter)
{
    struct test_CS104SlaveConnectionIsRedundancyGroup_Info* info =  (struct test_CS104SlaveConnectionIsRedundancyGroup_Info*) parameter;

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(info->slave);

    int16_t scaledValue = 0;

    while (info->running) {

        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(info->slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        Thread_sleep(10);
    }

    return NULL;
}

void
test_CS104SlaveConnectionIsRedundancyGroup()
{
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setServerMode(slave, CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    struct test_CS104SlaveConnectionIsRedundancyGroup_Info info;
    info.running = true;
    info.slave = slave;

    Thread enqueueThread = Thread_create(test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction, &info, false);
    Thread_start(enqueueThread);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    int i;

    for (i = 0; i < 50; i++) {
        bool result = CS104_Connection_connect(con);

        TEST_ASSERT_TRUE(result);

        CS104_Connection_sendStartDT(con);

        Thread_sleep(10);

        CS104_Connection_close(con);
    }

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveSingleRedundancyGroup()
{
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    struct test_CS104SlaveConnectionIsRedundancyGroup_Info info;
    info.running = true;
    info.slave = slave;

    Thread enqueueThread = Thread_create(test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction, &info, false);
    Thread_start(enqueueThread);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    int i;

    for (i = 0; i < 50; i++) {
        bool result = CS104_Connection_connect(con);
        TEST_ASSERT_TRUE(result);

        CS104_Connection_sendStartDT(con);

        CS104_Connection_close(con);

        Thread_sleep(10);
    }

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

static void
test_CS104SlaveSingleRedundancyGroupMultipleConnectionsEventHandler(void* parameter, IMasterConnection connection, CS104_PeerConnectionEvent event)
{
    char ipAddrBuf[100];
    ipAddrBuf[0] = 0;

    IMasterConnection_getPeerAddress(connection, ipAddrBuf, 100);
}

void
test_CS104SlaveSingleRedundancyGroupMultipleConnections()
{
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);
    CS104_Slave_setMaxOpenConnections(slave, 20);
    CS104_Slave_setConnectionEventHandler(slave, test_CS104SlaveSingleRedundancyGroupMultipleConnectionsEventHandler, NULL);

    struct test_CS104SlaveConnectionIsRedundancyGroup_Info info;
    info.running = true;
    info.slave = slave;

    CS104_Connection cons[3];
    int conState[3]; /* 0 = idle, 1 = connected, 2 = START_DT sent */

    cons[0] = CS104_Connection_create("127.0.0.1", 20004);
    cons[1] = CS104_Connection_create("127.0.0.1", 20004);
    cons[2] = CS104_Connection_create("127.0.0.1", 20004);

    conState[0] = 0;
    conState[1] = 0;
    conState[2] = 0;

    Thread enqueueThread = Thread_create(test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction, &info, false);
    Thread_start(enqueueThread);

    int i;
    for (i = 0; i < 200; i++)
    {
       // printf("round %i\n", i);

        int con = rand() % 3;

        if (conState[con] == 0) {
            bool result = CS104_Connection_connect(cons[con]);
            TEST_ASSERT_TRUE(result);
            conState[con] = 1;
        }
        else if (conState[con] == 1) {
            CS104_Connection_sendStartDT(cons[con]);
            conState[con] = 2;
        }
        else if (conState[con] == 2) {
            CS104_Connection_close(cons[con]);
            conState[con] = 0;
        }

        Thread_sleep(50);
    }

    CS104_Connection_destroy(cons[0]);
    CS104_Connection_destroy(cons[1]);
    CS104_Connection_destroy(cons[2]);

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Slave_destroy(slave);
}

struct stest_CS104SlaveEventQueue1 {
    int asduHandlerCalled;
    int spontCount;
    int16_t lastScaledValue;
};

static bool
test_CS104SlaveEventQueue1_asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    struct stest_CS104SlaveEventQueue1* info = (struct stest_CS104SlaveEventQueue1*) parameter;

    info->asduHandlerCalled++;

    if (CS101_ASDU_getCOT(asdu) == CS101_COT_SPONTANEOUS)
    {
        info->spontCount++;

        if (CS101_ASDU_getTypeID(asdu) == M_ME_NB_1)
        {
            static uint8_t ioBuf[250];

            MeasuredValueScaled mv = (MeasuredValueScaled) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

            info->lastScaledValue = MeasuredValueScaled_getValue(mv);
        }
    }

    return true;
}

struct sTestMessageQueueEntryInfo {
	uint64_t entryTimestamp;
	unsigned int entryState : 2;
	unsigned int size : 8;
};

void
test_CS104SlaveEventQueue1()
{
    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    for (int i = 0; i < 15; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(500);

    CS104_Connection_sendStopDT(con);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(14, info.lastScaledValue);

    info.asduHandlerCalled = 0;
    info.spontCount = 0;

    result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    for (int i = 0; i < 15; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        Thread_sleep(10);
    }

    Thread_sleep(500);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(15, info.asduHandlerCalled);
    TEST_ASSERT_EQUAL_INT(15, info.spontCount);
    TEST_ASSERT_EQUAL_INT(29, info.lastScaledValue);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveEventQueueOverflow()
{
    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    for (int i = 0; i < 300; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(1500);

    CS104_Connection_close(con);

    int asduSize = 12;
    int entrySize = sizeof(struct sTestMessageQueueEntryInfo) + asduSize;
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 10) / entrySize;

    TEST_ASSERT_EQUAL_INT(299, info.lastScaledValue);
    TEST_ASSERT_EQUAL_INT(msgQueueCapacity, info.asduHandlerCalled);

    result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    for (int i = 0; i < 150; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        Thread_sleep(10);
    }

    Thread_sleep(500);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(msgQueueCapacity + 150, info.asduHandlerCalled);
    TEST_ASSERT_EQUAL_INT(msgQueueCapacity + 150, info.spontCount);
    TEST_ASSERT_EQUAL_INT(449, info.lastScaledValue);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveEventQueueOverflow2()
{
    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    for (int i = 0; i < 300; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(500);

    CS104_Connection_close(con);

    int asduSize = 12;
    int entrySize = sizeof(struct sTestMessageQueueEntryInfo) + asduSize;
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 10) / entrySize;

    TEST_ASSERT_EQUAL_INT(299, info.lastScaledValue);
    TEST_ASSERT_EQUAL_INT(msgQueueCapacity, info.asduHandlerCalled);

    result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    int typeNo = 0;

    for (int i = 0; i < 20000; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io;

        if (typeNo == 0) {
            io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);
            CS101_ASDU_addInformationObject(newAsdu, io);
        }
        else if (typeNo == 1) {
            struct sCP56Time2a cp56;
            CP56Time2a_createFromMsTimestamp(&cp56, Hal_getTimeInMs());

            io =  (InformationObject) MeasuredValueScaledWithCP56Time2a_create(NULL, 111, scaledValue, IEC60870_QUALITY_GOOD, &cp56);

            CS101_ASDU_addInformationObject(newAsdu, io);
        }
        else if (typeNo == 2) {
            io = (InformationObject) SinglePointInformation_create(NULL, 112, true, IEC60870_QUALITY_GOOD);

            CS101_ASDU_addInformationObject(newAsdu, io);
        }
        else if (typeNo == 3) {
            io = (InformationObject) SinglePointInformation_create(NULL, 112, true, IEC60870_QUALITY_GOOD);
            int j = 1;
            while (CS101_ASDU_addInformationObject(newAsdu, io)) {
                io = (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 112 + j, true, IEC60870_QUALITY_GOOD);
                j++;
            }
        }

        typeNo++;

        if (typeNo == 4)
            typeNo = 0;

        scaledValue++;

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        //Thread_sleep(10);
    }

    Thread_sleep(500);

    CS104_Connection_close(con);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveEventQueueCheckCapacity()
{
    CS104_Slave slave = CS104_Slave_create(2, 2);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    /* Fill queue with small messages */
    int asduSize = 6 + 3 + 1;
    int entrySize = sizeof(struct sTestMessageQueueEntryInfo) + asduSize;
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 2) / entrySize;

    for (int i = 0; i < 299; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_GOOD);

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        if (i >= msgQueueCapacity)
            TEST_ASSERT_EQUAL_INT(msgQueueCapacity, CS104_Slave_getNumberOfQueueEntries(slave, NULL));
        else
            TEST_ASSERT_EQUAL_INT(i + 1, CS104_Slave_getNumberOfQueueEntries(slave, NULL));

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(1000);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(msgQueueCapacity, info.asduHandlerCalled);

    /* outstanding I messages that are not confirmed */
    TEST_ASSERT_EQUAL_INT(0, CS104_Slave_getNumberOfQueueEntries(slave, NULL));

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveEventQueueOverflow3()
{
    /**
     * Trigger code to remove multiple messages at once from the buffer
     */

    CS104_Slave slave = CS104_Slave_create(2, 2);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    /* Fill queue with small messages */

    int asduSize = 6 + 3 + 1;
    int entrySize = sizeof(struct sTestMessageQueueEntryInfo) + asduSize;
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 2) / entrySize;

    for (int i = 0; i < 35; i++) {

        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_GOOD);

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    int count1 = CS104_Slave_getNumberOfQueueEntries(slave, NULL);



    /* add a single large messages */
    CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);

    for (int i = 0; i < 50; i++) {
          SinglePointInformation spi = SinglePointInformation_create(NULL, 110, false, IEC60870_QUALITY_GOOD);;

          CS101_ASDU_addInformationObject(newAsdu, (InformationObject) spi);

          InformationObject_destroy((InformationObject) spi);
    }

    CS104_Slave_enqueueASDU(slave, newAsdu);

    CS101_ASDU_destroy(newAsdu);

    int count2 = CS104_Slave_getNumberOfQueueEntries(slave, NULL);

    /* check that multiple buffer entries were removed */
    TEST_ASSERT_TRUE(count2 + 1 < count1);

    info.asduHandlerCalled = 0;

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(500);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(count2, info.asduHandlerCalled);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}


void
test_IpAddressHandling(void)
{
    struct sCS104_IPAddress ipAddr1;

    CS104_IPAddress_setFromString(&ipAddr1, "192.168.34.25");

    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV4, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(192, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(168, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(34, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(25, ipAddr1.address[3]);

    CS104_IPAddress_setFromString(&ipAddr1, "1:22:333:aaaa:b:c:d:e");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x22, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x03, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x33, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0xaa, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0xaa, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x0b, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x0c, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x0d, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x0e, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "::2001:db8");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x20, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x0d, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0xb8, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "fe80::70d2:6cba:a994:2ced");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0xfe, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x70, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0xd2, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x6c, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0xba, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0xa9, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x94, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x2c, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0xed, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "2001:db8:1::ab9:C0A8:102");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x20, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x0d, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0xb8, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x0a, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0xb9, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0xc0, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0xa8, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x02, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "::1");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "::1");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "::");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[15]);

    CS104_IPAddress_setFromString(&ipAddr1, "fe80:2001::");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0xfe, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x20, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[15]);
}

void
test_DoublePointInformation(void)
{
    DoublePointInformation dpi1;
    DoublePointInformation dpi2;
    DoublePointInformation dpi3;

    dpi1 = DoublePointInformation_create(NULL, 101, IEC60870_DOUBLE_POINT_OFF, IEC60870_QUALITY_INVALID);
    dpi2 = DoublePointInformation_create(NULL, 102, IEC60870_DOUBLE_POINT_ON, IEC60870_QUALITY_BLOCKED);
    dpi3 = DoublePointInformation_create(NULL, 103, IEC60870_DOUBLE_POINT_INDETERMINATE, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_OFF, DoublePointInformation_getValue(dpi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_ON, DoublePointInformation_getValue(dpi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_INDETERMINATE, DoublePointInformation_getValue(dpi3));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi3);

    InformationObject_destroy((InformationObject) dpi1);
    InformationObject_destroy((InformationObject) dpi2);
    InformationObject_destroy((InformationObject) dpi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(18, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    DoublePointInformation dpi1_dec = (DoublePointInformation) CS101_ASDU_getElement(asdu2, 0);
    DoublePointInformation dpi2_dec = (DoublePointInformation) CS101_ASDU_getElement(asdu2, 1);
    DoublePointInformation dpi3_dec = (DoublePointInformation) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )dpi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )dpi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )dpi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, DoublePointInformation_getQuality(dpi1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_OFF, DoublePointInformation_getValue(dpi1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, DoublePointInformation_getQuality(dpi2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_ON, DoublePointInformation_getValue(dpi2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, DoublePointInformation_getQuality(dpi3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_INDETERMINATE, DoublePointInformation_getValue(dpi3_dec));

    InformationObject_destroy((InformationObject) dpi1_dec);
    InformationObject_destroy((InformationObject) dpi2_dec);
    InformationObject_destroy((InformationObject) dpi3_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SinglePointInformation(void)
{
    SinglePointInformation spi1;
    SinglePointInformation spi2;
    SinglePointInformation spi3;
    SinglePointInformation spi4;

    spi1 = SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_INVALID);
    spi2 = SinglePointInformation_create(NULL, 102, false, IEC60870_QUALITY_BLOCKED);
    spi3 = SinglePointInformation_create(NULL, 103, true, IEC60870_QUALITY_GOOD);

    /* invalid quality bit (overflow) is expected to be ignored */
    spi4 = SinglePointInformation_create(NULL, 104, false, IEC60870_QUALITY_OVERFLOW);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi4));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi4);

    SinglePointInformation_destroy(spi1);
    SinglePointInformation_destroy(spi2);
    SinglePointInformation_destroy(spi3);
    SinglePointInformation_destroy(spi4);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(22, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(4, CS101_ASDU_getNumberOfElements(asdu2));

    SinglePointInformation spi1_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 0);
    SinglePointInformation spi2_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 1);
    SinglePointInformation spi3_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 2);
    SinglePointInformation spi4_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 3);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject)spi4_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue(spi2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi4_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue(spi4_dec));

    SinglePointInformation_destroy(spi1_dec);
    SinglePointInformation_destroy(spi2_dec);
    SinglePointInformation_destroy(spi3_dec);
    SinglePointInformation_destroy(spi4_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SinglePointInformationSequence(void)
{
    SinglePointInformation spi1;
    SinglePointInformation spi2;
    SinglePointInformation spi3;
    SinglePointInformation spi4;

    spi1 = SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_INVALID);
    spi2 = SinglePointInformation_create(NULL, 102, false, IEC60870_QUALITY_BLOCKED);
    spi3 = SinglePointInformation_create(NULL, 103, true, IEC60870_QUALITY_GOOD);

    /* invalid quality bit (overflow) is expected to be ignored */
    spi4 = SinglePointInformation_create(NULL, 104, false, IEC60870_QUALITY_OVERFLOW);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi4));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, true, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi4);

    SinglePointInformation_destroy(spi1);
    SinglePointInformation_destroy(spi2);
    SinglePointInformation_destroy(spi3);
    SinglePointInformation_destroy(spi4);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(13, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(4, CS101_ASDU_getNumberOfElements(asdu2));

    SinglePointInformation spi1_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 0);
    SinglePointInformation spi2_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 1);
    SinglePointInformation spi3_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 2);
    SinglePointInformation spi4_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 3);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject)spi4_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue(spi2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi4_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue(spi4_dec));

    SinglePointInformation_destroy(spi1_dec);
    SinglePointInformation_destroy(spi2_dec);
    SinglePointInformation_destroy(spi3_dec);
    SinglePointInformation_destroy(spi4_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_DoublePointWithCP24Time2a(void)
{
    DoublePointWithCP24Time2a dpi1;
    DoublePointWithCP24Time2a dpi2;
    DoublePointWithCP24Time2a dpi3;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;
    struct sCP24Time2a cpTime3;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));
    bzero(&cpTime3, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    CP24Time2a_setMinute(&cpTime3, 00);
    CP24Time2a_setMillisecond(&cpTime3, 00001);

    dpi1 = DoublePointWithCP24Time2a_create(NULL, 101, IEC60870_DOUBLE_POINT_OFF, IEC60870_QUALITY_INVALID, &cpTime1);
    dpi2 = DoublePointWithCP24Time2a_create(NULL, 102, IEC60870_DOUBLE_POINT_ON, IEC60870_QUALITY_BLOCKED, &cpTime2);
    dpi3 = DoublePointWithCP24Time2a_create(NULL, 103, IEC60870_DOUBLE_POINT_INDETERMINATE, IEC60870_QUALITY_GOOD, &cpTime3);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, DoublePointInformation_getQuality((DoublePointInformation )dpi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, DoublePointInformation_getQuality((DoublePointInformation )dpi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, DoublePointInformation_getQuality((DoublePointInformation )dpi3));
    TEST_ASSERT_TRUE(DoublePointInformation_getQuality((DoublePointInformation )dpi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi3);

    InformationObject_destroy((InformationObject) dpi1);
    InformationObject_destroy((InformationObject) dpi2);
    InformationObject_destroy((InformationObject) dpi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(27, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    DoublePointWithCP24Time2a dpi1_dec = (DoublePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    DoublePointWithCP24Time2a dpi2_dec = (DoublePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
    DoublePointWithCP24Time2a dpi3_dec = (DoublePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )dpi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )dpi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )dpi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, DoublePointInformation_getQuality((DoublePointInformation )dpi1_dec));
    TEST_ASSERT_TRUE(DoublePointInformation_getQuality((DoublePointInformation )dpi1_dec));
    CP24Time2a time1_dec = DoublePointWithCP24Time2a_getTimestamp(dpi1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, DoublePointInformation_getQuality((DoublePointInformation )dpi2_dec));
    TEST_ASSERT_TRUE(DoublePointInformation_getQuality((DoublePointInformation )dpi2_dec));
    CP24Time2a time2_dec = DoublePointWithCP24Time2a_getTimestamp(dpi2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, DoublePointInformation_getQuality((DoublePointInformation )dpi3_dec));
    TEST_ASSERT_TRUE(DoublePointInformation_getValue((DoublePointInformation )dpi3_dec));
    CP24Time2a time3_dec = DoublePointWithCP24Time2a_getTimestamp(dpi3_dec);
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
    TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

    InformationObject_destroy((InformationObject) dpi1_dec);
    InformationObject_destroy((InformationObject) dpi2_dec);
    InformationObject_destroy((InformationObject) dpi3_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SinglePointWithCP24Time2a(void)
{
    SinglePointWithCP24Time2a spi1;
    SinglePointWithCP24Time2a spi2;
    SinglePointWithCP24Time2a spi3;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;
    struct sCP24Time2a cpTime3;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));
    bzero(&cpTime3, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    CP24Time2a_setMinute(&cpTime3, 00);
    CP24Time2a_setMillisecond(&cpTime3, 00001);

    spi1 = SinglePointWithCP24Time2a_create(NULL, 101, true, IEC60870_QUALITY_INVALID, &cpTime1);
    spi2 = SinglePointWithCP24Time2a_create(NULL, 102, false, IEC60870_QUALITY_BLOCKED, &cpTime2);
    spi3 = SinglePointWithCP24Time2a_create(NULL, 103, true, IEC60870_QUALITY_GOOD, &cpTime3);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality((SinglePointInformation)spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality((SinglePointInformation)spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality((SinglePointInformation)spi3));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);

    InformationObject_destroy((InformationObject) spi1);
    InformationObject_destroy((InformationObject) spi2);
    InformationObject_destroy((InformationObject) spi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(27, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    SinglePointWithCP24Time2a spi1_dec = (SinglePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    SinglePointWithCP24Time2a spi2_dec = (SinglePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
    SinglePointWithCP24Time2a spi3_dec = (SinglePointWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality((SinglePointInformation)spi1_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi1_dec));
    CP24Time2a time1_dec = SinglePointWithCP24Time2a_getTimestamp(spi1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality((SinglePointInformation)spi2_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue((SinglePointInformation)spi2_dec));
    CP24Time2a time2_dec = SinglePointWithCP24Time2a_getTimestamp(spi2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality((SinglePointInformation)spi3_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi3_dec));
    CP24Time2a time3_dec = SinglePointWithCP24Time2a_getTimestamp(spi3_dec);
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
    TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

    InformationObject_destroy((InformationObject) spi1_dec);
    InformationObject_destroy((InformationObject) spi2_dec);
    InformationObject_destroy((InformationObject) spi3_dec);

    CS101_ASDU_destroy(asdu2);
}
void
test_DoublePointWithCP56Time2a(void)
{
	DoublePointWithCP56Time2a dpi1;
	DoublePointWithCP56Time2a dpi2;
	DoublePointWithCP56Time2a dpi3;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);

    dpi1 = DoublePointWithCP56Time2a_create(NULL, 101, IEC60870_DOUBLE_POINT_OFF, IEC60870_QUALITY_INVALID, &cpTime1);
    dpi2 = DoublePointWithCP56Time2a_create(NULL, 102, IEC60870_DOUBLE_POINT_ON, IEC60870_QUALITY_BLOCKED, &cpTime2);
    dpi3 = DoublePointWithCP56Time2a_create(NULL, 103, IEC60870_DOUBLE_POINT_INDETERMINATE, IEC60870_QUALITY_GOOD, &cpTime3);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, DoublePointInformation_getQuality((DoublePointInformation)dpi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, DoublePointInformation_getQuality((DoublePointInformation)dpi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, DoublePointInformation_getQuality((DoublePointInformation)dpi3));
    TEST_ASSERT_TRUE(DoublePointInformation_getValue((DoublePointInformation)dpi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) dpi3);

    InformationObject_destroy((InformationObject) dpi1);
    InformationObject_destroy((InformationObject) dpi2);
    InformationObject_destroy((InformationObject) dpi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(39, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    DoublePointWithCP56Time2a dpi1_dec = (DoublePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    DoublePointWithCP56Time2a dpi2_dec = (DoublePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
    DoublePointWithCP56Time2a dpi3_dec = (DoublePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)dpi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)dpi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)dpi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, DoublePointInformation_getQuality((DoublePointInformation)dpi1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_OFF, DoublePointInformation_getValue((DoublePointInformation)dpi1_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(DoublePointWithCP56Time2a_getTimestamp(dpi1_dec)));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, DoublePointInformation_getQuality((DoublePointInformation)dpi2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_ON, DoublePointInformation_getValue((DoublePointInformation)dpi2_dec));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(DoublePointWithCP56Time2a_getTimestamp(dpi2_dec)));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, DoublePointInformation_getQuality((DoublePointInformation)dpi3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_DOUBLE_POINT_INDETERMINATE, DoublePointInformation_getValue((DoublePointInformation)dpi3_dec));
    TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(DoublePointWithCP56Time2a_getTimestamp(dpi3_dec)));

    InformationObject_destroy((InformationObject) dpi1_dec);
    InformationObject_destroy((InformationObject) dpi2_dec);
    InformationObject_destroy((InformationObject) dpi3_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SinglePointWithCP56Time2a(void)
{
    SinglePointWithCP56Time2a spi1;
    SinglePointWithCP56Time2a spi2;
    SinglePointWithCP56Time2a spi3;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);

    spi1 = SinglePointWithCP56Time2a_create(NULL, 101, true, IEC60870_QUALITY_INVALID, &cpTime1);
    spi2 = SinglePointWithCP56Time2a_create(NULL, 102, false, IEC60870_QUALITY_BLOCKED, &cpTime2);
    spi3 = SinglePointWithCP56Time2a_create(NULL, 103, true, IEC60870_QUALITY_GOOD, &cpTime3);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality((SinglePointInformation)spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality((SinglePointInformation)spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality((SinglePointInformation)spi3));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);

    InformationObject_destroy((InformationObject) spi1);
    InformationObject_destroy((InformationObject) spi2);
    InformationObject_destroy((InformationObject) spi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(39, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    SinglePointWithCP56Time2a spi1_dec = (SinglePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    SinglePointWithCP56Time2a spi2_dec = (SinglePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
    SinglePointWithCP56Time2a spi3_dec = (SinglePointWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality((SinglePointInformation)spi1_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi1_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(SinglePointWithCP56Time2a_getTimestamp(spi1_dec)));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality((SinglePointInformation)spi2_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue((SinglePointInformation)spi2_dec));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(SinglePointWithCP56Time2a_getTimestamp(spi2_dec)));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality((SinglePointInformation)spi3_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue((SinglePointInformation)spi3_dec));
    TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(SinglePointWithCP56Time2a_getTimestamp(spi3_dec)));

    InformationObject_destroy((InformationObject) spi1_dec);
    InformationObject_destroy((InformationObject) spi2_dec);
    InformationObject_destroy((InformationObject) spi3_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_NormalizeMeasureValueWithoutQuality(void)
{
    MeasuredValueNormalizedWithoutQuality nmv1;

    nmv1 = MeasuredValueNormalizedWithoutQuality_create(NULL, 101, 0.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalizedWithoutQuality_getValue((MeasuredValueNormalizedWithoutQuality )nmv1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv1);

    MeasuredValueNormalizedWithoutQuality_destroy(nmv1);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(11, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueNormalizedWithoutQuality nmv1_dec = (MeasuredValueNormalizedWithoutQuality) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )nmv1_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalizedWithoutQuality_getValue((MeasuredValueNormalizedWithoutQuality )nmv1_dec));

    MeasuredValueNormalizedWithoutQuality_destroy(nmv1_dec);
    CS101_ASDU_destroy(asdu2);
}

void
test_NormalizeMeasureValue(void)
{
    MeasuredValueNormalized nmv1;
    MeasuredValueNormalized nmv2;
    MeasuredValueNormalized nmv3;
    MeasuredValueNormalized nmv4;
    MeasuredValueNormalized nmv5;
    MeasuredValueNormalized nmv6;
    MeasuredValueNormalized nmv7;
    MeasuredValueNormalized nmv8;

    nmv1 = MeasuredValueNormalized_create(NULL, 101, -0.5f, IEC60870_QUALITY_GOOD);
    nmv2 = MeasuredValueNormalized_create(NULL, 102, -0.2f, IEC60870_QUALITY_OVERFLOW);
    nmv3 = MeasuredValueNormalized_create(NULL, 103, -0.1f, IEC60870_QUALITY_RESERVED);
    nmv4 = MeasuredValueNormalized_create(NULL, 104, 0, IEC60870_QUALITY_ELAPSED_TIME_INVALID);
    nmv5 = MeasuredValueNormalized_create(NULL, 105, 0.2f, IEC60870_QUALITY_BLOCKED);
    nmv6 = MeasuredValueNormalized_create(NULL, 106, 0.3f, IEC60870_QUALITY_SUBSTITUTED);
    nmv7 = MeasuredValueNormalized_create(NULL, 107, 0.4f, IEC60870_QUALITY_NON_TOPICAL);
    nmv8 = MeasuredValueNormalized_create(NULL, 108, 0.5f, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv8);

    MeasuredValueNormalized_destroy(nmv1);
    MeasuredValueNormalized_destroy(nmv2);
    MeasuredValueNormalized_destroy(nmv3);
    MeasuredValueNormalized_destroy(nmv4);
    MeasuredValueNormalized_destroy(nmv5);
    MeasuredValueNormalized_destroy(nmv6);
    MeasuredValueNormalized_destroy(nmv7);
    MeasuredValueNormalized_destroy(nmv8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(54, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueNormalized nmv1_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueNormalized nmv2_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueNormalized nmv3_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueNormalized nmv4_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueNormalized nmv5_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueNormalized nmv6_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueNormalized nmv7_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueNormalized nmv8_dec = (MeasuredValueNormalized) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )nmv1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )nmv2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )nmv3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )nmv4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )nmv5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )nmv6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )nmv7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )nmv8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8_dec));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8_dec));

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )nmv1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )nmv2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )nmv3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )nmv4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )nmv5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )nmv6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )nmv7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )nmv8_dec));

    MeasuredValueNormalized_destroy(nmv1_dec);
    MeasuredValueNormalized_destroy(nmv2_dec);
    MeasuredValueNormalized_destroy(nmv3_dec);
    MeasuredValueNormalized_destroy(nmv4_dec);
    MeasuredValueNormalized_destroy(nmv5_dec);
    MeasuredValueNormalized_destroy(nmv6_dec);
    MeasuredValueNormalized_destroy(nmv7_dec);
    MeasuredValueNormalized_destroy(nmv8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueNormalizedWithCP24Time2a(void)
{
    MeasuredValueNormalizedWithCP24Time2a nmv1;
    MeasuredValueNormalizedWithCP24Time2a nmv2;
    MeasuredValueNormalizedWithCP24Time2a nmv3;
    MeasuredValueNormalizedWithCP24Time2a nmv4;
    MeasuredValueNormalizedWithCP24Time2a nmv5;
    MeasuredValueNormalizedWithCP24Time2a nmv6;
    MeasuredValueNormalizedWithCP24Time2a nmv7;
    MeasuredValueNormalizedWithCP24Time2a nmv8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;
    struct sCP24Time2a cpTime3;
    struct sCP24Time2a cpTime4;
    struct sCP24Time2a cpTime5;
    struct sCP24Time2a cpTime6;
    struct sCP24Time2a cpTime7;
    struct sCP24Time2a cpTime8;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));
    bzero(&cpTime3, sizeof(struct sCP24Time2a));
    bzero(&cpTime4, sizeof(struct sCP24Time2a));
    bzero(&cpTime5, sizeof(struct sCP24Time2a));
    bzero(&cpTime6, sizeof(struct sCP24Time2a));
    bzero(&cpTime7, sizeof(struct sCP24Time2a));
    bzero(&cpTime8, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    CP24Time2a_setMinute(&cpTime3, 00);
    CP24Time2a_setMillisecond(&cpTime3, 00001);

    CP24Time2a_setMinute(&cpTime4, 12);
    CP24Time2a_setMillisecond(&cpTime4, 24123);

    CP24Time2a_setMinute(&cpTime5, 12);
    CP24Time2a_setMillisecond(&cpTime5, 24123);

    CP24Time2a_setMinute(&cpTime6, 12);
    CP24Time2a_setMillisecond(&cpTime6, 24123);

    CP24Time2a_setMinute(&cpTime7, 12);
    CP24Time2a_setMillisecond(&cpTime7, 24123);

    CP24Time2a_setMinute(&cpTime8, 12);
    CP24Time2a_setMillisecond(&cpTime8, 24123);

    nmv1 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 101, -0.5f, IEC60870_QUALITY_GOOD, &cpTime1);
    nmv2 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 102, -0.2f, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    nmv3 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 103, -0.1f, IEC60870_QUALITY_RESERVED, &cpTime3);
    nmv4 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 104, 0, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    nmv5 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 105, 0.2f, IEC60870_QUALITY_BLOCKED, &cpTime5);
    nmv6 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 106, 0.3f, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    nmv7 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 107, 0.4f, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    nmv8 = MeasuredValueNormalizedWithCP24Time2a_create(NULL, 108, 0.5f, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv8);

    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv1);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv2);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv3);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv4);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv5);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv6);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv7);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(78, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueNormalizedWithCP24Time2a nmv1_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueNormalizedWithCP24Time2a nmv2_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueNormalizedWithCP24Time2a nmv3_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueNormalizedWithCP24Time2a nmv4_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueNormalizedWithCP24Time2a nmv5_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueNormalizedWithCP24Time2a nmv6_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueNormalizedWithCP24Time2a nmv7_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueNormalizedWithCP24Time2a nmv8_dec = (MeasuredValueNormalizedWithCP24Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )nmv1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )nmv2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )nmv3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )nmv4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )nmv5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )nmv6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )nmv7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )nmv8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8_dec));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8_dec));

    CP24Time2a time1_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    CP24Time2a time2_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    CP24Time2a time3_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv3_dec);
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
    TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

    CP24Time2a time4_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv4_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time4_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time4_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time4_dec));

    CP24Time2a time5_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv5_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time5_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time5_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time5_dec));

    CP24Time2a time6_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv6_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time6_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time6_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time6_dec));

    CP24Time2a time7_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv7_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time7_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time7_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time7_dec));

    CP24Time2a time8_dec = MeasuredValueNormalizedWithCP24Time2a_getTimestamp(nmv8_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time8_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time8_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time8_dec));

    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv1_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv2_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv3_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv4_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv5_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv6_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv7_dec);
    MeasuredValueNormalizedWithCP24Time2a_destroy(nmv8_dec);

    CS101_ASDU_destroy(asdu2);
}

void

test_MeasuredValueNormalizedWithCP56Time2a(void)
{
    MeasuredValueNormalizedWithCP56Time2a nmv1;
    MeasuredValueNormalizedWithCP56Time2a nmv2;
    MeasuredValueNormalizedWithCP56Time2a nmv3;
    MeasuredValueNormalizedWithCP56Time2a nmv4;
    MeasuredValueNormalizedWithCP56Time2a nmv5;
    MeasuredValueNormalizedWithCP56Time2a nmv6;
    MeasuredValueNormalizedWithCP56Time2a nmv7;
    MeasuredValueNormalizedWithCP56Time2a nmv8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;
    struct sCP56Time2a cpTime4;
    struct sCP56Time2a cpTime5;
    struct sCP56Time2a cpTime6;
    struct sCP56Time2a cpTime7;
    struct sCP56Time2a cpTime8;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);
    CP56Time2a_createFromMsTimestamp(&cpTime4, time4);
    CP56Time2a_createFromMsTimestamp(&cpTime5, time5);
    CP56Time2a_createFromMsTimestamp(&cpTime6, time6);
    CP56Time2a_createFromMsTimestamp(&cpTime7, time7);
    CP56Time2a_createFromMsTimestamp(&cpTime8, time8);

    nmv1 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 101, -0.5f, IEC60870_QUALITY_GOOD, &cpTime1);
    nmv2 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 102, -0.2f, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    nmv3 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 103, -0.1f, IEC60870_QUALITY_RESERVED, &cpTime3);
    nmv4 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 104, 0, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    nmv5 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 105, 0.2f, IEC60870_QUALITY_BLOCKED, &cpTime5);
    nmv6 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 106, 0.3f, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    nmv7 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 107, 0.4f, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    nmv8 = MeasuredValueNormalizedWithCP56Time2a_create(NULL, 108, 0.5f, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) nmv8);

    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv1);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv2);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv3);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv4);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv5);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv6);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv7);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(110, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueNormalizedWithCP56Time2a nmv1_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueNormalizedWithCP56Time2a nmv2_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueNormalizedWithCP56Time2a nmv3_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueNormalizedWithCP56Time2a nmv4_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueNormalizedWithCP56Time2a nmv5_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueNormalizedWithCP56Time2a nmv6_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueNormalizedWithCP56Time2a nmv7_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueNormalizedWithCP56Time2a nmv8_dec = (MeasuredValueNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )nmv1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )nmv2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )nmv3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )nmv4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )nmv5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )nmv6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )nmv7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )nmv8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueNormalized_getQuality((MeasuredValueNormalized )nmv8_dec));

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv1_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv2_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.1f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv3_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv4_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.2f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv5_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv6_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv7_dec));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, MeasuredValueNormalized_getValue((MeasuredValueNormalized )nmv8_dec));

    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv1_dec)));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv2_dec)));
    TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv3_dec)));
    TEST_ASSERT_EQUAL_UINT64(time4, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv4_dec)));
    TEST_ASSERT_EQUAL_UINT64(time5, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv5_dec)));
    TEST_ASSERT_EQUAL_UINT64(time6, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv6_dec)));
    TEST_ASSERT_EQUAL_UINT64(time7, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv7_dec)));
    TEST_ASSERT_EQUAL_UINT64(time8, CP56Time2a_toMsTimestamp(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(nmv8_dec)));

    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv1_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv2_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv3_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv4_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv5_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv6_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv7_dec);
    MeasuredValueNormalizedWithCP56Time2a_destroy(nmv8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueScaled(void)
{
    MeasuredValueScaled mvs1;
    MeasuredValueScaled mvs2;
    MeasuredValueScaled mvs3;
    MeasuredValueScaled mvs4;
    MeasuredValueScaled mvs5;
    MeasuredValueScaled mvs6;
    MeasuredValueScaled mvs7;
    MeasuredValueScaled mvs8;

    mvs1 = MeasuredValueScaled_create(NULL, 101, INT16_MAX, IEC60870_QUALITY_GOOD);
    mvs2 = MeasuredValueScaled_create(NULL, 102, INT16_MAX, IEC60870_QUALITY_OVERFLOW);
    mvs3 = MeasuredValueScaled_create(NULL, 103, INT16_MAX, IEC60870_QUALITY_RESERVED);
    mvs4 = MeasuredValueScaled_create(NULL, 104, INT16_MAX, IEC60870_QUALITY_ELAPSED_TIME_INVALID);
    mvs5 = MeasuredValueScaled_create(NULL, 105, INT16_MIN, IEC60870_QUALITY_BLOCKED);
    mvs6 = MeasuredValueScaled_create(NULL, 106, INT16_MIN, IEC60870_QUALITY_SUBSTITUTED);
    mvs7 = MeasuredValueScaled_create(NULL, 107, INT16_MIN, IEC60870_QUALITY_NON_TOPICAL);
    mvs8 = MeasuredValueScaled_create(NULL, 108, INT16_MIN, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueScaled_destroy(mvs1);
    MeasuredValueScaled_destroy(mvs2);
    MeasuredValueScaled_destroy(mvs3);
    MeasuredValueScaled_destroy(mvs4);
    MeasuredValueScaled_destroy(mvs5);
    MeasuredValueScaled_destroy(mvs6);
    MeasuredValueScaled_destroy(mvs7);
    MeasuredValueScaled_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(54, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueScaled mvs1_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueScaled mvs2_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueScaled mvs3_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueScaled mvs4_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueScaled mvs5_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueScaled mvs6_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueScaled mvs7_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueScaled mvs8_dec = (MeasuredValueScaled) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8_dec));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8_dec));

    MeasuredValueScaled_destroy(mvs1_dec);
    MeasuredValueScaled_destroy(mvs2_dec);
    MeasuredValueScaled_destroy(mvs3_dec);
    MeasuredValueScaled_destroy(mvs4_dec);
    MeasuredValueScaled_destroy(mvs5_dec);
    MeasuredValueScaled_destroy(mvs6_dec);
    MeasuredValueScaled_destroy(mvs7_dec);
    MeasuredValueScaled_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void

test_MeasuredValueScaledWithCP24Time2a(void)
{
    MeasuredValueScaledWithCP24Time2a mvs1;
    MeasuredValueScaledWithCP24Time2a mvs2;
    MeasuredValueScaledWithCP24Time2a mvs3;
    MeasuredValueScaledWithCP24Time2a mvs4;
    MeasuredValueScaledWithCP24Time2a mvs5;
    MeasuredValueScaledWithCP24Time2a mvs6;
    MeasuredValueScaledWithCP24Time2a mvs7;
    MeasuredValueScaledWithCP24Time2a mvs8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;
    struct sCP24Time2a cpTime3;
    struct sCP24Time2a cpTime4;
    struct sCP24Time2a cpTime5;
    struct sCP24Time2a cpTime6;
    struct sCP24Time2a cpTime7;
    struct sCP24Time2a cpTime8;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));
    bzero(&cpTime3, sizeof(struct sCP24Time2a));
    bzero(&cpTime4, sizeof(struct sCP24Time2a));
    bzero(&cpTime5, sizeof(struct sCP24Time2a));
    bzero(&cpTime6, sizeof(struct sCP24Time2a));
    bzero(&cpTime7, sizeof(struct sCP24Time2a));
    bzero(&cpTime8, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    CP24Time2a_setMinute(&cpTime3, 00);
    CP24Time2a_setMillisecond(&cpTime3, 00001);

    CP24Time2a_setMinute(&cpTime4, 12);
    CP24Time2a_setMillisecond(&cpTime4, 24123);

    CP24Time2a_setMinute(&cpTime5, 12);
    CP24Time2a_setMillisecond(&cpTime5, 24123);

    CP24Time2a_setMinute(&cpTime6, 12);
    CP24Time2a_setMillisecond(&cpTime6, 24123);

    CP24Time2a_setMinute(&cpTime7, 12);
    CP24Time2a_setMillisecond(&cpTime7, 24123);

    CP24Time2a_setMinute(&cpTime8, 12);
    CP24Time2a_setMillisecond(&cpTime8, 24123);

    mvs1 = MeasuredValueScaledWithCP24Time2a_create(NULL, 101, INT16_MAX, IEC60870_QUALITY_GOOD, &cpTime1);
    mvs2 = MeasuredValueScaledWithCP24Time2a_create(NULL, 102, INT16_MAX, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    mvs3 = MeasuredValueScaledWithCP24Time2a_create(NULL, 103, INT16_MAX, IEC60870_QUALITY_RESERVED, &cpTime3);
    mvs4 = MeasuredValueScaledWithCP24Time2a_create(NULL, 104, INT16_MAX, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    mvs5 = MeasuredValueScaledWithCP24Time2a_create(NULL, 105, INT16_MIN, IEC60870_QUALITY_BLOCKED, &cpTime5);
    mvs6 = MeasuredValueScaledWithCP24Time2a_create(NULL, 106, INT16_MIN, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    mvs7 = MeasuredValueScaledWithCP24Time2a_create(NULL, 107, INT16_MIN, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    mvs8 = MeasuredValueScaledWithCP24Time2a_create(NULL, 108, INT16_MIN, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueScaledWithCP24Time2a_destroy(mvs1);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs2);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs3);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs4);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs5);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs6);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs7);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(78, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueScaledWithCP24Time2a mvs1_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueScaledWithCP24Time2a mvs2_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueScaledWithCP24Time2a mvs3_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueScaledWithCP24Time2a mvs4_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueScaledWithCP24Time2a mvs5_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueScaledWithCP24Time2a mvs6_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueScaledWithCP24Time2a mvs7_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueScaledWithCP24Time2a mvs8_dec = (MeasuredValueScaledWithCP24Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8_dec));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8_dec));

    CP24Time2a time1_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    CP24Time2a time2_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    CP24Time2a time3_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs3_dec);
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
    TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

    CP24Time2a time4_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs4_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time4_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time4_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time4_dec));

    CP24Time2a time5_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs5_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time5_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time5_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time5_dec));

    CP24Time2a time6_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs6_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time6_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time6_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time6_dec));

    CP24Time2a time7_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs7_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time7_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time7_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time7_dec));

    CP24Time2a time8_dec = MeasuredValueScaledWithCP24Time2a_getTimestamp(mvs8_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time8_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time8_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time8_dec));

    MeasuredValueScaledWithCP24Time2a_destroy(mvs1_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs2_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs3_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs4_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs5_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs6_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs7_dec);
    MeasuredValueScaledWithCP24Time2a_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueScaledWithCP56Time2a(void)
{
    MeasuredValueScaledWithCP56Time2a mvs1;
    MeasuredValueScaledWithCP56Time2a mvs2;
    MeasuredValueScaledWithCP56Time2a mvs3;
    MeasuredValueScaledWithCP56Time2a mvs4;
    MeasuredValueScaledWithCP56Time2a mvs5;
    MeasuredValueScaledWithCP56Time2a mvs6;
    MeasuredValueScaledWithCP56Time2a mvs7;
    MeasuredValueScaledWithCP56Time2a mvs8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;
    struct sCP56Time2a cpTime4;
    struct sCP56Time2a cpTime5;
    struct sCP56Time2a cpTime6;
    struct sCP56Time2a cpTime7;
    struct sCP56Time2a cpTime8;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);
    CP56Time2a_createFromMsTimestamp(&cpTime4, time4);
    CP56Time2a_createFromMsTimestamp(&cpTime5, time5);
    CP56Time2a_createFromMsTimestamp(&cpTime6, time6);
    CP56Time2a_createFromMsTimestamp(&cpTime7, time7);
    CP56Time2a_createFromMsTimestamp(&cpTime8, time8);

    mvs1 = MeasuredValueScaledWithCP56Time2a_create(NULL, 101, INT16_MAX, IEC60870_QUALITY_GOOD, &cpTime1);
    mvs2 = MeasuredValueScaledWithCP56Time2a_create(NULL, 102, INT16_MAX, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    mvs3 = MeasuredValueScaledWithCP56Time2a_create(NULL, 103, INT16_MAX, IEC60870_QUALITY_RESERVED, &cpTime3);
    mvs4 = MeasuredValueScaledWithCP56Time2a_create(NULL, 104, INT16_MAX, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    mvs5 = MeasuredValueScaledWithCP56Time2a_create(NULL, 105, INT16_MIN, IEC60870_QUALITY_BLOCKED, &cpTime5);
    mvs6 = MeasuredValueScaledWithCP56Time2a_create(NULL, 106, INT16_MIN, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    mvs7 = MeasuredValueScaledWithCP56Time2a_create(NULL, 107, INT16_MIN, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    mvs8 = MeasuredValueScaledWithCP56Time2a_create(NULL, 108, INT16_MIN, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueScaledWithCP56Time2a_destroy(mvs1);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs2);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs3);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs4);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs5);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs6);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs7);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(110, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueScaledWithCP56Time2a mvs1_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueScaledWithCP56Time2a mvs2_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueScaledWithCP56Time2a mvs3_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueScaledWithCP56Time2a mvs4_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueScaledWithCP56Time2a mvs5_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueScaledWithCP56Time2a mvs6_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueScaledWithCP56Time2a mvs7_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueScaledWithCP56Time2a mvs8_dec = (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(104, InformationObject_getObjectAddress((InformationObject )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(105, InformationObject_getObjectAddress((InformationObject )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(106, InformationObject_getObjectAddress((InformationObject )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(107, InformationObject_getObjectAddress((InformationObject )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(108, InformationObject_getObjectAddress((InformationObject )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueScaled_getQuality((MeasuredValueScaled )mvs8_dec));

    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs1_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs2_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs3_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MAX, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs4_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs5_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs6_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs7_dec));
    TEST_ASSERT_EQUAL_INT(INT16_MIN, MeasuredValueScaled_getValue((MeasuredValueScaled )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs1_dec)));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs2_dec)));
    TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs3_dec)));
    TEST_ASSERT_EQUAL_UINT64(time4, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs4_dec)));
    TEST_ASSERT_EQUAL_UINT64(time5, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs5_dec)));
    TEST_ASSERT_EQUAL_UINT64(time6, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs6_dec)));
    TEST_ASSERT_EQUAL_UINT64(time7, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs7_dec)));
    TEST_ASSERT_EQUAL_UINT64(time8, CP56Time2a_toMsTimestamp(MeasuredValueScaledWithCP56Time2a_getTimestamp(mvs8_dec)));

    MeasuredValueScaledWithCP56Time2a_destroy(mvs1_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs2_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs3_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs4_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs5_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs6_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs7_dec);
    MeasuredValueScaledWithCP56Time2a_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueShort(void)
{
    MeasuredValueShort mvs1;
    MeasuredValueShort mvs2;
    MeasuredValueShort mvs3;
    MeasuredValueShort mvs4;
    MeasuredValueShort mvs5;
    MeasuredValueShort mvs6;
    MeasuredValueShort mvs7;
    MeasuredValueShort mvs8;

    mvs1 = MeasuredValueShort_create(NULL, 101, 10.5f, IEC60870_QUALITY_GOOD);
    mvs2 = MeasuredValueShort_create(NULL, 102, 11.5f, IEC60870_QUALITY_OVERFLOW);
    mvs3 = MeasuredValueShort_create(NULL, 103, 12.5f, IEC60870_QUALITY_RESERVED);
    mvs4 = MeasuredValueShort_create(NULL, 104, 13.5f, IEC60870_QUALITY_ELAPSED_TIME_INVALID);
    mvs5 = MeasuredValueShort_create(NULL, 105, 14.5f, IEC60870_QUALITY_BLOCKED);
    mvs6 = MeasuredValueShort_create(NULL, 106, 15.5f, IEC60870_QUALITY_SUBSTITUTED);
    mvs7 = MeasuredValueShort_create(NULL, 107, 16.5f, IEC60870_QUALITY_NON_TOPICAL);
    mvs8 = MeasuredValueShort_create(NULL, 108, 17.5f, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality(mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality(mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality(mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality(mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality(mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality(mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality(mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality(mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueShort_destroy(mvs1);
    MeasuredValueShort_destroy(mvs2);
    MeasuredValueShort_destroy(mvs3);
    MeasuredValueShort_destroy(mvs4);
    MeasuredValueShort_destroy(mvs5);
    MeasuredValueShort_destroy(mvs6);
    MeasuredValueShort_destroy(mvs7);
    MeasuredValueShort_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(70, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueShort mvs1_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueShort mvs2_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueShort mvs3_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueShort mvs4_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueShort mvs5_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueShort mvs6_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueShort mvs7_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueShort mvs8_dec = (MeasuredValueShort) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1_dec));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2_dec));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3_dec));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4_dec));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5_dec));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6_dec));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7_dec));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality(mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality(mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality(mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality(mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality(mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality(mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality(mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality(mvs8_dec));

    MeasuredValueShort_destroy(mvs1_dec);
    MeasuredValueShort_destroy(mvs2_dec);
    MeasuredValueShort_destroy(mvs3_dec);
    MeasuredValueShort_destroy(mvs4_dec);
    MeasuredValueShort_destroy(mvs5_dec);
    MeasuredValueShort_destroy(mvs6_dec);
    MeasuredValueShort_destroy(mvs7_dec);
    MeasuredValueShort_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueShortWithCP24Time2a(void)
{
    MeasuredValueShortWithCP24Time2a mvs1;
    MeasuredValueShortWithCP24Time2a mvs2;
    MeasuredValueShortWithCP24Time2a mvs3;
    MeasuredValueShortWithCP24Time2a mvs4;
    MeasuredValueShortWithCP24Time2a mvs5;
    MeasuredValueShortWithCP24Time2a mvs6;
    MeasuredValueShortWithCP24Time2a mvs7;
    MeasuredValueShortWithCP24Time2a mvs8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;
    struct sCP24Time2a cpTime3;
    struct sCP24Time2a cpTime4;
    struct sCP24Time2a cpTime5;
    struct sCP24Time2a cpTime6;
    struct sCP24Time2a cpTime7;
    struct sCP24Time2a cpTime8;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));
    bzero(&cpTime3, sizeof(struct sCP24Time2a));
    bzero(&cpTime4, sizeof(struct sCP24Time2a));
    bzero(&cpTime5, sizeof(struct sCP24Time2a));
    bzero(&cpTime6, sizeof(struct sCP24Time2a));
    bzero(&cpTime7, sizeof(struct sCP24Time2a));
    bzero(&cpTime8, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    CP24Time2a_setMinute(&cpTime3, 00);
    CP24Time2a_setMillisecond(&cpTime3, 00001);

    CP24Time2a_setMinute(&cpTime4, 12);
    CP24Time2a_setMillisecond(&cpTime4, 24123);

    CP24Time2a_setMinute(&cpTime5, 12);
    CP24Time2a_setMillisecond(&cpTime5, 24123);

    CP24Time2a_setMinute(&cpTime6, 12);
    CP24Time2a_setMillisecond(&cpTime6, 24123);

    CP24Time2a_setMinute(&cpTime7, 12);
    CP24Time2a_setMillisecond(&cpTime7, 24123);

    CP24Time2a_setMinute(&cpTime8, 12);
    CP24Time2a_setMillisecond(&cpTime8, 24123);

    mvs1 = MeasuredValueShortWithCP24Time2a_create(NULL, 101, 10.5f, IEC60870_QUALITY_GOOD, &cpTime1);
    mvs2 = MeasuredValueShortWithCP24Time2a_create(NULL, 102, 11.5f, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    mvs3 = MeasuredValueShortWithCP24Time2a_create(NULL, 103, 12.5f, IEC60870_QUALITY_RESERVED, &cpTime3);
    mvs4 = MeasuredValueShortWithCP24Time2a_create(NULL, 104, 13.5f, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    mvs5 = MeasuredValueShortWithCP24Time2a_create(NULL, 105, 14.5f, IEC60870_QUALITY_BLOCKED, &cpTime5);
    mvs6 = MeasuredValueShortWithCP24Time2a_create(NULL, 106, 15.5f, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    mvs7 = MeasuredValueShortWithCP24Time2a_create(NULL, 107, 16.5f, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    mvs8 = MeasuredValueShortWithCP24Time2a_create(NULL, 108, 17.5f, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality((MeasuredValueShort )mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality((MeasuredValueShort )mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality((MeasuredValueShort )mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueShortWithCP24Time2a_destroy(mvs1);
    MeasuredValueShortWithCP24Time2a_destroy(mvs2);
    MeasuredValueShortWithCP24Time2a_destroy(mvs3);
    MeasuredValueShortWithCP24Time2a_destroy(mvs4);
    MeasuredValueShortWithCP24Time2a_destroy(mvs5);
    MeasuredValueShortWithCP24Time2a_destroy(mvs6);
    MeasuredValueShortWithCP24Time2a_destroy(mvs7);
    MeasuredValueShortWithCP24Time2a_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(94, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueShortWithCP24Time2a mvs1_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueShortWithCP24Time2a mvs2_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueShortWithCP24Time2a mvs3_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueShortWithCP24Time2a mvs4_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueShortWithCP24Time2a mvs5_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueShortWithCP24Time2a mvs6_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueShortWithCP24Time2a mvs7_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueShortWithCP24Time2a mvs8_dec = (MeasuredValueShortWithCP24Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1_dec));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2_dec));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3_dec));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4_dec));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5_dec));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6_dec));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7_dec));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality((MeasuredValueShort )mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality((MeasuredValueShort )mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality((MeasuredValueShort )mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs8_dec));

    CP24Time2a time1_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    CP24Time2a time2_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    CP24Time2a time3_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs3_dec);
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getMinute(time3_dec));
    TEST_ASSERT_EQUAL_INT(00, CP24Time2a_getSecond(time3_dec));
    TEST_ASSERT_EQUAL_INT(1, CP24Time2a_getMillisecond(time3_dec));

    CP24Time2a time4_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs4_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time4_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time4_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time4_dec));

    CP24Time2a time5_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs5_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time5_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time5_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time5_dec));

    CP24Time2a time6_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs6_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time6_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time6_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time6_dec));

    CP24Time2a time7_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs7_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time7_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time7_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time7_dec));

    CP24Time2a time8_dec = MeasuredValueShortWithCP24Time2a_getTimestamp(mvs8_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time8_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time8_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time8_dec));

    MeasuredValueShortWithCP24Time2a_destroy(mvs1_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs2_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs3_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs4_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs5_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs6_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs7_dec);
    MeasuredValueShortWithCP24Time2a_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_MeasuredValueShortWithCP56Time2a(void)
{
    MeasuredValueShortWithCP56Time2a mvs1;
    MeasuredValueShortWithCP56Time2a mvs2;
    MeasuredValueShortWithCP56Time2a mvs3;
    MeasuredValueShortWithCP56Time2a mvs4;
    MeasuredValueShortWithCP56Time2a mvs5;
    MeasuredValueShortWithCP56Time2a mvs6;
    MeasuredValueShortWithCP56Time2a mvs7;
    MeasuredValueShortWithCP56Time2a mvs8;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;
    uint64_t time3 = time2 + 1000;
    uint64_t time4 = time3 + 1000;
    uint64_t time5 = time4 + 1000;
    uint64_t time6 = time5 + 1000;
    uint64_t time7 = time6 + 1000;
    uint64_t time8 = time7 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;
    struct sCP56Time2a cpTime3;
    struct sCP56Time2a cpTime4;
    struct sCP56Time2a cpTime5;
    struct sCP56Time2a cpTime6;
    struct sCP56Time2a cpTime7;
    struct sCP56Time2a cpTime8;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);
    CP56Time2a_createFromMsTimestamp(&cpTime3, time3);
    CP56Time2a_createFromMsTimestamp(&cpTime4, time4);
    CP56Time2a_createFromMsTimestamp(&cpTime5, time5);
    CP56Time2a_createFromMsTimestamp(&cpTime6, time6);
    CP56Time2a_createFromMsTimestamp(&cpTime7, time7);
    CP56Time2a_createFromMsTimestamp(&cpTime8, time8);

    mvs1 = MeasuredValueShortWithCP56Time2a_create(NULL, 101, 10.5f, IEC60870_QUALITY_GOOD, &cpTime1);
    mvs2 = MeasuredValueShortWithCP56Time2a_create(NULL, 102, 11.5f, IEC60870_QUALITY_OVERFLOW, &cpTime2);
    mvs3 = MeasuredValueShortWithCP56Time2a_create(NULL, 103, 12.5f, IEC60870_QUALITY_RESERVED, &cpTime3);
    mvs4 = MeasuredValueShortWithCP56Time2a_create(NULL, 104, 13.5f, IEC60870_QUALITY_ELAPSED_TIME_INVALID, &cpTime4);
    mvs5 = MeasuredValueShortWithCP56Time2a_create(NULL, 105, 14.5f, IEC60870_QUALITY_BLOCKED, &cpTime5);
    mvs6 = MeasuredValueShortWithCP56Time2a_create(NULL, 106, 15.5f, IEC60870_QUALITY_SUBSTITUTED, &cpTime6);
    mvs7 = MeasuredValueShortWithCP56Time2a_create(NULL, 107, 16.5f, IEC60870_QUALITY_NON_TOPICAL, &cpTime7);
    mvs8 = MeasuredValueShortWithCP56Time2a_create(NULL, 108, 17.5f, IEC60870_QUALITY_INVALID, &cpTime8);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality((MeasuredValueShort )mvs1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality((MeasuredValueShort )mvs2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs3));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs4));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs5));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs6));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality((MeasuredValueShort )mvs7));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs8));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs3);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs4);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs5);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs6);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs7);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) mvs8);

    MeasuredValueShortWithCP56Time2a_destroy(mvs1);
    MeasuredValueShortWithCP56Time2a_destroy(mvs2);
    MeasuredValueShortWithCP56Time2a_destroy(mvs3);
    MeasuredValueShortWithCP56Time2a_destroy(mvs4);
    MeasuredValueShortWithCP56Time2a_destroy(mvs5);
    MeasuredValueShortWithCP56Time2a_destroy(mvs6);
    MeasuredValueShortWithCP56Time2a_destroy(mvs7);
    MeasuredValueShortWithCP56Time2a_destroy(mvs8);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(126, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(8, CS101_ASDU_getNumberOfElements(asdu2));

    MeasuredValueShortWithCP56Time2a mvs1_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    MeasuredValueShortWithCP56Time2a mvs2_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);
    MeasuredValueShortWithCP56Time2a mvs3_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 2);
    MeasuredValueShortWithCP56Time2a mvs4_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 3);
    MeasuredValueShortWithCP56Time2a mvs5_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 4);
    MeasuredValueShortWithCP56Time2a mvs6_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 5);
    MeasuredValueShortWithCP56Time2a mvs7_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 6);
    MeasuredValueShortWithCP56Time2a mvs8_dec = (MeasuredValueShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 7);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs1_dec));
    TEST_ASSERT_EQUAL_FLOAT(11.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs2_dec));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs3_dec));
    TEST_ASSERT_EQUAL_FLOAT(13.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs4_dec));
    TEST_ASSERT_EQUAL_FLOAT(14.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs5_dec));
    TEST_ASSERT_EQUAL_FLOAT(15.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs6_dec));
    TEST_ASSERT_EQUAL_FLOAT(16.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs7_dec));
    TEST_ASSERT_EQUAL_FLOAT(17.5f, MeasuredValueShort_getValue((MeasuredValueShort )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, MeasuredValueShort_getQuality((MeasuredValueShort )mvs1_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_OVERFLOW, MeasuredValueShort_getQuality((MeasuredValueShort )mvs2_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_RESERVED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs3_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_ELAPSED_TIME_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs4_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs5_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_SUBSTITUTED, MeasuredValueShort_getQuality((MeasuredValueShort )mvs6_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_NON_TOPICAL, MeasuredValueShort_getQuality((MeasuredValueShort )mvs7_dec));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, MeasuredValueShort_getQuality((MeasuredValueShort )mvs8_dec));

    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs1_dec)));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs2_dec)));
    TEST_ASSERT_EQUAL_UINT64(time3, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs3_dec)));
    TEST_ASSERT_EQUAL_UINT64(time4, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs4_dec)));
    TEST_ASSERT_EQUAL_UINT64(time5, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs5_dec)));
    TEST_ASSERT_EQUAL_UINT64(time6, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs6_dec)));
    TEST_ASSERT_EQUAL_UINT64(time7, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs7_dec)));
    TEST_ASSERT_EQUAL_UINT64(time8, CP56Time2a_toMsTimestamp(MeasuredValueShortWithCP56Time2a_getTimestamp(mvs8_dec)));

    MeasuredValueShortWithCP56Time2a_destroy(mvs1_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs2_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs3_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs4_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs5_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs6_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs7_dec);
    MeasuredValueShortWithCP56Time2a_destroy(mvs8_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_IntegratedTotals(void)
{
    IntegratedTotals it1;
    IntegratedTotals it2;
    BinaryCounterReading bcr1;
    BinaryCounterReading bcr2;

    bcr1 = BinaryCounterReading_create(NULL, INT32_MAX, 0, true, true, true);
    bcr2 = BinaryCounterReading_create(NULL, INT32_MIN, 15, false, false, false);
    it1 = IntegratedTotals_create(NULL, 101, bcr1);
    it2 = IntegratedTotals_create(NULL, 102, bcr2);

    BinaryCounterReading_destroy(bcr1);
    BinaryCounterReading_destroy(bcr2);

    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR(it1)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR(it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR(it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR(it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR(it1)));

    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR(it2)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR(it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR(it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR(it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR(it2)));
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) it1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) it2);

    IntegratedTotals_destroy(it1);
    IntegratedTotals_destroy(it2);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(22, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(2, CS101_ASDU_getNumberOfElements(asdu2));

    IntegratedTotals it1_dec = (IntegratedTotals) CS101_ASDU_getElement(asdu2, 0);
    IntegratedTotals it2_dec = (IntegratedTotals) CS101_ASDU_getElement(asdu2, 1);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )it1_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR(it1_dec)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR(it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR(it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR(it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR(it1_dec)));

    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )it2_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR(it2_dec)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR(it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR(it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR(it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR(it2_dec)));

    IntegratedTotals_destroy(it1_dec);
    IntegratedTotals_destroy(it2_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_IntegratedTotalsWithCP24Time2a(void)
{
    IntegratedTotalsWithCP24Time2a it1;
    IntegratedTotalsWithCP24Time2a it2;
    BinaryCounterReading bcr1;
    BinaryCounterReading bcr2;
    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;

    struct sCP24Time2a cpTime1;
    struct sCP24Time2a cpTime2;

    bzero(&cpTime1, sizeof(struct sCP24Time2a));
    bzero(&cpTime2, sizeof(struct sCP24Time2a));

    CP24Time2a_setMinute(&cpTime1, 12);
    CP24Time2a_setMillisecond(&cpTime1, 24123);

    CP24Time2a_setMinute(&cpTime2, 54);
    CP24Time2a_setMillisecond(&cpTime2, 12345);

    bcr1 = BinaryCounterReading_create(NULL, INT32_MAX, 0, true, true, true);
    bcr2 = BinaryCounterReading_create(NULL, INT32_MIN, 15, false, false, false);
    it1 = IntegratedTotalsWithCP24Time2a_create(NULL, 101, bcr1, &cpTime1);
    it2 = IntegratedTotalsWithCP24Time2a_create(NULL, 102, bcr2, &cpTime2);

    BinaryCounterReading_destroy(bcr1);
    BinaryCounterReading_destroy(bcr2);

    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it1)));

    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) it1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) it2);

    IntegratedTotalsWithCP24Time2a_destroy(it1);
    IntegratedTotalsWithCP24Time2a_destroy(it2);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(28, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(2, CS101_ASDU_getNumberOfElements(asdu2));

    IntegratedTotalsWithCP24Time2a it1_dec = (IntegratedTotalsWithCP24Time2a) CS101_ASDU_getElement(asdu2, 0);
    IntegratedTotalsWithCP24Time2a it2_dec = (IntegratedTotalsWithCP24Time2a) CS101_ASDU_getElement(asdu2, 1);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )it1_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));

    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )it2_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));

    CP24Time2a time1_dec = IntegratedTotalsWithCP24Time2a_getTimestamp(it1_dec);
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getMinute(time1_dec));
    TEST_ASSERT_EQUAL_INT(24, CP24Time2a_getSecond(time1_dec));
    TEST_ASSERT_EQUAL_INT(123, CP24Time2a_getMillisecond(time1_dec));

    CP24Time2a time2_dec = IntegratedTotalsWithCP24Time2a_getTimestamp(it2_dec);
    TEST_ASSERT_EQUAL_INT(54, CP24Time2a_getMinute(time2_dec));
    TEST_ASSERT_EQUAL_INT(12, CP24Time2a_getSecond(time2_dec));
    TEST_ASSERT_EQUAL_INT(345, CP24Time2a_getMillisecond(time2_dec));

    IntegratedTotalsWithCP24Time2a_destroy(it1_dec);
    IntegratedTotalsWithCP24Time2a_destroy(it2_dec);
    CS101_ASDU_destroy(asdu2);
}

void
test_IntegratedTotalsWithCP56Time2a(void)
{
    IntegratedTotalsWithCP56Time2a it1;
    IntegratedTotalsWithCP56Time2a it2;
    BinaryCounterReading bcr1;
    BinaryCounterReading bcr2;

    uint64_t time1 = Hal_getTimeInMs();
    uint64_t time2 = time1 + 1000;

    struct sCP56Time2a cpTime1;
    struct sCP56Time2a cpTime2;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    CP56Time2a_createFromMsTimestamp(&cpTime2, time2);

    bcr1 = BinaryCounterReading_create(NULL, INT32_MAX, 0, true, true, true);
    bcr2 = BinaryCounterReading_create(NULL, INT32_MIN, 15, false, false, false);
    it1 = IntegratedTotalsWithCP56Time2a_create(NULL, 101, bcr1, &cpTime1);
    it2 = IntegratedTotalsWithCP56Time2a_create(NULL, 102, bcr2, &cpTime2);

    BinaryCounterReading_destroy(bcr1);
    BinaryCounterReading_destroy(bcr2);

    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it1)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it1)));

    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it2)));
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) it1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) it2);

    IntegratedTotalsWithCP56Time2a_destroy(it1);
    IntegratedTotalsWithCP56Time2a_destroy(it2);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(36, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(2, CS101_ASDU_getNumberOfElements(asdu2));

    IntegratedTotalsWithCP56Time2a it1_dec = (IntegratedTotalsWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);
    IntegratedTotalsWithCP56Time2a it2_dec = (IntegratedTotalsWithCP56Time2a) CS101_ASDU_getElement(asdu2, 1);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )it1_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_EQUAL_UINT8(0, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));
    TEST_ASSERT_TRUE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it1_dec)));

    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject )it2_dec));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, BinaryCounterReading_getValue(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_EQUAL_UINT8(15, BinaryCounterReading_getSequenceNumber(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_hasCarry(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isAdjusted(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));
    TEST_ASSERT_FALSE(BinaryCounterReading_isInvalid(IntegratedTotals_getBCR((IntegratedTotals )it2_dec)));

    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(IntegratedTotalsWithCP56Time2a_getTimestamp(it1_dec)));
    TEST_ASSERT_EQUAL_UINT64(time2, CP56Time2a_toMsTimestamp(IntegratedTotalsWithCP56Time2a_getTimestamp(it2_dec)));

    IntegratedTotalsWithCP56Time2a_destroy(it1_dec);
    IntegratedTotalsWithCP56Time2a_destroy(it2_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SingleCommand(void)
{
    SingleCommand sc;
    sc = SingleCommand_create(NULL, 101, true, true, 0);

    TEST_ASSERT_TRUE(SingleCommand_getState(sc));
    TEST_ASSERT_TRUE(SingleCommand_isSelect(sc));
    TEST_ASSERT_EQUAL_INT(0, SingleCommand_getQU(sc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) sc);

    SingleCommand_destroy(sc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SingleCommand sc_dec = (SingleCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )sc_dec));

    TEST_ASSERT_TRUE(SingleCommand_getState(sc_dec));
    TEST_ASSERT_TRUE(SingleCommand_isSelect(sc_dec));
    TEST_ASSERT_EQUAL_INT(0, SingleCommand_getQU(sc_dec));

    SingleCommand_destroy(sc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SingleCommandWithCP56Time2a(void)
{
    SingleCommandWithCP56Time2a sc;

    uint64_t time1 = Hal_getTimeInMs();

    struct sCP56Time2a cpTime1;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    sc = SingleCommandWithCP56Time2a_create(NULL, 101, true, true, 0, &cpTime1);

    TEST_ASSERT_TRUE(SingleCommand_getState((SingleCommand )sc));
    TEST_ASSERT_TRUE(SingleCommand_isSelect((SingleCommand )sc));
    TEST_ASSERT_EQUAL_INT(0, SingleCommand_getQU((SingleCommand )sc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) sc);

    SingleCommandWithCP56Time2a_destroy(sc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(17, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SingleCommandWithCP56Time2a sc_dec = (SingleCommandWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )sc_dec));

    TEST_ASSERT_TRUE(SingleCommand_getState((SingleCommand )sc_dec));
    TEST_ASSERT_TRUE(SingleCommand_isSelect((SingleCommand )sc_dec));
    TEST_ASSERT_EQUAL_INT(0, SingleCommand_getQU((SingleCommand )sc_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(SingleCommandWithCP56Time2a_getTimestamp(sc_dec)));

    SingleCommandWithCP56Time2a_destroy(sc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_DoubleCommand(void)
{
    DoubleCommand dc;
    dc = DoubleCommand_create(NULL, 101, 1, true, 0);

    TEST_ASSERT_TRUE(DoubleCommand_isSelect(dc));
    TEST_ASSERT_EQUAL_INT(1, DoubleCommand_getState(dc));
    TEST_ASSERT_EQUAL_INT(0, DoubleCommand_getQU(dc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dc);

    DoubleCommand_destroy(dc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    DoubleCommand dc_dec = (DoubleCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )dc_dec));

    TEST_ASSERT_TRUE(DoubleCommand_isSelect(dc_dec));
    TEST_ASSERT_EQUAL_INT(1, DoubleCommand_getState(dc_dec));
    TEST_ASSERT_EQUAL_INT(0, DoubleCommand_getQU(dc_dec));

    DoubleCommand_destroy(dc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_DoubleCommandWithCP56Time2a(void)
{
    DoubleCommandWithCP56Time2a dc;

    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    dc = DoubleCommandWithCP56Time2a_create(NULL, 101, 1, true, 0, &cpTime1);

    TEST_ASSERT_TRUE(DoubleCommandWithCP56Time2a_isSelect(dc));
    TEST_ASSERT_EQUAL_INT(1, DoubleCommandWithCP56Time2a_getState(dc));
    TEST_ASSERT_EQUAL_INT(0, DoubleCommandWithCP56Time2a_getQU(dc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dc);

    DoubleCommandWithCP56Time2a_destroy(dc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(17, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    DoubleCommandWithCP56Time2a dc_dec = (DoubleCommandWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )dc_dec));

    TEST_ASSERT_TRUE(DoubleCommandWithCP56Time2a_isSelect(dc_dec));
    TEST_ASSERT_EQUAL_INT(1, DoubleCommandWithCP56Time2a_getState(dc_dec));
    TEST_ASSERT_EQUAL_INT(0, DoubleCommandWithCP56Time2a_getQU(dc_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(DoubleCommandWithCP56Time2a_getTimestamp(dc_dec)));

    DoubleCommandWithCP56Time2a_destroy(dc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_StepCommandValue(void)
{
    StepCommand scv;
    scv = StepCommand_create(NULL, 101, IEC60870_STEP_INVALID_0, true, 0);

    TEST_ASSERT_TRUE(StepCommand_isSelect(scv));
    TEST_ASSERT_EQUAL_INT(IEC60870_STEP_INVALID_0, StepCommand_getState(scv));
    TEST_ASSERT_EQUAL_INT(0, StepCommand_getQU(scv));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) scv);

    StepCommand_destroy(scv);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    StepCommand scv_dec = (StepCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )scv_dec));

    TEST_ASSERT_TRUE(StepCommand_isSelect(scv_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_STEP_INVALID_0, StepCommand_getState(scv_dec));
    TEST_ASSERT_EQUAL_INT(0, StepCommand_getQU(scv_dec));

    StepCommand_destroy(scv_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_StepCommandWithCP56Time2a(void)
{
    StepCommandWithCP56Time2a scv;
    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    scv = StepCommandWithCP56Time2a_create(NULL, 101, IEC60870_STEP_INVALID_0, true, 0, &cpTime1);

    TEST_ASSERT_TRUE(StepCommandWithCP56Time2a_isSelect(scv));
    TEST_ASSERT_EQUAL_INT(IEC60870_STEP_INVALID_0, StepCommandWithCP56Time2a_getState(scv));
    TEST_ASSERT_EQUAL_INT(0, StepCommandWithCP56Time2a_getQU(scv));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) scv);

    StepCommandWithCP56Time2a_destroy((StepCommandWithCP56Time2a) scv);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(17, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    StepCommandWithCP56Time2a scv_dec = (StepCommandWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )scv_dec));

    TEST_ASSERT_TRUE(StepCommandWithCP56Time2a_isSelect(scv_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_STEP_INVALID_0, StepCommandWithCP56Time2a_getState(scv_dec));
    TEST_ASSERT_EQUAL_INT(0, StepCommandWithCP56Time2a_getQU(scv_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(StepCommandWithCP56Time2a_getTimestamp(scv_dec)));

    StepCommandWithCP56Time2a_destroy((StepCommandWithCP56Time2a) scv_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandNormalized(void)
{
    SetpointCommandNormalized spcn;
    spcn = SetpointCommandNormalized_create(NULL, 101, -1, true, 0);

    TEST_ASSERT_EQUAL_INT(-1, SetpointCommandNormalized_getValue(spcn));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalized_getQL(spcn));
    TEST_ASSERT_TRUE(SetpointCommandNormalized_isSelect(spcn));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcn);

    SetpointCommandNormalized_destroy(spcn);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(12, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandNormalized spcn_dec = (SetpointCommandNormalized) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcn_dec));

    TEST_ASSERT_EQUAL_INT(-1, SetpointCommandNormalized_getValue(spcn_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalized_getQL(spcn_dec));
    TEST_ASSERT_TRUE(SetpointCommandNormalized_isSelect(spcn_dec));

    SetpointCommandNormalized_destroy(spcn_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandNormalizedWithCP56Time2a(void)
{
    SetpointCommandNormalizedWithCP56Time2a spcn;
    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    spcn = SetpointCommandNormalizedWithCP56Time2a_create(NULL, 101, 0, true, 0, &cpTime1);

    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalizedWithCP56Time2a_getValue(spcn));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalizedWithCP56Time2a_getQL(spcn));
    TEST_ASSERT_TRUE(SetpointCommandNormalizedWithCP56Time2a_isSelect(spcn));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcn);

    SetpointCommandNormalizedWithCP56Time2a_destroy(spcn);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(19, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandNormalizedWithCP56Time2a spcn_dec = (SetpointCommandNormalizedWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcn_dec));

    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalizedWithCP56Time2a_getValue(spcn_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandNormalizedWithCP56Time2a_getQL(spcn_dec));
    TEST_ASSERT_TRUE(SetpointCommandNormalizedWithCP56Time2a_isSelect(spcn_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(SetpointCommandNormalizedWithCP56Time2a_getTimestamp(spcn_dec)));

    SetpointCommandNormalizedWithCP56Time2a_destroy(spcn_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandScaled(void)
{
    SetpointCommandScaled spcs;
    spcs = SetpointCommandScaled_create(NULL, 101, -32768, true, 0);

    TEST_ASSERT_EQUAL_INT(-32768, SetpointCommandScaled_getValue(spcs));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandScaled_getQL(spcs));
    TEST_ASSERT_TRUE(SetpointCommandScaled_isSelect(spcs));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcs);

    SetpointCommandScaled_destroy(spcs);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(12, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandScaled spcs_dec = (SetpointCommandScaled) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcs_dec));

    TEST_ASSERT_EQUAL_INT(-32768, SetpointCommandScaled_getValue(spcs_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandScaled_getQL(spcs_dec));
    TEST_ASSERT_TRUE(SetpointCommandScaled_isSelect(spcs_dec));

    SetpointCommandScaled_destroy(spcs_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandScaledWithCP56Time2a(void)
{
    SetpointCommandScaledWithCP56Time2a spcs;

    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    spcs = SetpointCommandScaledWithCP56Time2a_create(NULL, 101, -32768, true, 0, &cpTime1);

    TEST_ASSERT_EQUAL_INT(-32768, SetpointCommandScaledWithCP56Time2a_getValue(spcs));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandScaledWithCP56Time2a_getQL(spcs));
    TEST_ASSERT_TRUE(SetpointCommandScaledWithCP56Time2a_isSelect(spcs));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcs);

    SetpointCommandScaledWithCP56Time2a_destroy(spcs);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(19, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandScaledWithCP56Time2a spcs_dec = (SetpointCommandScaledWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcs_dec));

    TEST_ASSERT_EQUAL_INT(-32768, SetpointCommandScaledWithCP56Time2a_getValue(spcs_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandScaledWithCP56Time2a_getQL(spcs_dec));
    TEST_ASSERT_TRUE(SetpointCommandScaledWithCP56Time2a_isSelect(spcs_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(SetpointCommandScaledWithCP56Time2a_getTimestamp(spcs_dec)));

    SetpointCommandScaledWithCP56Time2a_destroy(spcs_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandShort(void)
{
    SetpointCommandShort spcs;
    spcs = SetpointCommandShort_create(NULL, 101, 10.5f, true, 0);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, SetpointCommandShort_getValue(spcs));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandShort_getQL(spcs));
    TEST_ASSERT_TRUE(SetpointCommandShort_isSelect(spcs));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcs);

    SetpointCommandShort_destroy(spcs);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(14, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandShort spcs_dec = (SetpointCommandShort) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcs_dec));

    TEST_ASSERT_EQUAL_FLOAT(10.5f, SetpointCommandShort_getValue(spcs_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandShort_getQL(spcs_dec));
    TEST_ASSERT_TRUE(SetpointCommandShort_isSelect(spcs_dec));

    SetpointCommandShort_destroy(spcs_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_SetpointCommandShortWithCP56Time2a(void)
{
    SetpointCommandShortWithCP56Time2a spcs;
    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    spcs = SetpointCommandShortWithCP56Time2a_create(NULL, 101, 10.5f, true, 0, &cpTime1);

    TEST_ASSERT_EQUAL_FLOAT(10.5f, SetpointCommandShortWithCP56Time2a_getValue(spcs));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandShortWithCP56Time2a_getQL(spcs));
    TEST_ASSERT_TRUE(SetpointCommandShortWithCP56Time2a_isSelect(spcs));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spcs);

    SetpointCommandShortWithCP56Time2a_destroy(spcs);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(21, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    SetpointCommandShortWithCP56Time2a spcs_dec = (SetpointCommandShortWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )spcs_dec));

    TEST_ASSERT_EQUAL_FLOAT(10.5f, SetpointCommandShortWithCP56Time2a_getValue(spcs_dec));
    TEST_ASSERT_EQUAL_INT(0, SetpointCommandShortWithCP56Time2a_getQL(spcs_dec));
    TEST_ASSERT_TRUE(SetpointCommandShortWithCP56Time2a_isSelect(spcs_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(SetpointCommandShortWithCP56Time2a_getTimestamp(spcs_dec)));

    SetpointCommandShortWithCP56Time2a_destroy(spcs_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_InterrogationCommand(void)
{
    InterrogationCommand ic;
    uint8_t qoi = 21;
    ic = InterrogationCommand_create(NULL, 101, qoi);
    TEST_ASSERT_EQUAL_INT(21, InterrogationCommand_getQOI(ic));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) ic);

    InterrogationCommand_destroy(ic);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    InterrogationCommand ic_dec = (InterrogationCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )ic_dec));

    TEST_ASSERT_EQUAL_INT(21, InterrogationCommand_getQOI(ic_dec));

    InterrogationCommand_destroy(ic_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_CounterInterrogationCommand(void)
{
    CounterInterrogationCommand cic;
    uint8_t qcc = 1;
    cic = CounterInterrogationCommand_create(NULL, 101, qcc);

    TEST_ASSERT_EQUAL_INT(1, CounterInterrogationCommand_getQCC(cic));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) cic);

    CounterInterrogationCommand_destroy(cic);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    CounterInterrogationCommand cic_dec = (CounterInterrogationCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )cic_dec));

    TEST_ASSERT_EQUAL_INT(1, CounterInterrogationCommand_getQCC(cic_dec));

    CounterInterrogationCommand_destroy(cic_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_ReadCommand(void)
{
    ReadCommand rc;
    rc = ReadCommand_create( NULL, 101);

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) rc);

    ReadCommand_destroy(rc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(9, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    ReadCommand rc_dec = (ReadCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )rc_dec));

    ReadCommand_destroy(rc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_ClockSynchronizationCommand(void)
{
    ClockSynchronizationCommand csc;
    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);
    csc = ClockSynchronizationCommand_create(NULL, 101, &cpTime1);

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) csc);

    ClockSynchronizationCommand_destroy(csc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(16, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    ClockSynchronizationCommand csc_dec = (ClockSynchronizationCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )csc_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(ClockSynchronizationCommand_getTime(csc_dec)));

    ClockSynchronizationCommand_destroy(csc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_ResetProcessCommand(void)
{
    ResetProcessCommand rpc;
    uint8_t qrp = 0;
    rpc = ResetProcessCommand_create(NULL, 101, qrp);

    TEST_ASSERT_EQUAL_INT(0, ResetProcessCommand_getQRP(rpc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) rpc);

    ResetProcessCommand_destroy(rpc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(10, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    ResetProcessCommand rpc_dec = (ResetProcessCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )rpc_dec));

    TEST_ASSERT_EQUAL_INT(0, ResetProcessCommand_getQRP(rpc_dec));

    ResetProcessCommand_destroy(rpc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_DelayAcquisitionCommand(void)
{
    DelayAcquisitionCommand dac;
    uint64_t time1 = Hal_getTimeInMs();

    struct sCP16Time2a delay;

    bzero(&delay, sizeof(struct sCP16Time2a));

    CP16Time2a_setEplapsedTimeInMs(&delay, 24123);

    dac = DelayAcquisitionCommand_create(NULL, 101, &delay);
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) dac);

    DelayAcquisitionCommand_destroy(dac);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(11, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    DelayAcquisitionCommand dac_dec = (DelayAcquisitionCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )dac_dec));
    CP16Time2a time1_dec = DelayAcquisitionCommand_getDelay(dac_dec);
    TEST_ASSERT_EQUAL_INT(24123, CP16Time2a_getEplapsedTimeInMs(time1_dec));

    DelayAcquisitionCommand_destroy(dac_dec);

    CS101_ASDU_destroy(asdu2);
}
void
test_TestCommand(void)
{
    TestCommand tc;
    tc = TestCommand_create(NULL);

    uint8_t buffer[256];
    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);

    TestCommand_destroy(tc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(11, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    TestCommand tc_dec = (TestCommand) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(0, InformationObject_getObjectAddress((InformationObject )tc_dec));
    TEST_ASSERT_EQUAL_INT(0xaa, buffer[9]);
    TEST_ASSERT_EQUAL_INT(0x55, buffer[10]);
    TEST_ASSERT_TRUE(TestCommand_isValid(tc_dec));

    TestCommand_destroy(tc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_TestCommandWithTime(void)
{
    TestCommandWithCP56Time2a  tc;

    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    tc = TestCommandWithCP56Time2a_create(NULL, 0xaa55, &cpTime1);

    uint8_t buffer[256];
    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);

    TestCommandWithCP56Time2a_destroy(tc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(18, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    TestCommandWithCP56Time2a  tc_dec = (TestCommandWithCP56Time2a ) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(0, InformationObject_getObjectAddress((InformationObject )tc_dec));
    TEST_ASSERT_EQUAL_INT(0xaa55, TestCommandWithCP56Time2a_getCounter(tc_dec));

    TestCommandWithCP56Time2a_destroy(tc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_BitString32(void)
{
    BitString32 bs32;

    bs32 = BitString32_createEx(NULL, 101, 0xaaaa, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, BitString32_getQuality(bs32));

    BitString32_destroy(bs32);

    bs32 = BitString32_create(NULL, 101, 0xaaaa);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32));

    BitString32_destroy(bs32);

    bs32 = BitString32_createEx(NULL, 101, 0xaaaa, IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID + IEC60870_QUALITY_NON_TOPICAL, BitString32_getQuality(bs32));

    TEST_ASSERT_EQUAL_UINT32(0xaaaa, BitString32_getValue(bs32));

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject ) bs32));

    BitString32_destroy(bs32);

    Bitstring32WithCP24Time2a bs32cp24;

    struct sCP24Time2a cp24;

    bs32cp24 = Bitstring32WithCP24Time2a_createEx(NULL, 100002, 0xbbbb, IEC60870_QUALITY_INVALID, &cp24);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, BitString32_getQuality((BitString32 )bs32cp24));

    TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32 )bs32cp24));

    TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject ) bs32cp24));

    Bitstring32WithCP24Time2a_destroy(bs32cp24);

    bs32cp24 = Bitstring32WithCP24Time2a_create(NULL, 100002, 0xbbbb, &cp24);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32 )bs32cp24));

    TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32 )bs32cp24));

    TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject ) bs32cp24));

    Bitstring32WithCP24Time2a_destroy(bs32cp24);

    Bitstring32WithCP56Time2a bs32cp56;

    struct sCP56Time2a cp56;

    bs32cp56 = Bitstring32WithCP56Time2a_createEx(NULL, 1000002, 0xcccc, IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID + IEC60870_QUALITY_NON_TOPICAL, BitString32_getQuality((BitString32 )bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32 )bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject ) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);

    bs32cp56 = Bitstring32WithCP56Time2a_create(NULL, 1000002, 0xcccc, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32 )bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32 )bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject ) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);
}

void
test_Bitstring32CommandWithCP56Time2a(void)
{
    Bitstring32CommandWithCP56Time2a bsc;
    uint64_t time1 = Hal_getTimeInMs();

    struct sCP56Time2a cpTime1;

    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    bsc = Bitstring32CommandWithCP56Time2a_create(NULL, 101, (uint32_t) 0x0000000000, &cpTime1);

    TEST_ASSERT_EQUAL_UINT32(0x0000000000, Bitstring32CommandWithCP56Time2a_getValue(bsc));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) bsc);

    Bitstring32CommandWithCP56Time2a_destroy(bsc);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(20, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    Bitstring32CommandWithCP56Time2a bsc_dec = (Bitstring32CommandWithCP56Time2a) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )bsc_dec));
    TEST_ASSERT_EQUAL_UINT64(time1, CP56Time2a_toMsTimestamp(Bitstring32CommandWithCP56Time2a_getTimestamp(bsc_dec)));
    TEST_ASSERT_EQUAL_UINT32(0x0000000000, Bitstring32CommandWithCP56Time2a_getValue(bsc_dec));

    Bitstring32CommandWithCP56Time2a_destroy(bsc_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_QueryLog(void)
{
    QueryLog queryLog;

    uint64_t stopTime = Hal_getTimeInMs();
    uint64_t startTime = stopTime - 2000;

    struct sCP56Time2a rangeStartTime;
    struct sCP56Time2a rangeStopTime;

    CP56Time2a_createFromMsTimestamp(&rangeStartTime, startTime);
    CP56Time2a_createFromMsTimestamp(&rangeStopTime, stopTime);

    queryLog = QueryLog_create(NULL, 101, 256, &rangeStartTime, &rangeStopTime);

    TEST_ASSERT_EQUAL_UINT16(256, QueryLog_getNOF(queryLog));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) queryLog);

    QueryLog_destroy(queryLog);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(25, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    QueryLog queryLog_dec = (QueryLog) CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_NOT_NULL(queryLog_dec);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject )queryLog_dec));
    TEST_ASSERT_EQUAL_UINT16(256, QueryLog_getNOF(queryLog_dec));
    TEST_ASSERT_EQUAL_UINT64(startTime, CP56Time2a_toMsTimestamp(QueryLog_getRangeStartTime(queryLog_dec)));
    TEST_ASSERT_EQUAL_UINT64(stopTime, CP56Time2a_toMsTimestamp(QueryLog_getRangeStopTime(queryLog_dec)));

    QueryLog_destroy(queryLog_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_FileDirectory(void)
{
    FileDirectory fd1;
    FileDirectory fd2;
    FileDirectory fd3;

    struct sCP56Time2a fileCreationTime;
    uint64_t timeval = Hal_getTimeInMs();
    CP56Time2a_createFromMsTimestamp(&fileCreationTime, timeval);

    fd1 = FileDirectory_create(NULL, 100, 1, 1024, 0, &fileCreationTime);
    fd2 = FileDirectory_create(NULL, 101, 2, 2048, 1, &fileCreationTime);
    fd3 = FileDirectory_create(NULL, 102, 60001, 4444, 2, &fileCreationTime);

    TEST_ASSERT_NOT_NULL(fd1);
    TEST_ASSERT_NOT_NULL(fd2);
    TEST_ASSERT_NOT_NULL(fd3);

    TEST_ASSERT_EQUAL_UINT32(1024, FileDirectory_getLengthOfFile(fd1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    /* NOTE: file directory is always a "sequence" */
    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, true, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) fd1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) fd2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) fd3);

    FileDirectory_destroy(fd1);
    FileDirectory_destroy(fd2);
    FileDirectory_destroy(fd3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(48, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    FileDirectory fd1_dec = (FileDirectory)CS101_ASDU_getElement(asdu2, 0);
    FileDirectory fd2_dec = (FileDirectory)CS101_ASDU_getElement(asdu2, 1);
    FileDirectory fd3_dec = (FileDirectory)CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_NOT_NULL(fd1_dec);
    TEST_ASSERT_NOT_NULL(fd2_dec);
    TEST_ASSERT_NOT_NULL(fd3_dec);

    TEST_ASSERT_EQUAL_UINT32(1024, FileDirectory_getLengthOfFile(fd1_dec));
    TEST_ASSERT_EQUAL_UINT32(2048, FileDirectory_getLengthOfFile(fd2_dec));
    TEST_ASSERT_EQUAL_UINT32(4444, FileDirectory_getLengthOfFile(fd3_dec));
    TEST_ASSERT_EQUAL_INT(100, InformationObject_getObjectAddress((InformationObject)fd1_dec));
    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)fd2_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)fd3_dec));
    TEST_ASSERT_EQUAL_UINT8(0, FileDirectory_getSOF(fd1_dec));
    TEST_ASSERT_EQUAL_UINT8(1, FileDirectory_getSOF(fd2_dec));
    TEST_ASSERT_EQUAL_UINT8(2, FileDirectory_getSOF(fd3_dec));
    TEST_ASSERT_EQUAL_UINT16(1, FileDirectory_getNOF(fd1_dec));
    TEST_ASSERT_EQUAL_UINT16(2, FileDirectory_getNOF(fd2_dec));
    TEST_ASSERT_EQUAL_UINT16(60001, FileDirectory_getNOF(fd3_dec));

    FileDirectory_destroy(fd1_dec);
    FileDirectory_destroy(fd2_dec);
    FileDirectory_destroy(fd3_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_FileDirectorySingleEntry(void)
{
    FileDirectory fd1;

    struct sCP56Time2a fileCreationTime;
    uint64_t timeval = Hal_getTimeInMs();
    CP56Time2a_createFromMsTimestamp(&fileCreationTime, timeval);

    fd1 = FileDirectory_create(NULL, 101, 1, 1024, 0, &fileCreationTime);

    TEST_ASSERT_NOT_NULL(fd1);

    TEST_ASSERT_EQUAL_UINT32(1024, FileDirectory_getLengthOfFile(fd1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    /* NOTE: file directory is always a "sequence" */
    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, true, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) fd1);

    FileDirectory_destroy(fd1);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(22, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(1, CS101_ASDU_getNumberOfElements(asdu2));

    FileDirectory fd1_dec = (FileDirectory)CS101_ASDU_getElement(asdu2, 0);

    TEST_ASSERT_NOT_NULL(fd1_dec);

    TEST_ASSERT_EQUAL_UINT32(1024, FileDirectory_getLengthOfFile(fd1_dec));
    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)fd1_dec));
    TEST_ASSERT_EQUAL_UINT8(0, FileDirectory_getSOF(fd1_dec));
    TEST_ASSERT_EQUAL_UINT16(1, FileDirectory_getNOF(fd1_dec));

    FileDirectory_destroy(fd1_dec);

    CS101_ASDU_destroy(asdu2);
}

void
test_BitString32xx_encodeDecode(void)
{
#ifndef _WIN32
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    BitString32 bs32_1 = BitString32_createEx(NULL, 101, (uint32_t) 0xaaaaaaaaaa, IEC60870_QUALITY_INVALID);
    BitString32 bs32_2 = BitString32_create(NULL, 102, (uint32_t) 0x0000000000);
    BitString32 bs32_3 = BitString32_create(NULL, 103, (uint32_t) 0xffffffffffUL);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_3);

    CS101_ASDU_encode(asdu, f);

    InformationObject_destroy((InformationObject) bs32_1);
    InformationObject_destroy((InformationObject) bs32_2);
    InformationObject_destroy((InformationObject) bs32_3);
    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    BitString32 bs32_1_dec = (BitString32) CS101_ASDU_getElement(asdu2, 0);
    BitString32 bs32_2_dec = (BitString32) CS101_ASDU_getElement(asdu2, 1);
    BitString32 bs32_3_dec = (BitString32) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_UINT32(0xaaaaaaaaaaUL, BitString32_getValue(bs32_1_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_INVALID, BitString32_getQuality(bs32_1_dec));

    TEST_ASSERT_EQUAL_UINT32(0x0000000000UL, BitString32_getValue(bs32_2_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32_2_dec));

    TEST_ASSERT_EQUAL_UINT32(0xffffffffUL, BitString32_getValue(bs32_3_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32_3_dec));

    InformationObject_destroy((InformationObject)bs32_1_dec);
    InformationObject_destroy((InformationObject)bs32_2_dec);
    InformationObject_destroy((InformationObject)bs32_3_dec);

    CS101_ASDU_destroy(asdu2);
#endif
}

void
test_version_number(void)
{
    Lib60870VersionInfo version = Lib60870_getLibraryVersionInfo();

    TEST_ASSERT_EQUAL_INT(2, version.major);
    TEST_ASSERT_EQUAL_INT(3, version.minor);
    TEST_ASSERT_EQUAL_INT(6, version.patch);
}

void
test_CS104_Slave_CreateDestroy(void)
{
	CS104_Slave slave = CS104_Slave_create(100, 100);

	TEST_ASSERT_NOT_NULL(slave);

	CS104_Slave_destroy(slave);
}

void
test_CS104_Connection_CreateDestroy(void)
{
	CS104_Connection con = CS104_Connection_create("127.0.0.1", 2404);

	TEST_ASSERT_NOT_NULL(con);

	CS104_Connection_destroy(con);
}

void
test_CS104_MasterSlave_CreateDestroy(void)
{
	CS104_Slave slave = CS104_Slave_create(100, 100);

	TEST_ASSERT_NOT_NULL(slave);

	CS104_Slave_setLocalPort(slave, 20004);

	CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

	TEST_ASSERT_NOT_NULL(con);

	CS104_Connection_connect(con);

	CS104_Slave_destroy(slave);

	CS104_Connection_destroy(con);
}

void
test_CS104_MasterSlave_CreateDestroyLoop(void)
{
	CS104_Slave slave = NULL;
	CS104_Connection con = NULL;

	for (int i = 0; i < 1000; i++) {
		slave = CS104_Slave_create(100, 100);

		TEST_ASSERT_NOT_NULL(slave);

		CS104_Slave_setLocalPort(slave, 20004);

		CS104_Slave_start(slave);

		con = CS104_Connection_create("127.0.0.1", 20004);

		TEST_ASSERT_NOT_NULL(con);

		bool result = CS104_Connection_connect(con);

		TEST_ASSERT_TRUE(result);

		CS104_Slave_destroy(slave);

		CS104_Connection_destroy(con);
	}
}

void
test_CS104_Connection_ConnectTimeout(void)
{
	CS104_Connection con = CS104_Connection_create("192.168.3.120", 2404);

	TEST_ASSERT_NOT_NULL(con);

	bool result = CS104_Connection_connect(con);

	TEST_ASSERT_FALSE(result);

	CS104_Connection_destroy(con);
}

void
test_CS104_Connection_UseAfterClose(void)
{
    CS104_Slave slave = NULL;
    CS104_Connection con = NULL;

    slave = CS104_Slave_create(100, 100);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);
    CS104_Slave_start(slave);

    con = CS104_Connection_create("127.0.0.1", 20004);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_close(con);

    result = CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);
}

void
test_CS104_Connection_UseAfterServerClosedConnection(void)
{
    CS104_Slave slave = NULL;
    CS104_Connection con = NULL;

    slave = CS104_Slave_create(100, 100);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);
    CS104_Slave_start(slave);

    con = CS104_Connection_create("127.0.0.1", 20004);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Slave_destroy(slave);

    /* wait to allow client side to detect connection loss */
    Thread_sleep(500);

    result = CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

    TEST_ASSERT_FALSE(result);

    CS104_Connection_destroy(con);
}

static CS104_ConnectionEvent test_CS104_Connection_async_timeout_event;

static void
test_CS104_Connection_async_timeout_connectionHandler (void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    test_CS104_Connection_async_timeout_event = event;
}

void
test_CS104_Connection_async_success(void)
{
    test_CS104_Connection_async_timeout_event = CS104_CONNECTION_CLOSED;

    CS104_Slave slave = NULL;
    CS104_Connection con = NULL;

    slave = CS104_Slave_create(100, 100);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);
    CS104_Slave_start(slave);

    con = CS104_Connection_create("127.0.0.1", 20004);

	TEST_ASSERT_NOT_NULL(con);

    CS104_APCIParameters apciParameters = CS104_Connection_getAPCIParameters(con);
    apciParameters->t0 = 1;

    CS104_Connection_setConnectionHandler(con, test_CS104_Connection_async_timeout_connectionHandler, NULL);

	CS104_Connection_connectAsync(con);

    Thread_sleep(500);

    TEST_ASSERT_EQUAL_INT(CS104_CONNECTION_OPENED, test_CS104_Connection_async_timeout_event);

	CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104_Connection_async_timeout(void)
{
    test_CS104_Connection_async_timeout_event = CS104_CONNECTION_CLOSED;

    CS104_Connection con = CS104_Connection_create("192.168.3.120", 2404);

	TEST_ASSERT_NOT_NULL(con);

    CS104_APCIParameters apciParameters = CS104_Connection_getAPCIParameters(con);
    apciParameters->t0 = 1;

    CS104_Connection_setConnectionHandler(con, test_CS104_Connection_async_timeout_connectionHandler, NULL);

	CS104_Connection_connectAsync(con);

    Thread_sleep(2000);

    TEST_ASSERT_EQUAL_INT(CS104_CONNECTION_FAILED, test_CS104_Connection_async_timeout_event);

	CS104_Connection_destroy(con);
}

void
test_CS101_ASDU_addObjectOfWrongType(void)
{
    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    InformationObject io1 = (InformationObject) SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_GOOD);

    bool added = CS101_ASDU_addInformationObject(asdu, io1);

    TEST_ASSERT_TRUE(added);

    InformationObject_destroy(io1);

    InformationObject io2 = (InformationObject) DoublePointInformation_create(NULL, 102, IEC60870_DOUBLE_POINT_OFF, IEC60870_QUALITY_GOOD);

    added = CS101_ASDU_addInformationObject(asdu, io2);

    TEST_ASSERT_FALSE(added);

    InformationObject_destroy(io2);

    CS101_ASDU_destroy(asdu);
}

void
test_CS101_ASDU_addUntilOverflow(void)
{
    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    int i = 0;

    for (i = 0; i < 60; i++) {
        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, 100 + i, true, IEC60870_QUALITY_GOOD);

        bool added = CS101_ASDU_addInformationObject(asdu, io);

        TEST_ASSERT_TRUE(added);

        InformationObject_destroy(io);

        TEST_ASSERT_EQUAL_INT(i + 1, CS101_ASDU_getNumberOfElements(asdu));
    }

    InformationObject io = (InformationObject) SinglePointInformation_create(NULL, 100 + i, true, IEC60870_QUALITY_GOOD);

    bool added = CS101_ASDU_addInformationObject(asdu, io);

    TEST_ASSERT_FALSE(added);

    InformationObject_destroy(io);

    CS101_ASDU_destroy(asdu);
}

void
test_CS101_ASDU_clone(void)
{
    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    int i = 0;

    for (i = 0; i < 60; i++) {
        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, 100 + i, true, IEC60870_QUALITY_GOOD);

        bool added = CS101_ASDU_addInformationObject(asdu, io);

        TEST_ASSERT_TRUE(added);

        InformationObject_destroy(io);

        TEST_ASSERT_EQUAL_INT(i + 1, CS101_ASDU_getNumberOfElements(asdu));
    }

    CS101_ASDU clonedAsdu = CS101_ASDU_clone(asdu, NULL);

    TEST_ASSERT_NOT_NULL(clonedAsdu);
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getCA(asdu), CS101_ASDU_getCA(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getOA(asdu), CS101_ASDU_getOA(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getCOT(asdu), CS101_ASDU_getCOT(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getTypeID(asdu), CS101_ASDU_getTypeID(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getNumberOfElements(asdu), CS101_ASDU_getNumberOfElements(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(CS101_ASDU_getPayloadSize(asdu), CS101_ASDU_getPayloadSize(clonedAsdu));
    TEST_ASSERT_EQUAL_INT(0, memcmp(CS101_ASDU_getPayload(asdu), CS101_ASDU_getPayload(clonedAsdu), CS101_ASDU_getPayloadSize(clonedAsdu)));

    CS101_ASDU_destroy(asdu);
    CS101_ASDU_destroy(clonedAsdu);
}

#if (CONFIG_CS104_SUPPORT_TLS == 1)

struct secEventInfo {
    int eventHandlerCalled;
    int eventCodes[100];
};

static void
securityEventHandler(void* parameter, TLSEventLevel eventLevel, int eventCode, const char* msg, TLSConnection con)
{
    struct secEventInfo* eventInfo = (struct secEventInfo*)parameter;

    char peerAddrBuf[60];
    char* peerAddr = NULL;
    const char* tlsVersion = "unknown";

    if (con) {
        peerAddr = TLSConnection_getPeerAddress(con, peerAddrBuf);
        tlsVersion = TLSConfigVersion_toString(TLSConnection_getTLSVersion(con));
    }

    printf("[SECURITY EVENT] %s (t: %i, c: %i, version: %s remote-ip: %s)\n", msg, eventLevel, eventCode, tlsVersion, peerAddr);

    if (eventInfo) {
        eventInfo->eventCodes[eventInfo->eventHandlerCalled] = eventCode;
        eventInfo->eventHandlerCalled++;
    }
}

void
test_CS104_MasterSlave_TLSConnectSuccess(void)
{
    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    /* use valid certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    res = TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSConnectSuccessWithoutSeparateCACert(void)
{
    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1_chain.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "server_CA1_1_chain.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    /* use expired certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    res = TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1_chain.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSConnectFails(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    /* use valid certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    res = TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);

    TEST_ASSERT_EQUAL_INT(1, eventInfo.eventHandlerCalled);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_ALGO_NOT_SUPPORTED, eventInfo.eventCodes[0]);
}

void
test_CS104_MasterSlave_TLSVersionMismatch(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setMinTlsVersion(tlsConfig1, TLS_VERSION_TLS_1_2);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    TLSConfiguration_setMinTlsVersion(tlsConfig2, TLS_VERSION_TLS_1_1);
    TLSConfiguration_setMaxTlsVersion(tlsConfig2, TLS_VERSION_TLS_1_1);

    /* use valid certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    res = TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);

    TEST_ASSERT_EQUAL_INT(1, eventInfo.eventHandlerCalled);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_UNSECURE_COMMUNICATION, eventInfo.eventCodes[0]);
}

void
test_CS104_MasterSlave_TLSCertificateExpired(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");

    TLSConfiguration_setMinTlsVersion(tlsConfig1, TLS_VERSION_TLS_1_2);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use expired certificate */
    TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");

    TLSConfiguration_setMinTlsVersion(tlsConfig2, TLS_VERSION_TLS_1_2);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);

    TEST_ASSERT_EQUAL_INT(2, eventInfo.eventHandlerCalled);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_CERT_EXPIRED, eventInfo.eventCodes[0]);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_CERT_VALIDATION_FAILED, eventInfo.eventCodes[1]);
}

void
test_CS104_MasterSlave_TLSCertificateRevoked(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);

    TEST_ASSERT_EQUAL_INT(2, eventInfo.eventHandlerCalled);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_CERT_REVOKED, eventInfo.eventCodes[0]);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_ALM_CERT_VALIDATION_FAILED, eventInfo.eventCodes[1]);
}

void
test_CS104_MasterSlave_TLSRenegotiateAfterCRLUpdate(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    // res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    // TEST_ASSERT_TRUE(res);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration_setRenegotiationTime(tlsConfig1, 10000);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    CS101_ASDU newAsdu = CS101_ASDU_create(CS104_Slave_getAppLayerParameters(slave), false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, 1, IEC60870_QUALITY_GOOD);

    CS101_ASDU_addInformationObject(newAsdu, io);

    InformationObject_destroy(io);

    CS104_Slave_enqueueASDU(slave, newAsdu);

    CS101_ASDU_destroy(newAsdu);

    Thread_sleep(1000);

    TEST_ASSERT_EQUAL_INT(1, eventInfo.eventHandlerCalled);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_INF_SESSION_RENEGOTIATION, eventInfo.eventCodes[0]);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSCertificateRevokedBeforeRenegotiation(void)
{
    struct secEventInfo eventInfo;
    memset(&eventInfo, 0, sizeof(struct secEventInfo));

    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, &eventInfo);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration_setRenegotiationTime(tlsConfig1, 1000);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    /* update CRL -> expect renegotiation to fail! */
    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    Thread_sleep(1500);

    CS101_ASDU newAsdu = CS101_ASDU_create(CS104_Slave_getAppLayerParameters(slave), false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, 1, IEC60870_QUALITY_GOOD);

    CS101_ASDU_addInformationObject(newAsdu, io);

    InformationObject_destroy(io);

    CS104_Slave_enqueueASDU(slave, newAsdu);

    CS101_ASDU_destroy(newAsdu);

    Thread_sleep(1500);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);

    TEST_ASSERT_TRUE(eventInfo.eventHandlerCalled > 0);
    TEST_ASSERT_EQUAL_INT(TLS_EVENT_CODE_INF_SESSION_RENEGOTIATION, eventInfo.eventCodes[0]);
}

void
test_CS104_MasterSlave_TLSCertificateRevokedBeforeReconnect(void)
{
    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig1, false);
    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration_setRenegotiationTime(tlsConfig1, 1000);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_close(con);

    /* update CRL -> expect renegotiation to fail! */
    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSUnknownCertificate(void)
{
    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");

    TLSConfiguration_addAllowedCertificateFromFile(tlsConfig1, "client_CA1_3.pem");

    TLSConfiguration_setMinTlsVersion(tlsConfig1, TLS_VERSION_TLS_1_2);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig2, true);;

    /* use expired certificate */
    TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_4.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_4.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");

    TLSConfiguration_setMinTlsVersion(tlsConfig2, TLS_VERSION_TLS_1_2);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSUseSessionResumption(void)
{
    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig1, true);
    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig2, true);
    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    /* use valid certificate */
    TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");

    TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1.pem");

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_destroy(con);

    printf("New connection should use the old TLS session\n");

    con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSCertificateSessionResumptionExpiredAtClient(void)
{
    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_setChainValidation(tlsConfig1, true);


    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration_setRenegotiationTime(tlsConfig1, 1000);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig2, true);
    TLSConfiguration_setSessionResumptionInterval(tlsConfig2, 1);

    TLSConfiguration_setChainValidation(tlsConfig2, true);

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_close(con);

    Thread_sleep(1500);

    /* update CRL -> expect renegotiation to fail! */
    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSCertificateSessionResumptionExpiredAtServer(void)
{
    bool res = false;

    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig1, true);
    TLSConfiguration_setSessionResumptionInterval(tlsConfig1, 1);

    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setEventHandler(tlsConfig1, securityEventHandler, NULL);

    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    TLSConfiguration_setRenegotiationTime(tlsConfig1, 1000);

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig2, true);

    TLSConfiguration_setChainValidation(tlsConfig2, true);

    /* use revoked certificate */
    res = TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TEST_ASSERT_TRUE(res);
    res = TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");
    TEST_ASSERT_TRUE(res);

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_close(con);

    Thread_sleep(2000);

    /* update CRL -> expect renegotiation to fail! */
    res = TLSConfiguration_addCRLFromFile(tlsConfig1, "test.crl");
    TEST_ASSERT_TRUE(res);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_FALSE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

void
test_CS104_MasterSlave_TLSReuseConfigurationWithSessionResumption(void)
{
    TLSConfiguration tlsConfig1 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig1, true);
    TLSConfiguration_setChainValidation(tlsConfig1, true);

    TLSConfiguration_setOwnKeyFromFile(tlsConfig1, "server_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig1, "server_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig1, "root_CA1.pem");

    TLSConfiguration tlsConfig2 = TLSConfiguration_create();

    TLSConfiguration_enableSessionResumption(tlsConfig2, true);
    TLSConfiguration_setChainValidation(tlsConfig2, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig2, true);

    /* use valid certificate */
    TLSConfiguration_setOwnKeyFromFile(tlsConfig2, "client_CA1_3.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig2, "client_CA1_3.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig2, "root_CA1.pem");

    TLSConfiguration_addAllowedCertificateFromFile(tlsConfig2, "server_CA1_1.pem");

    CS104_Slave slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS104_Connection con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    bool result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Connection_destroy(con);

    con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    slave = CS104_Slave_createSecure(100, 100, tlsConfig1);

    TEST_ASSERT_NOT_NULL(slave);

    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    con = CS104_Connection_createSecure("127.0.0.1", 20004, tlsConfig2);

    TEST_ASSERT_NOT_NULL(con);

    result = CS104_Connection_connect(con);

    TEST_ASSERT_TRUE(result);

    Thread_sleep(500);

    CS104_Slave_destroy(slave);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig1);
    TLSConfiguration_destroy(tlsConfig2);
}

#endif /* #if (CONFIG_CS104_SUPPORT_TLS == 1) */

void
test_ASDUsetGetNumberOfElements(void)
{
    struct sCS101_AppLayerParameters salParameters;

    salParameters.maxSizeOfASDU = 100;
    salParameters.originatorAddress = 0;
    salParameters.sizeOfCA = 2;
    salParameters.sizeOfCOT = 2;
    salParameters.sizeOfIOA = 3;
    salParameters.sizeOfTypeId = 1;
    salParameters.sizeOfVSQ = 1;

    CS101_ASDU asdu = CS101_ASDU_create(&salParameters, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    TEST_ASSERT_FALSE(CS101_ASDU_isSequence(asdu));

    CS101_ASDU_setNumberOfElements(asdu, 127);

    TEST_ASSERT_EQUAL_INT(127, CS101_ASDU_getNumberOfElements(asdu));

    CS101_ASDU_setNumberOfElements(asdu, 5);

    TEST_ASSERT_EQUAL_INT(5, CS101_ASDU_getNumberOfElements(asdu));

    TEST_ASSERT_FALSE(CS101_ASDU_isSequence(asdu));

    CS101_ASDU_setSequence(asdu, true);

    CS101_ASDU_setNumberOfElements(asdu, 127);

    TEST_ASSERT_EQUAL_INT(127, CS101_ASDU_getNumberOfElements(asdu));

    CS101_ASDU_setNumberOfElements(asdu, 5);

    TEST_ASSERT_EQUAL_INT(5, CS101_ASDU_getNumberOfElements(asdu));

    TEST_ASSERT_TRUE(CS101_ASDU_isSequence(asdu));

    CS101_ASDU_destroy(asdu);
}

static uint8_t STARTDT_ACT_MSG[] = { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };
static uint8_t STOPDT_ACT_MSG[] = { 0x68, 0x04, 0x13, 0x00, 0x00, 0x00 };

void
test_CS104SlaveUnconfirmedStoppedMode()
{
    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    for (int i = 0; i < 15; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(500);

    CS104_Connection_sendStopDT(con);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(14, info.lastScaledValue);

    info.asduHandlerCalled = 0;
    info.spontCount = 0;

    result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    for (int i = 0; i < 6; i++)
    {
        Thread_sleep(10);

        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    Thread_sleep(500);

    TEST_ASSERT_EQUAL_INT(6, CS104_Connection_sendMessage(con, STOPDT_ACT_MSG, sizeof(STOPDT_ACT_MSG)));

    Thread_sleep(5000);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(6, info.asduHandlerCalled);
    TEST_ASSERT_EQUAL_INT(6, info.spontCount);
    TEST_ASSERT_EQUAL_INT(20, info.lastScaledValue);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

#define SCALED_VALUE_MAX 32767
#define SCALED_VALUE_MIN -32768
#define NORMALIZED_VALUE_MAX (32767.f/32768.f)

void
test_ScaledNormalizedConversion()
{
    TEST_ASSERT_EQUAL_INT(32767, NormalizedValue_toScaled(NORMALIZED_VALUE_MAX));
    TEST_ASSERT_EQUAL_INT(32767, NormalizedValue_toScaled(1.0f));
    TEST_ASSERT_EQUAL_INT(32767, NormalizedValue_toScaled(2.0f));
    TEST_ASSERT_EQUAL_INT(-32768, NormalizedValue_toScaled(-1.0f));
    TEST_ASSERT_EQUAL_INT(-32768, NormalizedValue_toScaled(-2.0f));
    TEST_ASSERT_EQUAL_INT(0, NormalizedValue_toScaled(0.0f));
    TEST_ASSERT_EQUAL_INT(0, NormalizedValue_toScaled(-0.0f));

    float normalizedUnit = (1.f - NORMALIZED_VALUE_MAX);

    TEST_ASSERT_EQUAL_FLOAT(NORMALIZED_VALUE_MAX - normalizedUnit, NormalizedValue_fromScaled(32766));
    TEST_ASSERT_EQUAL_FLOAT(NORMALIZED_VALUE_MAX, NormalizedValue_fromScaled(32767));
    TEST_ASSERT_EQUAL_FLOAT(NORMALIZED_VALUE_MAX, NormalizedValue_fromScaled(32768));
    TEST_ASSERT_EQUAL_FLOAT(NORMALIZED_VALUE_MAX, NormalizedValue_fromScaled(99999));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f + (normalizedUnit * 2.f), NormalizedValue_fromScaled(-32766));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f + normalizedUnit, NormalizedValue_fromScaled(-32767));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, NormalizedValue_fromScaled(-32768));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, NormalizedValue_fromScaled(-32769));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, NormalizedValue_fromScaled(0));
}

void
test_CS104Connection_cannotWriteToSocketWhenNotConnected()
{
    /* test that the client does not crash when it tries to send a message after the connection is closed */

    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    CS104_Slave_stop(slave);

    Thread_sleep(500);

    CS104_Connection_sendStopDT(con);

    CS104_Connection_close(con);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

static bool
test_CS104Slave_handleTestCommand_asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    CS101_ASDU* receivedASDU = (CS101_ASDU*)parameter;

    if (*receivedASDU != NULL)
    {
        CS101_ASDU_destroy(*receivedASDU);
    }
    
    *receivedASDU = CS101_ASDU_clone(asdu, NULL);

    return true;
}

void
test_CS104Slave_handleTestCommandWithTimestamp()
{
    //TODO install asduHandler to intercept
    CS101_ASDU receivedASDU = NULL;

    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104Slave_handleTestCommand_asduReceivedHandler, &receivedASDU);

    CS104_Connection_sendStartDT(con);

    TestCommandWithCP56Time2a  tc;

    uint64_t time1 = Hal_getTimeInMs();
    struct sCP56Time2a cpTime1;
    CP56Time2a_createFromMsTimestamp(&cpTime1, time1);

    /* send correct test command */
    tc = TestCommandWithCP56Time2a_create(NULL, 0xaa55, &cpTime1);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);
    TestCommandWithCP56Time2a_destroy(tc);
    TEST_ASSERT_TRUE(CS104_Connection_sendASDU(con, asdu));
    CS101_ASDU_destroy(asdu);

    Thread_sleep(500);

    TEST_ASSERT_NOT_NULL(receivedASDU);
    TEST_ASSERT_EQUAL_INT(C_TS_TA_1, CS101_ASDU_getTypeID(receivedASDU));
    TEST_ASSERT_EQUAL_INT(CS101_COT_ACTIVATION_CON, CS101_ASDU_getCOT(receivedASDU));
    TEST_ASSERT_FALSE(CS101_ASDU_isNegative(receivedASDU));

    /* send test command with wrong COT */
    tc = TestCommandWithCP56Time2a_create(NULL, 0xaa55, &cpTime1);
    asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION_CON, 0, 1, false, false);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);
    TestCommandWithCP56Time2a_destroy(tc);
    TEST_ASSERT_TRUE(CS104_Connection_sendASDU(con, asdu));
    CS101_ASDU_destroy(asdu);

    Thread_sleep(500);

    TEST_ASSERT_NOT_NULL(receivedASDU);
    TEST_ASSERT_EQUAL_INT(C_TS_TA_1, CS101_ASDU_getTypeID(receivedASDU));
    TEST_ASSERT_EQUAL_INT(CS101_COT_UNKNOWN_COT, CS101_ASDU_getCOT(receivedASDU));
    TEST_ASSERT_TRUE(CS101_ASDU_isNegative(receivedASDU));

    /* send test command with correct COT but IOA != 0 */
    tc = TestCommandWithCP56Time2a_create(NULL, 0xaa55, &cpTime1);
    InformationObject_setObjectAddress((InformationObject)tc, 2);
    asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION_CON, 0, 1, false, false);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);
    TestCommandWithCP56Time2a_destroy(tc);
    TEST_ASSERT_TRUE(CS104_Connection_sendASDU(con, asdu));
    CS101_ASDU_destroy(asdu);

    Thread_sleep(500);

    TEST_ASSERT_NOT_NULL(receivedASDU);
    TEST_ASSERT_EQUAL_INT(C_TS_TA_1, CS101_ASDU_getTypeID(receivedASDU));
    TEST_ASSERT_EQUAL_INT(CS101_COT_UNKNOWN_IOA, CS101_ASDU_getCOT(receivedASDU));
    TEST_ASSERT_TRUE(CS101_ASDU_isNegative(receivedASDU));

    /* send test command with wrong COT AND IOA != 0 */
    tc = TestCommandWithCP56Time2a_create(NULL, 0xaa55, &cpTime1);
    InformationObject_setObjectAddress((InformationObject)tc, 2);
    asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_ACTIVATION_TERMINATION, 0, 1, false, false);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) tc);
    TestCommandWithCP56Time2a_destroy(tc);
    TEST_ASSERT_TRUE(CS104_Connection_sendASDU(con, asdu));
    CS101_ASDU_destroy(asdu);

    Thread_sleep(500);

    TEST_ASSERT_NOT_NULL(receivedASDU);
    TEST_ASSERT_EQUAL_INT(C_TS_TA_1, CS101_ASDU_getTypeID(receivedASDU));
    TEST_ASSERT_EQUAL_INT(CS101_COT_UNKNOWN_IOA, CS101_ASDU_getCOT(receivedASDU));
    TEST_ASSERT_TRUE(CS101_ASDU_isNegative(receivedASDU));

    CS104_Connection_close(con);

    CS104_Connection_destroy(con);

    if (receivedASDU)
        CS101_ASDU_destroy(receivedASDU);

    CS104_Slave_destroy(slave);
}

int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_version_number);
    RUN_TEST(test_CS104_Slave_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroyLoop);
    RUN_TEST(test_CS104_Connection_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroy);
    RUN_TEST(test_CP56Time2a);
    RUN_TEST(test_CP56Time2aToMsTimestamp);
    RUN_TEST(test_CP56Time2aConversionFunctions);
    RUN_TEST(test_StepPositionInformation);
    RUN_TEST(test_addMaxNumberOfIOsToASDU);
    RUN_TEST(test_SingleEventType);

    RUN_TEST(test_SinglePointInformation);
    RUN_TEST(test_SinglePointInformationSequence);
    RUN_TEST(test_SinglePointWithCP24Time2a);
    RUN_TEST(test_SinglePointWithCP56Time2a);

    RUN_TEST(test_DoublePointInformation);
    RUN_TEST(test_DoublePointWithCP24Time2a);
    RUN_TEST(test_DoublePointWithCP56Time2a);

    RUN_TEST(test_NormalizeMeasureValueWithoutQuality);
    RUN_TEST(test_NormalizeMeasureValue);
    RUN_TEST(test_MeasuredValueNormalizedWithCP24Time2a);
    RUN_TEST(test_MeasuredValueNormalizedWithCP56Time2a);

    RUN_TEST(test_MeasuredValueScaled);
    RUN_TEST(test_MeasuredValueScaledWithCP24Time2a);
    RUN_TEST(test_MeasuredValueScaledWithCP56Time2a);

    RUN_TEST(test_MeasuredValueShort);
    RUN_TEST(test_MeasuredValueShortWithCP24Time2a);
    RUN_TEST(test_MeasuredValueShortWithCP56Time2a);

    RUN_TEST(test_StepPositionInformation);
    RUN_TEST(test_StepPositionWithCP24Time2a);
    RUN_TEST(test_StepPositionWithCP56Time2a);

    RUN_TEST(test_IntegratedTotals);
    RUN_TEST(test_IntegratedTotalsWithCP24Time2a);
    RUN_TEST(test_IntegratedTotalsWithCP56Time2a);

    RUN_TEST(test_SingleCommand);
    RUN_TEST(test_SingleCommandWithCP56Time2a);

    RUN_TEST(test_DoubleCommand);
    RUN_TEST(test_DoubleCommandWithCP56Time2a);

    RUN_TEST(test_StepCommandValue);
    RUN_TEST(test_StepCommandWithCP56Time2a);

    RUN_TEST(test_SetpointCommandNormalized);
    RUN_TEST(test_SetpointCommandNormalizedWithCP56Time2a);

    RUN_TEST(test_SetpointCommandScaled);
    RUN_TEST(test_SetpointCommandScaledWithCP56Time2a);

    RUN_TEST(test_SetpointCommandShort);
    RUN_TEST(test_SetpointCommandShortWithCP56Time2a);

    RUN_TEST(test_InterrogationCommand);
    RUN_TEST(test_CounterInterrogationCommand);

    RUN_TEST(test_ReadCommand);
    RUN_TEST(test_ClockSynchronizationCommand);
    RUN_TEST(test_ResetProcessCommand);
    RUN_TEST(test_DelayAcquisitionCommand);
    RUN_TEST(test_TestCommand);
    RUN_TEST(test_TestCommandWithTime);

    RUN_TEST(test_BitString32);
    RUN_TEST(test_Bitstring32CommandWithCP56Time2a);

    RUN_TEST(test_QueryLog);

    RUN_TEST(test_FileDirectory);
    RUN_TEST(test_FileDirectorySingleEntry);

    RUN_TEST(test_BitString32xx_encodeDecode);
    RUN_TEST(test_EventOfProtectionEquipmentWithTime);
    RUN_TEST(test_IpAddressHandling);

    RUN_TEST(test_CS104SlaveConnectionIsRedundancyGroup);
    RUN_TEST(test_CS104SlaveSingleRedundancyGroup);
    RUN_TEST(test_CS104SlaveSingleRedundancyGroupMultipleConnections);

    RUN_TEST(test_CS104SlaveEventQueue1);
    RUN_TEST(test_CS104SlaveEventQueueOverflow);
    RUN_TEST(test_CS104SlaveEventQueueOverflow2);
    RUN_TEST(test_CS104SlaveEventQueueCheckCapacity);
    RUN_TEST(test_CS104SlaveEventQueueOverflow3);

    RUN_TEST(test_CS104_Connection_ConnectTimeout);

    RUN_TEST(test_CS104_Connection_UseAfterClose);
    RUN_TEST(test_CS104_Connection_UseAfterServerClosedConnection);

    RUN_TEST(test_CS104_Connection_async_success);
    RUN_TEST(test_CS104_Connection_async_timeout);

    RUN_TEST(test_CS101_ASDU_addObjectOfWrongType);
    RUN_TEST(test_CS101_ASDU_addUntilOverflow);

#if (CONFIG_CS104_SUPPORT_TLS == 1)
    RUN_TEST(test_CS104_MasterSlave_TLSConnectSuccess);
    RUN_TEST(test_CS104_MasterSlave_TLSConnectSuccessWithoutSeparateCACert);
    RUN_TEST(test_CS104_MasterSlave_TLSConnectFails);
    RUN_TEST(test_CS104_MasterSlave_TLSVersionMismatch);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateExpired);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateRevoked);
    RUN_TEST(test_CS104_MasterSlave_TLSRenegotiateAfterCRLUpdate);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateRevokedBeforeRenegotiation);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateRevokedBeforeReconnect);
    RUN_TEST(test_CS104_MasterSlave_TLSUnknownCertificate);
    RUN_TEST(test_CS104_MasterSlave_TLSUseSessionResumption);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateSessionResumptionExpiredAtClient);
    RUN_TEST(test_CS104_MasterSlave_TLSCertificateSessionResumptionExpiredAtServer);
    RUN_TEST(test_CS104_MasterSlave_TLSReuseConfigurationWithSessionResumption);
#endif /* #if (CONFIG_CS104_SUPPORT_TLS == 1) */

    RUN_TEST(test_ASDUsetGetNumberOfElements);
    RUN_TEST(test_CS101_ASDU_clone);

    RUN_TEST(test_CS104SlaveUnconfirmedStoppedMode);

    RUN_TEST(test_ScaledNormalizedConversion);

    RUN_TEST(test_CS104Connection_cannotWriteToSocketWhenNotConnected);

    RUN_TEST(test_CS104Slave_handleTestCommandWithTimestamp);

    return UNITY_END();
}
