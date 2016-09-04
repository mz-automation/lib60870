#include "unity.h"
#include "iec60870_common.h"
#include "hal_time.h"



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


int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_CP56Time2a);
    RUN_TEST(test_StepPositionInformation);
    return UNITY_END();
}
