/*
 * slave_example.c
 *
 * Example CS101 slave
 *
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "cs101_slave.h"
#include "cs101_file_service.h"

#include "hal_serial.h"
#include "hal_thread.h"
#include "hal_time.h"

static bool running = true;

void
sigint_handler(int signalId)
{
    running = false;
}

static int fileSize = 800;
static uint8_t sectionData1[500];
static uint8_t sectionData2[500];

static uint64_t
getFileDate (CS101_IFileProvider self)
{

    return 0;
}

static int
getFileSize (CS101_IFileProvider self)
{
    printf("getFileSize --> %i\n", fileSize);

    return fileSize;
}

static int
getSectionSize (CS101_IFileProvider self, int sectionNumber)
{
    printf("getSectionSize(%i)\n", sectionNumber);

    if (sectionNumber == 0)
        return fileSize / 2;
    if (sectionNumber == 1)
        return fileSize / 2;
    else
        return 0;
}

static bool
getSegmentData (CS101_IFileProvider self, int sectionNumber, int offset, int size, uint8_t* data)
{
    printf("getSegmentData(NoS=%i, offset=%i, size=%i)\n", sectionNumber, offset, size);
    if (sectionNumber == 0) {

        int i;

        for (i = 0; i < size; i++)
            data[i] = sectionData1[i + offset];

        return true;
    }
    else if (sectionNumber == 1) {
        int i;

        for (i = 0; i < size; i++)
            data[i] = sectionData2[i + offset];

        return true;
    }
    else
        return false;
}

static void
transferComplete (CS101_IFileProvider self, bool success)
{
    printf("FILE TRANSFER COMPLETE\n");
}

static int numberOfFiles = 1;
static struct sCS101_IFileProvider fileProvider[1];

static void
initializeFiles()
{
    fileProvider[0].ca = 1;
    fileProvider[0].ioa = 30000;
    fileProvider[0].nof = CS101_NOF_TRANSPARENT_FILE;
    fileProvider[0].object = NULL;
    fileProvider[0].getFileSize = getFileSize;
    fileProvider[0].getFileDate = getFileDate;
    fileProvider[0].getSectionSize = getSectionSize;
    fileProvider[0].getSegmentData = getSegmentData;
    fileProvider[0].transferComplete = transferComplete;

    int i;
    for (i = 0; i < fileSize / 2; i++) {
        sectionData1[i] = (uint8_t) (i % 0x100);
        sectionData2[i] = (uint8_t) ((i + 1) % 0x100);
    }
}

static CS101_IFileProvider
getNextFile (void* parameter, CS101_IFileProvider continueAfter)
{
    return NULL;
}

static CS101_IFileProvider
getFile (void* parameter, int ca, int ioa, uint16_t nof, int* errCode)
{
    printf("getFile %i:%i (type:%i)\n", ca, ioa, nof);

    int i;
    for (i = 0; i < numberOfFiles; i++) {
        if ((ca == fileProvider[i].ca) && (ioa == fileProvider[i].ioa)) {
            return &(fileProvider[i]);
        }
    }

    return NULL;
}

static void
IFileReceiver_finished(CS101_IFileReceiver self, CS101_FileErrorCode result)
{
    FILE* fp = (FILE*) self->object;

    fclose(fp);

    if (result != CS101_FILE_ERROR_SUCCESS) {
        remove("upload.dat");
    }

    printf("File upload finished: %i\n", result);
}

static void
IFileReceiver_segmentReceived (CS101_IFileReceiver self, uint8_t sectionName, int offset, int size, uint8_t* data)
{
    FILE* fp = (FILE*) self->object;

    printf("File upload - section %i - offset: %i - size: %i\n", sectionName, offset, size);

    fwrite(data, size, 1, fp);
}

struct sCS101_IFileReceiver myFileReceiver;



static CS101_IFileReceiver
fileReadyHandler (void* parameter, int ca, int ioa, uint16_t nof, int lengthOfFile, int* err)
{
    if ((ca == 1) && (ioa == 30001)) {

        myFileReceiver.object = fopen("upload.dat", "wb");

        myFileReceiver.finished = IFileReceiver_finished;
        myFileReceiver.segmentReceived = IFileReceiver_segmentReceived;

        printf("Accepted file upload\n");

        *err = 0;

        return &myFileReceiver;
    }
    else {
        printf("Rejected file upload - unknown file\n");

        *err = 1;

        return NULL;
    }
}


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

/* Callback handler that is called when a clock synchronization command is received */
static bool
clockSyncHandler (void* parameter, IMasterConnection connection, CS101_ASDU asdu, CP56Time2a newTime)
{
    printf("Process time sync command with time "); printCP56Time2a(newTime); printf("\n");

    return true;
}

