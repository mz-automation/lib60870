#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"

#include <stdio.h>
#include <stdlib.h>

/* Callback handler to log sent or received messages (optional) */
static void
rawMessageHandler (void* parameter, uint8_t* msg, int msgSize, bool sent)
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

/* Connection event handler */
static void
connectionHandler (void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    switch (event) {
    case CS104_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case CS104_CONNECTION_CLOSED:
        printf("Connection closed\n");
        break;
    case CS104_CONNECTION_FAILED:
        printf("Failed to connect\n");
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
        break;
    }
}

/*
 * CS101_ASDUReceivedHandler implementation
 *
 * For CS104 the address parameter has to be ignored
 */
static bool
asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1) {

        printf("  measured scaled values with CP56Time2a timestamp:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu, i);

            if (io)
            {
                printf("    IOA: %i value: %i\n",
                        InformationObject_getObjectAddress((InformationObject) io),
                        MeasuredValueScaled_getValue((MeasuredValueScaled) io)
                );

                MeasuredValueScaledWithCP56Time2a_destroy(io);
            }
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1)
    {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++)
        {
            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            if (io)
            {
                printf("    IOA: %i value: %i\n",
                        InformationObject_getObjectAddress((InformationObject) io),
                        SinglePointInformation_getValue((SinglePointInformation) io)
                );

                SinglePointInformation_destroy(io);
            }
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == C_TS_TA_1) {
        printf("  test command with timestamp\n");
    }

    return true;
}

int
main(int argc, char** argv)
{
    const char* ip = "localhost";
    uint16_t port = IEC_60870_5_104_DEFAULT_PORT;
    const char* localIp = NULL;
    int localPort = -1;

    if (argc > 1)
        ip = argv[1];

    if (argc > 2)
        port = atoi(argv[2]);

    if (argc > 3)
        localIp = argv[3];

    if (argc > 4)
        port = atoi(argv[4]);

    printf("Connecting to: %s:%i\n", ip, port);
    CS104_Connection con = CS104_Connection_create(ip, port);

    CS101_AppLayerParameters alParams = CS104_Connection_getAppLayerParameters(con);
    alParams->originatorAddress = 3;

    CS104_Connection_setConnectionHandler(con, connectionHandler, NULL);
    CS104_Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

    /* optional bind to local IP address/interface */
    if (localIp)
        CS104_Connection_setLocalAddress(con, localIp, localPort);

    /* uncomment to log messages */
    //CS104_Connection_setRawMessageHandler(con, rawMessageHandler, NULL);

    if (CS104_Connection_connect(con)) {
        printf("Connected!\n");

        CS104_Connection_sendStartDT(con);

        Thread_sleep(2000);

        CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

        Thread_sleep(5000);

        struct sCP56Time2a testTimestamp;
        CP56Time2a_createFromMsTimestamp(&testTimestamp, Hal_getTimeInMs());

        CS104_Connection_sendTestCommandWithTimestamp(con, 1, 0x4938, &testTimestamp);

#if 0
        InformationObject sc = (InformationObject)
                SingleCommand_create(NULL, 5000, true, false, 0);

        printf("Send control command C_SC_NA_1\n");
        CS104_Connection_sendProcessCommandEx(con, CS101_COT_ACTIVATION, 1, sc);

        InformationObject_destroy(sc);

        /* Send clock synchronization command */
        struct sCP56Time2a newTime;

        CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

        printf("Send time sync command\n");
        CS104_Connection_sendClockSyncCommand(con, 1, &newTime);
#endif

        printf("Wait ...\n");

        Thread_sleep(1000);
    }
    else
        printf("Connect failed!\n");

    Thread_sleep(1000);

    CS104_Connection_destroy(con);

    printf("exit\n");
}


