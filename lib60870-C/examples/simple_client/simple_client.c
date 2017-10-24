#include "iec60870_master.h"
#include "iec60870_common.h"
#include "hal_time.h"
#include "hal_thread.h"

#include <stdio.h>
#include <stdlib.h>

static void
connectionHandler (void* parameter, T104Connection connection, IEC60870ConnectionEvent event)
{
    switch (event) {
    case IEC60870_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case IEC60870_CONNECTION_CLOSED:
        printf("Connection closed\n");
        break;
    case IEC60870_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case IEC60870_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
        break;
    }
}

static bool
asduReceivedHandler (void* parameter, ASDU asdu)
{
    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(ASDU_getTypeID(asdu)),
            ASDU_getTypeID(asdu),
            ASDU_getNumberOfElements(asdu));

    if (ASDU_getTypeID(asdu) == M_ME_TE_1) {

        printf("  measured scaled values with CP56Time2a timestamp:\n");

        int i;

        for (i = 0; i < ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    MeasuredValueScaled_getValue((MeasuredValueScaled) io)
            );

            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }
    else if (ASDU_getTypeID(asdu) == M_SP_NA_1) {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < ASDU_getNumberOfElements(asdu); i++) {

            SinglePointInformation io =
                    (SinglePointInformation) ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    SinglePointInformation_getValue((SinglePointInformation) io)
            );

            SinglePointInformation_destroy(io);
        }
    }

    return true;
}

static const char* default_ip = "localhost";

int
main(int argc, char** argv)
{
    const char* ip = default_ip;
    uint16_t port = IEC_60870_5_104_DEFAULT_PORT;

    if (argc > 1)
    {
        ip = argv[1];
    }
    if (argc > 2)
    {
        port = strtol(argv[2], NULL, 10);
    }
    printf("Connecting to: %s:%i\n", ip, port);
    T104Connection con = T104Connection_create(ip, port);

    T104Connection_setConnectionHandler(con, connectionHandler, NULL);
    T104Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

    if (T104Connection_connect(con)) {
        printf("Connected!\n");

        T104Connection_sendStartDT(con);

        Thread_sleep(5000);

        T104Connection_sendInterrogationCommand(con, ACTIVATION, 1, IEC60870_QOI_STATION);

        Thread_sleep(5000);

        InformationObject sc = (InformationObject)
                SingleCommand_create(NULL, 5000, true, false, 0);

        printf("Send control command C_SC_NA_1\n");
        T104Connection_sendControlCommand(con, C_SC_NA_1, ACTIVATION, 1, sc);

        InformationObject_destroy(sc);

        /* Send clock synchronization command */
        struct sCP56Time2a newTime;

        CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

        printf("Send time sync command\n");
        T104Connection_sendClockSyncCommand(con, 1, &newTime);

        Thread_sleep(1000);
    }
    else
        printf("Connect failed!\n");

    Thread_sleep(1000);

    T104Connection_destroy(con);

    printf("exit\n");
}


