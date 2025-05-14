#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "cs104_slave.h"
#include "cs101_file_service.h"

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
                if (InformationObject_getObjectAddress(io) == 5000) {
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

static bool
connectionRequestHandler(void* parameter, const char* ipAddress)
{
    printf("New connection request from %s\n", ipAddress);

    return true;
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


    /* set the callback handler for the interrogation command */
    CS104_Slave_setInterrogationHandler(slave, interrogationHandler, NULL);

    /* set handler for other message types */
    CS104_Slave_setASDUHandler(slave, asduHandler, NULL);

    /* set handler to handle connection requests (optional) */
    CS104_Slave_setConnectionRequestHandler(slave, connectionRequestHandler, NULL);

    /* set handler to track connection events (optional) */
    CS104_Slave_setConnectionEventHandler(slave, connectionEventHandler, NULL);

    CS101_FileServer fileServer = CS101_FileServer_create(alParams);

    initializeFiles();

    struct sCS101_FilesAvailable fileAvailable;
    fileAvailable.getFile = getFile;
    fileAvailable.getNextFile = getNextFile;
    fileAvailable.parameter = NULL;

    CS101_FileServer_setFilesAvailableIfc(fileServer, &fileAvailable);
    CS101_FileServer_setFileReadyHandler(fileServer, fileReadyHandler, NULL);

    CS104_Slave_addPlugin(slave, CS101_FileServer_getSlavePlugin(fileServer));

    CS104_Slave_start(slave);

    if (CS104_Slave_isRunning(slave) == false) {
        printf("Starting server failed!\n");
        goto exit_program;
    }

    int16_t scaledValue = 0;

    uint64_t nextSendTime = Hal_getTimeInMs() + 1000;

    while (running)
    {
        if (Hal_getTimeInMs() >= nextSendTime) {

            nextSendTime += 1000;

            CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);

            scaledValue++;

            CS101_ASDU_addInformationObject(newAsdu,
                    (InformationObject) MeasuredValueScaled_create((MeasuredValueScaled)&ioBuf, 110, scaledValue, IEC60870_QUALITY_GOOD));

            /* Add ASDU to slave event queue - don't release the ASDU afterwards!
             * The ASDU will be released by the Slave instance when the ASDU
             * has been sent.
             */
            CS104_Slave_enqueueASDU(slave, newAsdu);
        }

        Thread_sleep(1);
    }

    CS104_Slave_stop(slave);

exit_program:
    CS104_Slave_destroy(slave);
    CS101_FileServer_destroy(fileServer);
}
