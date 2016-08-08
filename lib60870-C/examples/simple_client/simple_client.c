#include "iec60870_client.h"
#include "iec60870_common.h"

#include <stdio.h>

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
                    MeasuredValueScaledWithCP56Time2a_getObjectAddress(io),
                    MeasuredValueScaledWithCP56Time2a_getScaledValue(io)
            );

            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }

    return true;
}

int
main(int argc, char** argv)
{
    T104Connection con = T104Connection_create("127.0.0.1", IEC_60870_5_104_DEFAULT_PORT);

    if (T104Connection_connectBlocking(con)) {
        printf("Connected!\n");

        T104Connection_setASDUReceivedHandler(con, asduReceivedHandler, NULL);

        T104Connection_sendStartDT(con);

        Thread_sleep(5000);

        T104Connection_sendInterrogationCommand(con, ACTIVATION, 1, 20);

        Thread_sleep(5000);
    }
    else
        printf("Connect failed!\n");

    T104Connection_destroy(con);

    printf("exit\n");
}


