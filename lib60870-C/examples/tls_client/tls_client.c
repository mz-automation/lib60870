#include "cs104_connection.h"
#include "hal_thread.h"
#include "hal_time.h"

#include <stdio.h>

static void
connectionHandler(void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    switch (event)
    {
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
asduReceivedHandler(void* parameter, int address, CS101_ASDU asdu)
{
    printf("RECVD ASDU type: %s(%i) elements: %i\n", TypeID_toString(CS101_ASDU_getTypeID(asdu)),
           CS101_ASDU_getTypeID(asdu), CS101_ASDU_getNumberOfElements(asdu));

    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1)
    {
        printf("  measured scaled values with CP56Time2a timestamp:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++)
        {
            MeasuredValueScaledWithCP56Time2a io = (MeasuredValueScaledWithCP56Time2a)CS101_ASDU_getElement(asdu, i);

            if (io)
            {
                printf("    IOA: %i value: %i\n", InformationObject_getObjectAddress((InformationObject)io),
                        MeasuredValueScaled_getValue((MeasuredValueScaled)io));

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
            SinglePointInformation io = (SinglePointInformation)CS101_ASDU_getElement(asdu, i);

            if (io)
            {
                printf("    IOA: %i value: %i\n", InformationObject_getObjectAddress((InformationObject)io),
                    SinglePointInformation_getValue((SinglePointInformation)io));

                SinglePointInformation_destroy(io);
            }
        }
    }

    return true;
}

static void
securityEventHandler(void* parameter, TLSEventLevel eventLevel, int eventCode, const char* msg, TLSConnection con)
{
    (void)parameter;

    char peerAddrBuf[60];
    char* peerAddr = NULL;
    const char* tlsVersion = "unknown";

    if (con)
    {
        peerAddr = TLSConnection_getPeerAddress(con, peerAddrBuf);
        tlsVersion = TLSConfigVersion_toString(TLSConnection_getTLSVersion(con));
    }

    printf("[SECURITY EVENT] %s (t: %i, c: %i, version: %s remote-ip: %s)\n", msg, eventLevel, eventCode, tlsVersion,
           peerAddr);
}

int
main(int argc, char** argv)
{
    char* hostname = "127.0.0.1";

    if (argc > 1)
    {
        hostname = argv[1];
    }

    TLSConfiguration tlsConfig = TLSConfiguration_create();

    TLSConfiguration_setEventHandler(tlsConfig, securityEventHandler, NULL);

    TLSConfiguration_setChainValidation(tlsConfig, true);
    TLSConfiguration_setAllowOnlyKnownCertificates(tlsConfig, true);

    TLSConfiguration_setOwnKeyFromFile(tlsConfig, "client_CA1_1.key", NULL);
    TLSConfiguration_setOwnCertificateFromFile(tlsConfig, "client_CA1_1.pem");
    TLSConfiguration_addCACertificateFromFile(tlsConfig, "root_CA1.pem");

    TLSConfiguration_addAllowedCertificateFromFile(tlsConfig, "server_CA1_1.pem");

    // TLSConfiguration_setMaxTlsVersion(tlsConfig, TLS_VERSION_TLS_1_1);

    CS104_Connection con = CS104_Connection_createSecure(hostname, IEC_60870_5_104_DEFAULT_TLS_PORT, tlsConfig);

    CS104_Connection_setConnectionHandler(con, connectionHandler, NULL);
    CS104_Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

    if (CS104_Connection_connect(con))
    {
        printf("Connected!\n");

        CS104_Connection_sendStartDT(con);

        Thread_sleep(1000);

        CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

        Thread_sleep(1000);

        InformationObject sc = (InformationObject)SingleCommand_create(NULL, 5000, true, false, 0);

        printf("Send control command C_SC_NA_1\n");
        CS104_Connection_sendProcessCommand(con, C_SC_NA_1, CS101_COT_ACTIVATION, 1, sc);

        InformationObject_destroy(sc);

        /* Send clock synchronization command */
        struct sCP56Time2a newTime;

        CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

        printf("Send time sync command\n");
        CS104_Connection_sendClockSyncCommand(con, 1, &newTime);

        Thread_sleep(1000);

        printf("Close connection\n");

        CS104_Connection_close(con);
    }
    else
        printf("Connect failed!\n");

    printf("Waiting...\n");
    Thread_sleep(5000);

    if (CS104_Connection_connect(con))
    {
        printf("Connected!\n");

        CS104_Connection_sendStartDT(con);

        Thread_sleep(1000);

        CS104_Connection_sendInterrogationCommand(con, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

        Thread_sleep(1000);

        InformationObject sc = (InformationObject)SingleCommand_create(NULL, 5000, true, false, 0);

        printf("Send control command C_SC_NA_1\n");
        CS104_Connection_sendProcessCommand(con, C_SC_NA_1, CS101_COT_ACTIVATION, 1, sc);

        InformationObject_destroy(sc);

        /* Send clock synchronization command */
        struct sCP56Time2a newTime;

        CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

        printf("Send time sync command\n");
        CS104_Connection_sendClockSyncCommand(con, 1, &newTime);

        Thread_sleep(1000);

        printf("Close connection\n");
    }
    else
        printf("Connect failed!\n");

    Thread_sleep(1000);

    CS104_Connection_destroy(con);

    TLSConfiguration_destroy(tlsConfig);
}
