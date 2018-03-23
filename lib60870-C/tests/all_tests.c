#include "unity.h"
#include "iec60870_common.h"
#include "hal_time.h"

void setUp(void) { }
void tearDown(void) {}


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
test_StepPositionInformation(void)
{
    StepPositionInformation spi;

    spi = StepPositionInformation_create(NULL, 101, 0, true, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue(spi));
    TEST_ASSERT_TRUE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, 63, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, -64, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(-64, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, 64, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, -65, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(-64, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_destroy(spi);
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


int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_CP56Time2a);
    RUN_TEST(test_CP56Time2aToMsTimestamp);
    RUN_TEST(test_StepPositionInformation);
    RUN_TEST(test_addMaxNumberOfIOsToASDU);
    return UNITY_END();
}
