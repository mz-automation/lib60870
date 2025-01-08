/*
 * master_example.c
 */

#include "hal_time.h"
#include "hal_thread.h"
#include "hal_serial.h"
#include "cs101_master.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static bool running = false;

void
sigint_handler(int signalId)
{
    running = false;
}

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

static bool
asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    printf("SLAVE %i: RECVD ASDU type: %s(%i) elements: %i\n",
            address,
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1)
    {
        printf("  measured scaled values with CP56Time2a timestamp (M_ME_TE_1):\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++)
        {
            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu, i);

            if (io != NULL)
            {
                printf("    IOA: %i value: %i\n",
                        InformationObject_getObjectAddress((InformationObject) io),
                        MeasuredValueScaled_getValue((MeasuredValueScaled) io)
                );

                MeasuredValueScaledWithCP56Time2a_destroy(io);
            }
            else {
                printf("     invalid object!\n");
            }
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1) {
        printf("  single point information (M_SP_NA_1):\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++)
        {
            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            if (io != NULL)
            {
                printf("    IOA: %i value: %i\n",
                        InformationObject_getObjectAddress((InformationObject) io),
                        SinglePointInformation_getValue((SinglePointInformation) io)
                );

                SinglePointInformation_destroy(io);
            }
            else {
                printf("     invalid object!\n");
            }
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_EP_TD_1) {
        printf("   event of protection equipment (M_EP_TD_1):\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++)
        {
            EventOfProtectionEquipmentWithCP56Time2a epe = (EventOfProtectionEquipmentWithCP56Time2a)
                    CS101_ASDU_getElement(asdu, i);

            if (epe != NULL)
            {
                SingleEvent singleEvent = EventOfProtectionEquipmentWithCP56Time2a_getEvent(epe);

                printf("    IOA: %i state: %i  QDQ: %i\n",
                                   InformationObject_getObjectAddress((InformationObject) epe),
                                   SingleEvent_getEventState(singleEvent),
                                   SingleEvent_getQDP(singleEvent)
                           );

                EventOfProtectionEquipmentWithCP56Time2a_destroy(epe);
            }
            else {
                printf("     invalid object!\n");
            }
        }
    }

    return true;
}

static void
linkLayerStateChanged(void* parameter, int address, LinkLayerState state)
{
    printf("Link layer state changed for slave %i: ", address);

    switch (state) {
    case LL_STATE_IDLE:
        printf("IDLE\n");
        break;
    case LL_STATE_ERROR:
        printf("ERROR\n");
        break;
    case LL_STATE_BUSY:
        printf("BUSY\n");
        break;
    case LL_STATE_AVAILABLE:
        printf("AVAILABLE\n");
        break;
    }
}

int
main(int argc, char** argv)
{
    signal(SIGINT, sigint_handler);

    const char* serialPort = "/dev/ttyUSB0";

    if (argc > 1)
        serialPort = argv[1];

    SerialPort port = SerialPort_create(serialPort, 9600, 8, 'E', 1);

    CS101_Master master = CS101_Master_create(port, NULL, NULL, IEC60870_LINK_LAYER_UNBALANCED);

    LinkLayerParameters llParams = CS101_Master_getLinkLayerParameters(master);

    llParams->addressLength = 1;

    /* Setting the callback handler for generic ASDUs */
    CS101_Master_setASDUReceivedHandler(master, asduReceivedHandler, NULL);

    /* set callback handler for link layer state changes */
    CS101_Master_setLinkLayerStateChanged(master, linkLayerStateChanged, NULL);

    /* log messages */
    CS101_Master_setRawMessageHandler(master, rawMessageHandler, NULL);

    CS101_Master_addSlave(master, 1);
    CS101_Master_addSlave(master, 2);

    SerialPort_open(port);

    running = true;

    int cycleCounter = 0;

    while (running)
    {
        CS101_Master_pollSingleSlave(master, 1);
        CS101_Master_run(master);

        CS101_Master_pollSingleSlave(master, 2);
        CS101_Master_run(master);

        if (cycleCounter == 10)
        {
            /* Send a general interrogation to a specific slave */
            if (CS101_Master_isChannelReady(master, 1)) {
                CS101_Master_useSlaveAddress(master, 1);
                CS101_Master_sendInterrogationCommand(master, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);
                CS101_Master_run(master);
            }
            else
                cycleCounter--;
        }

        if (cycleCounter == 30)
        {
            /* Send a read request */
            if (CS101_Master_isChannelReady(master, 1)) {
                CS101_Master_useSlaveAddress(master, 1);
                CS101_Master_sendReadCommand(master, 1, 102);
                CS101_Master_run(master);
            }
            else
                cycleCounter--;
        }

        if (cycleCounter == 50)
        {
            if (CS101_Master_isChannelReady(master, 1))
            {
                printf("Send control command C_SC_NA_1\n");

                InformationObject sc = (InformationObject)
                        SingleCommand_create(NULL, 5000, true, false, 0);

                CS101_Master_useSlaveAddress(master, 2);
                CS101_Master_sendProcessCommand(master, CS101_COT_ACTIVATION, 1, sc);
                CS101_Master_run(master);

                InformationObject_destroy(sc);
            }
            else
                cycleCounter--;
        }

        if (cycleCounter == 80)
        {
            /* Send clock synchronization command */

            if (CS101_Master_isChannelReady(master, 1)) {
                struct sCP56Time2a newTime;

                CP56Time2a_createFromMsTimestamp(&newTime, Hal_getTimeInMs());

                printf("Send time sync command\n");
                /* Use broadcast address */
                CS101_Master_useSlaveAddress(master, 255);
                CS101_Master_sendClockSyncCommand(master, 1, &newTime);
                CS101_Master_run(master);
            }
            else
                cycleCounter--;
        }

        Thread_sleep(100);

        cycleCounter++;
    }

    CS101_Master_destroy(master);

    SerialPort_close(port);
    SerialPort_destroy(port);
}
