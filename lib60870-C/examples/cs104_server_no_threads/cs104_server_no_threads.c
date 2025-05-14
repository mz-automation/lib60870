#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "cs104_slave.h"

#include "hal_thread.h"
#include "hal_time.h"

static bool running = true;

void
sigint_handler(int signalId)
{
    running = false;
}

static sCS101_StaticASDU _asdu;
static uint8_t ioBuf[250];

void
printCP56Time2a(CP56Time2a time)
{
    printf("%02i:%02i:%02i %02i/%02i/%04i", CP56Time2a_getHour(time),
                             CP56Time2a_getMinute(time),
                             CP56Time2a_getSecond(time),
                             CP56Time2a_getDayOfMonth(time),
                             CP56Time2a_getMonth(time),
                             CP56Time2a_getYear(time) + 2000);
}

/* Callback handler to log sent or received messages (optional) */
static void
rawMessageHandler(void* parameter, IMasterConnection conneciton, uint8_t* msg, int msgSize, bool sent)
{
    if (sent)
        printf("SEND: ");
    else
        printf("RCVD: ");

    int i;
    for (i = 0; i < msgSize; i++) {
        printf("%02x ", msg[i]);
    }

    printf("\n");
}

static bool
clockSyncHandler (void* parameter, IMasterConnection connection, CS101_ASDU asdu, CP56Time2a newTime)
{
    printf("Process time sync command with time "); printCP56Time2a(newTime); printf("\n");

    return true;
}


static bool
interrogationHandler(void* parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi)
{
    printf("Received interrogation for group %i\n", qoi);

    if (qoi == 20) { /* only handle station interrogation */

        CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

        IMasterConnection_sendACT_CON(connection, asdu, false);

        /* The CS101 specification only allows information objects without timestamp in GI responses */

        CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_INTERROGATED_BY_STATION,
                0, 1, false, false);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create((MeasuredValueScaled) &ioBuf, 100, -1, IEC60870_QUALITY_GOOD));

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create((MeasuredValueScaled) &ioBuf, 101, 23, IEC60870_QUALITY_GOOD));

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create((MeasuredValueScaled) &ioBuf, 102, 2300, IEC60870_QUALITY_GOOD));

        IMasterConnection_sendASDU(connection, newAsdu);


        newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_INTERROGATED_BY_STATION,
                    0, 1, false, false);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            SinglePointInformation_create((SinglePointInformation) &ioBuf, 104, true, IEC60870_QUALITY_GOOD));

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            SinglePointInformation_create((SinglePointInformation) &ioBuf, 105, false, IEC60870_QUALITY_GOOD));

        IMasterConnection_sendASDU(connection, newAsdu);;

        newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, true, CS101_COT_INTERROGATED_BY_STATION,
                0, 1, false, false);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 300, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 301, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 302, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 303, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 304, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 305, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 306, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 307, false, IEC60870_QUALITY_GOOD));

        IMasterConnection_sendASDU(connection, newAsdu);

        for (int i = 0; i < 30; i++) {
            newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, true, CS101_COT_INTERROGATED_BY_STATION,
                    0, 1, false, false);

            CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) &ioBuf, 400 +
                     i, true, IEC60870_QUALITY_GOOD));

            IMasterConnection_sendASDU(connection, newAsdu);
        }

        IMasterConnection_sendACT_TERM(connection, asdu);
    }
    else {
        IMasterConnection_sendACT_CON(connection, asdu, true);
    }

    return true;
}

