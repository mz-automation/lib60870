#include <stdlib.h>
#include <stdbool.h>

#include "iec60870_slave.h"

#include "hal_thread.h"

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

    MasterConnection_sendACT_CON(connection, asdu, false);

    ASDU newAsdu = ASDU_create(connectionParameters, M_ME_NB_1, INTERROGATED_BY_STATION,
            0, 1, false, false);

    InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 100, -1, IEC60870_QUALITY_GOOD);

    ASDU_addInformationObject(newAsdu, io);

    ASDU_addInformationObject(newAsdu, MeasuredValueScaled_create(io, 101, 23, IEC60870_QUALITY_GOOD));
    ASDU_addInformationObject(newAsdu, MeasuredValueScaled_create(io, 102, 2300, IEC60870_QUALITY_GOOD));

    MasterConnection_sendASDU(connection, newAsdu);

    MasterConnection_sendACT_TERM(connection, asdu);

    return true;
}

int
main(int argc, char** argv)
{
    /* create a new master instance with default connection parameters */
    Master master = T104Master_create(NULL);

    /* get the connection parameters - we need them to create correct ASDUs */
    connectionParameters = Master_getConnectionParameters(master);

    Master_start(master);

    /* set the callback handler for the clock synchronization command */
    Master_setClockSyncHandler(master, clockSyncHandler, NULL);

    /* set the callback handler for the interrogation command */
    Master_setInterrogationHandler(master, interrogationHandler, NULL);

    while (running) {
        Thread_sleep(10000);
    }

    Master_stop(master);

}
