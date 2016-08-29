#include "unity.h"
#include "iec60870_common.h"
#include "hal_time.h"



void test_CP56Time2a(void)
{
    struct sCP56Time2a currentTime;

    uint64_t currentTimestamp = Hal_getTimeInMs();

    CP56Time2a_createFromMsTimestamp(&currentTime, currentTimestamp);

    uint64_t convertedTimestamp = CP56Time2a_toMsTimestamp(&currentTime);

    TEST_ASSERT_EQUAL_UINT64(currentTimestamp, convertedTimestamp);
}


int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_CP56Time2a);
    return UNITY_END();
}
