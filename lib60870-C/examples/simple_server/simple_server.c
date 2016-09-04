#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "iec60870_slave.h"

#include "hal_thread.h"
#include "hal_time.h"

static bool running = true;

static ConnectionParameters connectionParameters;

void
printCP56Time2a(CP56Time2a time)
{
    printf("%02i:%02i:%02i %02i/%02i/%04i", CP56Time2a_getHour(time),
                             CP56Time2a_getMinute(time),
                             CP56Time2a_getSecond(time),
                             CP56Time2a_getDayOfMonth(time),
                             CP56Time2a_getMonth(time) + 1,
                             CP56Time2a_getYear(time) + 2000);
}

static bool
clockSyncHandler (void* parameter, MasterConnection connection, ASDU asdu, CP56Time2a newTime)
{
    printf("Process time sync command with time "); printCP56Time2a(newTime); printf("\n");

    return true;
}

static bool
interrogationHandler(void* parameter, MasterConnection connection, ASDU asdu, uint8_t qoi)
{
    printf("Received interrogation for group %i\n", qoi);

    struct sCP56Time2a timestamp;

    CP56Time2a_createFromMsTimestamp(&timestamp, Hal_getTimeInMs());

    MasterConnection_sendACT_CON(connection, asdu, false);

    ASDU newAsdu = ASDU_create(connectionParameters, M_ME_NB_1, INTERROGATED_BY_STATION,
            0, 1, false, false);

    InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 100, -1, IEC60870_QUALITY_GOOD);

    ASDU_addInformationObject(newAsdu, io);

    ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create(io, 101, 23, IEC60870_QUALITY_GOOD));

    ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create(io, 102, 2300, IEC60870_QUALITY_GOOD));

    MeasuredValueScaled_destroy(io);

    MasterConnection_sendASDU(connection, newAsdu);

    newAsdu = ASDU_create(connectionParameters, M_SP_TB_1, INTERROGATED_BY_STATION,
                0, 1, false, false);

    io = (InformationObject) SinglePointWithCP56Time2a_create(NULL, 104, true, IEC60870_QUALITY_GOOD, &timestamp);

    ASDU_addInformationObject(newAsdu, io);

    ASDU_addInformationObject(newAsdu, (InformationObject)
                SinglePointWithCP56Time2a_create(io, 105, false, IEC60870_QUALITY_GOOD, &timestamp));

    SinglePointWithCP56Time2a_destroy(io);

    MasterConnection_sendASDU(connection, newAsdu);


    newAsdu = ASDU_create(connectionParameters, M_IT_TB_1, INTERROGATED_BY_STATION,
                0, 1, false, false);

    BinaryCounterReading bcr = BinaryCounterReading_create(NULL, 12345678, 0, false, false, true);

    io = (InformationObject) IntegratedTotalsWithCP56Time2a_create(NULL, 200, bcr, &timestamp);

    ASDU_addInformationObject(newAsdu, io);

    BinaryCounterReading_destroy(bcr);

    InformationObject_destroy(io);

    MasterConnection_sendASDU(connection, newAsdu);

    MasterConnection_sendACT_TERM(connection, asdu);

    return true;
}

int
main(int argc, char** argv)
{
    /* create a new slave/server instance with default connection parameters and
     * default message queue size */
    Slave slave = T104Slave_create(NULL, 0);

    /* get the connection parameters - we need them to create correct ASDUs */
    connectionParameters = Slave_getConnectionParameters(slave);

    Slave_start(slave);

    /* set the callback handler for the clock synchronization command */
    Slave_setClockSyncHandler(slave, clockSyncHandler, NULL);

    /* set the callback handler for the interrogation command */
    Slave_setInterrogationHandler(slave, interrogationHandler, NULL);

    int16_t scaledValue = 0;

    while (running) {
        Thread_sleep(1000);

        ASDU newAsdu = ASDU_create(connectionParameters, M_ME_NB_1, PERIODIC, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        Slave_enqueueASDU(slave, newAsdu);
    }

    Slave_stop(slave);

}
