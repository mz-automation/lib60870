#include "unity.h"
#include "iec60870_common.h"
#include "cs104_slave.h"
#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "buffer_frame.h"
#include <string.h>
#include <stdlib.h>

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
        char* doubleSep = strstr(ipAddrStr, "::");

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

void
CS101_ASDU_encode(CS101_ASDU self, Frame frame);

CS101_ASDU
CS101_ASDU_createFromBuffer(CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength);

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

        Thread_sleep(10);

        CS104_Connection_close(con);
    }

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Connection_destroy(con);

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

    if (CS101_ASDU_getCOT(asdu) == CS101_COT_SPONTANEOUS) {
        info->spontCount++;


        if (CS101_ASDU_getTypeID(asdu) == M_ME_NB_1) {
            static uint8_t ioBuf[250];

            MeasuredValueScaled mv = (MeasuredValueScaled) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

            info->lastScaledValue = MeasuredValueScaled_getValue(mv);
        }
    }

    return true;
}

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

    int i;

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

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(14, info.lastScaledValue);

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

    TEST_ASSERT_EQUAL_INT(30, info.asduHandlerCalled);
    TEST_ASSERT_EQUAL_INT(30, info.spontCount);
    TEST_ASSERT_EQUAL_INT(29, info.lastScaledValue);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

struct sTestMessageQueueEntryInfo {
    uint64_t entryTimestamp;
    unsigned int entryState:2;
    unsigned int size:8;
};

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

    int i;

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
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 10) / entrySize - 1;

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

    int i;

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
    int msgQueueCapacity = ((sizeof(struct sTestMessageQueueEntryInfo) + 256) * 10) / entrySize - 1;

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

    //TEST_ASSERT_EQUAL_INT(msgQueueCapacity + 150, info.asduHandlerCalled);
    //TEST_ASSERT_EQUAL_INT(msgQueueCapacity + 150, info.spontCount);
    //TEST_ASSERT_EQUAL_INT(449, info.lastScaledValue);

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
test_SinglePointInformation(void)
{
    SinglePointInformation spi1;
    SinglePointInformation spi2;
    SinglePointInformation spi3;

    spi1 = SinglePointInformation_create(NULL, 101, true, IEC60870_QUALITY_INVALID);
    spi2 = SinglePointInformation_create(NULL, 102, false, IEC60870_QUALITY_BLOCKED);
    spi3 = SinglePointInformation_create(NULL, 103, true, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2));
    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1));

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) spi3);

    SinglePointInformation_destroy(spi1);
    SinglePointInformation_destroy(spi2);
    SinglePointInformation_destroy(spi3);

    CS101_ASDU_encode(asdu, f);

    TEST_ASSERT_EQUAL_INT(18, Frame_getMsgSize(f));

    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    TEST_ASSERT_EQUAL_INT(3, CS101_ASDU_getNumberOfElements(asdu2));

    SinglePointInformation spi1_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 0);
    SinglePointInformation spi2_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 1);
    SinglePointInformation spi3_dec = (SinglePointInformation) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject)spi1_dec));
    TEST_ASSERT_EQUAL_INT(102, InformationObject_getObjectAddress((InformationObject)spi2_dec));
    TEST_ASSERT_EQUAL_INT(103, InformationObject_getObjectAddress((InformationObject)spi3_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, SinglePointInformation_getQuality(spi1_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi1_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_BLOCKED, SinglePointInformation_getQuality(spi2_dec));
    TEST_ASSERT_FALSE(SinglePointInformation_getValue(spi2_dec));

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, SinglePointInformation_getQuality(spi3_dec));
    TEST_ASSERT_TRUE(SinglePointInformation_getValue(spi3_dec));

    SinglePointInformation_destroy(spi1_dec);
    SinglePointInformation_destroy(spi2_dec);
    SinglePointInformation_destroy(spi3_dec);

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

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject) bs32));

    BitString32_destroy(bs32);

    Bitstring32WithCP24Time2a bs32cp24;

    struct sCP24Time2a cp24;

    bs32cp24 = Bitstring32WithCP24Time2a_createEx(NULL, 100002, 0xbbbb, IEC60870_QUALITY_INVALID, &cp24);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, BitString32_getQuality((BitString32)bs32cp24));

    TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32)bs32cp24));

    TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject) bs32cp24));

    Bitstring32WithCP24Time2a_destroy(bs32cp24);

    bs32cp24 = Bitstring32WithCP24Time2a_create(NULL, 100002, 0xbbbb, &cp24);

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32)bs32cp24));

	TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32)bs32cp24));

	TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject) bs32cp24));

	Bitstring32WithCP24Time2a_destroy(bs32cp24);

    Bitstring32WithCP56Time2a bs32cp56;

    struct sCP56Time2a cp56;

    bs32cp56 = Bitstring32WithCP56Time2a_createEx(NULL, 1000002, 0xcccc, IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID + IEC60870_QUALITY_NON_TOPICAL, BitString32_getQuality((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);

    bs32cp56 = Bitstring32WithCP56Time2a_create(NULL, 1000002, 0xcccc, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);
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

		con = CS104_Connection_create("127.0.0.1", 20004);

		TEST_ASSERT_NOT_NULL(con);

		CS104_Connection_connect(con);

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

int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_CS104_Slave_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroyLoop);
    RUN_TEST(test_CS104_Connection_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroy);
    RUN_TEST(test_CP56Time2a);
    RUN_TEST(test_CP56Time2aToMsTimestamp);
    RUN_TEST(test_StepPositionInformation);
    RUN_TEST(test_addMaxNumberOfIOsToASDU);
    RUN_TEST(test_SingleEventType);
    RUN_TEST(test_SinglePointInformation);
    RUN_TEST(test_SinglePointWithCP24Time2a);
    RUN_TEST(test_SinglePointWithCP56Time2a);
    RUN_TEST(test_BitString32);
    RUN_TEST(test_BitString32xx_encodeDecode);
    RUN_TEST(test_EventOfProtectionEquipmentWithTime);
    RUN_TEST(test_IpAddressHandling);

    RUN_TEST(test_CS104SlaveConnectionIsRedundancyGroup);
    RUN_TEST(test_CS104SlaveSingleRedundancyGroup);

    RUN_TEST(test_CS104SlaveEventQueue1);
    RUN_TEST(test_CS104SlaveEventQueueOverflow);
    RUN_TEST(test_CS104SlaveEventQueueOverflow2);

    RUN_TEST(test_CS104_Connection_ConnectTimeout);

    return UNITY_END();
}