/* Callback handler that is called when an interrogation command is received */
static bool
interrogationHandler(void* parameter, IMasterConnection connection, CS101_ASDU asdu, uint8_t qoi)
{
    printf("Received interrogation for group %i\n", qoi);

    if (qoi == 20) { /* only handle station interrogation */

        CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

        IMasterConnection_sendACT_CON(connection, asdu, false);

        /* The CS101 specification only allows information objects without timestamp in GI responses */

        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION,
                0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 100, -1, IEC60870_QUALITY_GOOD);

        CS101_ASDU_addInformationObject(newAsdu, io);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create((MeasuredValueScaled) io, 101, 23, IEC60870_QUALITY_GOOD));

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            MeasuredValueScaled_create((MeasuredValueScaled) io, 102, 2300, IEC60870_QUALITY_GOOD));

        InformationObject_destroy(io);

        IMasterConnection_sendASDU(connection, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_INTERROGATED_BY_STATION,
                    0, 1, false, false);

        io = (InformationObject) SinglePointInformation_create(NULL, 104, true, IEC60870_QUALITY_GOOD);

        CS101_ASDU_addInformationObject(newAsdu, io);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            SinglePointInformation_create((SinglePointInformation) io, 105, false, IEC60870_QUALITY_GOOD));

        InformationObject_destroy(io);

        IMasterConnection_sendASDU(connection, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        newAsdu = CS101_ASDU_create(alParams, true, CS101_COT_INTERROGATED_BY_STATION,
                0, 1, false, false);

        CS101_ASDU_addInformationObject(newAsdu, io = (InformationObject) SinglePointInformation_create(NULL, 300, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 301, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 302, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 303, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 304, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 305, false, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 306, true, IEC60870_QUALITY_GOOD));
        CS101_ASDU_addInformationObject(newAsdu, (InformationObject) SinglePointInformation_create((SinglePointInformation) io, 307, false, IEC60870_QUALITY_GOOD));

        InformationObject_destroy(io);

        IMasterConnection_sendASDU(connection, newAsdu);

        CS101_ASDU_destroy(newAsdu);

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
        }
        else
            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);

        IMasterConnection_sendASDU(connection, asdu);

        return true;
    }

    return false;
}

static void
resetCUHandler(void* parameter)
{
    printf("Received reset CU\n");

    CS101_Slave_flushQueues((CS101_Slave) parameter);
}

static void
linkLayerStateChanged(void* parameter, int address, LinkLayerState state)
{
    printf("Link layer state: ");

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
    /* Add Ctrl-C handler */
    signal(SIGINT, sigint_handler);

    const char* serialPort = "/dev/ttyUSB1";

    if (argc > 1)
        serialPort = argv[1];

    SerialPort port = SerialPort_create(serialPort, 9600, 8, 'E', 1);

    /* create a new slave/server instance with default link layer and application layer parameters */
    // CS101_Slave slave = CS101_Slave_create(port, NULL, NULL, IEC60870_LINK_LAYER_UNBALANCED);
    CS101_Slave slave = CS101_Slave_create(port, NULL, NULL, IEC60870_LINK_LAYER_BALANCED);

    CS101_Slave_setLinkLayerAddress(slave, 3);
    CS101_Slave_setLinkLayerAddressOtherStation(slave, 3);

    /* get the application layer parameters - we need them to create correct ASDUs */
    CS101_AppLayerParameters alParameters = CS101_Slave_getAppLayerParameters(slave);

    LinkLayerParameters llParameters = CS101_Slave_getLinkLayerParameters(slave);
    llParameters->timeoutForAck = 500;


    /* set the callback handler for the clock synchronization command */
    CS101_Slave_setClockSyncHandler(slave, clockSyncHandler, NULL);

    /* set the callback handler for the interrogation command */
    CS101_Slave_setInterrogationHandler(slave, interrogationHandler, NULL);

    /* set handler for other message types */
    CS101_Slave_setASDUHandler(slave, asduHandler, NULL);

    /* set handler for reset CU (reset communication unit) message */
    CS101_Slave_setResetCUHandler(slave, resetCUHandler, (void*) slave);

    /* set timeout for detecting connection loss */
    CS101_Slave_setIdleTimeout(slave, 1500);

    /* set handler for link layer state changes */
    CS101_Slave_setLinkLayerStateChanged(slave, linkLayerStateChanged, NULL);

    CS101_FileServer fileServer = CS101_FileServer_create(alParameters);

    initializeFiles();

    struct sCS101_FilesAvailable fileAvailable;
    fileAvailable.getFile = getFile;
    fileAvailable.getNextFile = getNextFile;
    fileAvailable.parameter = NULL;

    CS101_FileServer_setFilesAvailableIfc(fileServer, &fileAvailable);
    CS101_FileServer_setFileReadyHandler(fileServer, fileReadyHandler, NULL);

    CS101_Slave_addPlugin(slave, CS101_FileServer_getSlavePlugin(fileServer));

    /* uncomment to log messages */
    //CS101_Slave_setRawMessageHandler(slave, rawMessageHandler, NULL);

    int16_t scaledValue = 0;

    uint64_t lastMessageSent = 0;

    SerialPort_open(port);

    while (running) {

        /* has to be called periodically */
        CS101_Slave_run(slave);

        /* Enqueue a measurement every second */
        if (Hal_getTimeInMs() > (lastMessageSent + 1000)) {

            CS101_ASDU newAsdu = CS101_ASDU_create(alParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

            InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

            scaledValue++;

            CS101_ASDU_addInformationObject(newAsdu, io);

            InformationObject_destroy(io);

            CS101_Slave_enqueueUserDataClass2(slave, newAsdu);

            CS101_ASDU_destroy(newAsdu);

            lastMessageSent = Hal_getTimeInMs();
        }

        Thread_sleep(1);
    }


exit_program:
    CS101_Slave_destroy(slave);

    SerialPort_close(port);
    SerialPort_destroy(port);
}