static bool
asduHandler(void* parameter, IMasterConnection connection, CS101_ASDU asdu)
{
    if (CS101_ASDU_getTypeID(asdu) == C_SC_NA_1)
    {
        printf("received single command\n");

        if  (CS101_ASDU_getCOT(asdu) == CS101_COT_ACTIVATION)
        {
            InformationObject io = CS101_ASDU_getElement(asdu, 0);

            if (io)
            {
                if (InformationObject_getObjectAddress(io) == 5000)
                {
                    SingleCommand sc = (SingleCommand) io;

                    printf("IOA: %i switch to %i\n", InformationObject_getObjectAddress(io),
                            SingleCommand_getState(sc));

                    CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
                }
                else
                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);

                InformationObject_destroy(io);
            }
            else
            {
                printf("ERROR: message has no valid information object\n");
                return true;
            }
        }
        else
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);

        IMasterConnection_sendASDU(connection, asdu);

        return true;
    }

    return false;
}

static bool
connectionRequestHandler(void* parameter, const char* ipAddress)
{
    printf("New connection request from %s\n", ipAddress);

#if 0
    if (strcmp(ipAddress, "127.0.0.1") == 0) {
        printf("Accept connection\n");
        return true;
    }
    else {
        printf("Deny connection\n");
        return false;
    }
#else
    return true;
#endif
}

static void
connectionEventHandler(void* parameter, IMasterConnection con, CS104_PeerConnectionEvent event)
{
    if (event == CS104_CON_EVENT_CONNECTION_OPENED) {
        printf("Connection opened (%p)\n", con);
    }
    else if (event == CS104_CON_EVENT_CONNECTION_CLOSED) {
        printf("Connection closed (%p)\n", con);
    }
    else if (event == CS104_CON_EVENT_ACTIVATED) {
        printf("Connection activated (%p)\n", con);
    }
    else if (event == CS104_CON_EVENT_DEACTIVATED) {
        printf("Connection deactivated (%p)\n", con);
    }
}

int
main(int argc, char** argv)
{
    /* Add Ctrl-C handler */
    signal(SIGINT, sigint_handler);

    /* create a new slave/server instance with default connection parameters and
     * default message queue size (will provide space for 100 messages of the maximum
     * message size or more messages for smaller messages */
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setLocalAddress(slave, "0.0.0.0");

    /* Set mode to a single redundancy group
     * NOTE: library has to be compiled with CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP enabled (=1)
     */
    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);

    /* get the connection parameters - we need them to create correct ASDUs */
    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    /* set the callback handler for the clock synchronization command */
    CS104_Slave_setClockSyncHandler(slave, clockSyncHandler, NULL);

    /* set the callback handler for the interrogation command */
    CS104_Slave_setInterrogationHandler(slave, interrogationHandler, NULL);

    /* set handler for other message types */
    CS104_Slave_setASDUHandler(slave, asduHandler, NULL);

    /* set handler to handle connection requests (optional) */
    CS104_Slave_setConnectionRequestHandler(slave, connectionRequestHandler, NULL);

    /* set handler to track connection events (optional) */
    CS104_Slave_setConnectionEventHandler(slave, connectionEventHandler, NULL);

    /* uncomment to log messages */
    //CS104_Slave_setRawMessageHandler(slave, rawMessageHandler, NULL);

    CS104_Slave_startThreadless(slave);

    if (CS104_Slave_isRunning(slave) == false) {
        printf("Starting server failed!\n");
        goto exit_program;
    }

    int16_t scaledValue = 0;

    uint64_t nextSendTime = Hal_getTimeInMs() + 1000;

    while (running) {

        CS104_Slave_tick(slave);

        if (Hal_getTimeInMs() >= nextSendTime) {

            nextSendTime += 1000;

            CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);

            scaledValue++;

            CS101_ASDU_addInformationObject(newAsdu,
                    (InformationObject) MeasuredValueScaled_create((MeasuredValueScaled)&ioBuf, 110, scaledValue, IEC60870_QUALITY_GOOD));

            /* Add ASDU to slave event queue */
            CS104_Slave_enqueueASDU(slave, newAsdu);
        }

        Thread_sleep(1);
    }

    CS104_Slave_stopThreadless(slave);

exit_program:
    CS104_Slave_destroy(slave);
}
