/*
 *  Copyright 2016-2024 Michael Zillgith
 *
 *  This file is part of lib60870-C
 *
 *  lib60870-C is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870-C is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870-C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include "cs101_file_service.h"
#include "lib_memory.h"
#include "iec60870_slave.h"
#include "lib60870_internal.h"
#include "hal_time.h"

typedef enum
{
    UNSELECTED_IDLE,
    WAITING_FOR_FILE_CALL,
    WAITING_FOR_SECTION_CALL,
    TRANSMIT_SECTION,
    WAITING_FOR_SECTION_ACK,
    WAITING_FOR_FILE_ACK,
    SEND_ABORT,
    TRANSFER_COMPLETED,
    WAITING_FOR_SECTION_READY,
    RECEIVE_SECTION,
} FileServerState;

struct sCS101_FileServer
{
    struct sCS101_SlavePlugin plugin;

    CS101_IFileReceiver fileReceiver;

    CS101_FileReadyHandler fileReadyHandler; /* handler that is called when the master wants to send a file */
    void* fileReadyHandlerParameter;

    CS101_FilesAvailable filesAvailable;

    int ca;
    int ioa;
    uint8_t oa;
    uint16_t nof;

    uint64_t lastSendTime;

    FileServerState state;

    uint8_t currentSectionNumber;
    int currentSectionOffset;
    int currentSectionSize;
    uint8_t sectionChecksum;
    uint8_t fileChecksum;

    int maxSegmentSize; /* max. size of segment payload data */

    uint64_t timeout;

    CS101_IFileProvider selectedFile;
    IMasterConnection selectedConnection;
};

static void
sendCallFile(CS101_FileServer self, IMasterConnection connection, int oa)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
        FileCallOrSelect_create((FileCallOrSelect) &ioBuf, self->ioa, self->nof, 0, 2/*=request-file*/));

    IMasterConnection_sendASDU(connection, newAsdu);
}

static void
sendCallSection(CS101_FileServer self, IMasterConnection connection, int oa)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
            FileCallOrSelect_create((FileCallOrSelect) &ioBuf, self->ioa, self->nof, self->currentSectionNumber, 6/*=request-section*/));

    DEBUG_PRINT("Send CALL SECTION number: %i\n", self->currentSectionNumber);

    IMasterConnection_sendASDU(connection, newAsdu);
}

static void
sendFileAck(CS101_FileServer self, IMasterConnection connection, int oa, uint8_t nos, uint8_t afq)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
        FileACK_create((FileACK) &ioBuf, self->ioa, self->nof, nos, afq));

    IMasterConnection_sendASDU(connection, newAsdu);
}

static void
sendSectionReady(CS101_FileServer self, IMasterConnection connection, int oa)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
        SectionReady_create((SectionReady) &ioBuf, self->ioa, self->nof, self->currentSectionNumber, self->currentSectionSize, false));

    IMasterConnection_sendASDU(connection, newAsdu);
}

static void
sendLastSection(CS101_FileServer self, IMasterConnection connection, int oa)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
           FileLastSegmentOrSection_create((FileLastSegmentOrSection) &ioBuf, self->ioa, self->nof, self->currentSectionNumber, 1 /* file-transfer-without-deact */, self->fileChecksum));

    IMasterConnection_sendASDU(connection, newAsdu);
}

static void
sendFileReady(CS101_FileServer self, IMasterConnection connection, int oa, uint32_t lof, bool positive)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
        FileReady_create((FileReady) &ioBuf, self->ioa, self->nof, lof, positive));

    IMasterConnection_sendASDU(connection, newAsdu);
}

static uint8_t
calculateChecksum(uint8_t* data, int size)
{
    uint8_t checksum = 0;

    int i;

    for (i = 0; i < size; i++) {
        checksum += data[i];
    }

    return checksum;
}

static bool
sendSegment(CS101_FileServer self, IMasterConnection connection, int oa)
{
    int currentSegmentSize = self->currentSectionSize - self->currentSectionOffset;

    DEBUG_PRINT("sendSegment(%i/%i)\n", self->currentSectionOffset, self->currentSectionSize);

    if (currentSegmentSize > 0)
    {
        if (currentSegmentSize > self->maxSegmentSize)
            currentSegmentSize = self->maxSegmentSize;

        sCS101_StaticASDU _asdu;
        uint8_t ioBuf[64];
        uint8_t segmentData[255];

        CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

        /* send call file */
        CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
                oa, self->ca, false, false);

        self->selectedFile->getSegmentData(self->selectedFile, self->currentSectionNumber - 1, self->currentSectionOffset, currentSegmentSize, segmentData);

        CS101_ASDU_addInformationObject(newAsdu, (InformationObject)
                FileSegment_create((FileSegment) &ioBuf, self->ioa, self->nof, self->currentSectionNumber, segmentData, currentSegmentSize));

        IMasterConnection_sendASDU(connection, newAsdu);

        self->currentSectionOffset += currentSegmentSize;

        self->lastSendTime = Hal_getMonotonicTimeInMs();
        self->sectionChecksum += calculateChecksum(segmentData, currentSegmentSize);

        return true;
    }
    else
    {
        return false;
    }
}

static void
sendLastSegment(CS101_FileServer self, IMasterConnection connection, int oa)
{
    sCS101_StaticASDU _asdu;
    uint8_t ioBuf[64];

    CS101_AppLayerParameters alParams = IMasterConnection_getApplicationLayerParameters(connection);

    /* send call file */
    CS101_ASDU newAsdu = CS101_ASDU_initializeStatic(&_asdu, alParams, false, CS101_COT_FILE_TRANSFER,
            oa, self->ca, false, false);

    CS101_ASDU_addInformationObject(newAsdu,
            (InformationObject)
            FileLastSegmentOrSection_create((FileLastSegmentOrSection) &ioBuf, self->ioa, self->nof, self->currentSectionNumber,
                    3 /* section-transfer-without-deact */, self->sectionChecksum));

    DEBUG_PRINT("Send LAST SEGMENT (NoS=%i, CHS=%i)\n", self->currentSectionNumber, self->sectionChecksum);

    self->fileChecksum += self->sectionChecksum;
    self->sectionChecksum = 0;

    IMasterConnection_sendASDU(connection, newAsdu);
}

/**
 *
 * Call inside asdu handler
 */
static CS101_SlavePlugin_Result
CS101_FileServer_handleAsdu(void* parameter, IMasterConnection connection,  CS101_ASDU asdu)
{
    CS101_FileServer self = (CS101_FileServer) parameter;

    CS101_SlavePlugin_Result result = CS101_PLUGIN_RESULT_NOT_HANDLED;

    IEC60870_5_TypeID typeId = CS101_ASDU_getTypeID(asdu);

    /* check if type if is in range of file services */
    if ((typeId >= F_FR_NA_1) && (typeId <= F_SC_NB_1))
    {
        /* check for timeout */
        if (self->state != UNSELECTED_IDLE)
        {
            if (Hal_getMonotonicTimeInMs() > self->lastSendTime + self->timeout)
            {
                DEBUG_PRINT ("Abort file transfer due to timeout\n");

                self->state = UNSELECTED_IDLE;
            }
        }

        int oa = CS101_ASDU_getOA(asdu);

        uint8_t ioBuf[250];

        switch (typeId) {

        case F_FR_NA_1: /* File Ready */

            DEBUG_PRINT("Received file ready F_FR_NA_1\n");

            if (self->fileReadyHandler)
            {
                FileReady fileReady = (FileReady) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (fileReady)
                {
                    int ioa = InformationObject_getObjectAddress((InformationObject) fileReady);

                    self->fileReceiver = NULL;

                    int errCode = 0;

                    if (self->fileReadyHandler) {
                        self->fileReceiver = self->fileReadyHandler(self->fileReadyHandlerParameter, CS101_ASDU_getCA(asdu), ioa, FileReady_getNOF(fileReady), FileReady_getLengthOfFile(fileReady), &errCode);
                    }

                    if (self->fileReceiver)
                    {
                        self->ca = CS101_ASDU_getCA(asdu);
                        self->ioa = ioa;
                        self->oa = oa;
                        self->nof = FileReady_getNOF(fileReady);
                        self->fileChecksum = 0;

                        sendCallFile(self, connection, oa);

                        self->lastSendTime = Hal_getMonotonicTimeInMs();
                        self->state = WAITING_FOR_SECTION_READY;
                    }
                    else
                    {
                        if (errCode == 1)
                        {
                            CS101_ASDU_setNegative(asdu, true);
                            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
                            IMasterConnection_sendASDU(connection, asdu);
                        }
                        else if (errCode == 2)
                        {
                            CS101_ASDU_setNegative(asdu, true);
                            CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);
                            IMasterConnection_sendASDU(connection, asdu);
                        }
                        else
                        {
                            self->ca = CS101_ASDU_getCA(asdu);
                            self->ioa = ioa;
                            self->nof = FileReady_getNOF(fileReady);
                            sendFileReady(self, connection, oa, 0, false);
                        }
                    }
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }
            else
            {
                CS101_ASDU_setNegative(asdu, true);
                CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);
                IMasterConnection_sendASDU(connection, asdu);
            }

            break;

        case F_SR_NA_1: /* Section Ready */

            if (self->state == WAITING_FOR_SECTION_READY)
            {
                SectionReady sectionReady = (SectionReady) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (sectionReady)
                {
                    self->currentSectionNumber = SectionReady_getNameOfSection(sectionReady);
                    self->currentSectionOffset = 0;
                    self->currentSectionSize = SectionReady_getLengthOfSection(sectionReady);

                    /* send call section */
                    sendCallSection(self, connection, oa);

                    self->lastSendTime = Hal_getMonotonicTimeInMs();
                    self->state = RECEIVE_SECTION;
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }

            break;

        case F_SG_NA_1: /* Segment */

            if (self->state == RECEIVE_SECTION)
            {
                FileSegment segment = (FileSegment) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (segment)
                {
                    uint8_t nos = FileSegment_getNameOfSection(segment);
                    uint8_t los = FileSegment_getLengthOfSegment(segment);

                    DEBUG_PRINT("Received F_SG_NA_1(segment) (NoS=%i, LoS=%i)\n", nos, los);

                    if (self->fileReceiver)
                    {
                        self->fileReceiver->segmentReceived(self->fileReceiver, nos, self->currentSectionOffset, los, FileSegment_getSegmentData(segment));
                    }

                    self->currentSectionOffset += los;

                    self->lastSendTime = Hal_getMonotonicTimeInMs();
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }
            else {
                DEBUG_PRINT ("Unexpected F_SG_NA_1(file segment)\n");
            }

            break;

        case F_LS_NA_1: /* Last Segment/Section */
            {
                DEBUG_PRINT ("Received F_LS_NA_1 (last segment/section)\n");

                FileLastSegmentOrSection lastSection = (FileLastSegmentOrSection) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (lastSection)
                {
                    uint8_t lsq = FileLastSegmentOrSection_getLSQ(lastSection);

                    if (self->state == RECEIVE_SECTION)
                    {
                        if (lsq == 3 /* SECTION_TRANSFER_WITHOUT_DEACT */)
                        {
                            DEBUG_PRINT("Send segment ACK for NoS=%i\n", FileLastSegmentOrSection_getNameOfSection(lastSection));

                            sendFileAck(self, connection, oa, FileLastSegmentOrSection_getNameOfSection(lastSection), 3 /* POS_ACK_SECTION */);

                            self->lastSendTime = Hal_getMonotonicTimeInMs();
                            self->state = WAITING_FOR_SECTION_READY;

                        }
                        else if (lsq == 2 /* FILE_TRANSFER_WITH_DEACT */)
                        {
                            /* master aborted transfer */

                            if (self->fileReceiver) {
                                self->fileReceiver->finished(self->fileReceiver, CS101_FILE_ERROR_ABORTED_BY_REMOTE);
                            }

                            self->state = UNSELECTED_IDLE;
                        }
                        else {
                            DEBUG_PRINT("Unexpected LSQ\n");
                        }

                    }
                    else if (self->state == WAITING_FOR_SECTION_READY)
                    {
                        if (lsq == 1 /* FILE_TRANSFER_WITHOUT_DEACT */)
                        {
                            DEBUG_PRINT("Send file ACK\n");

                            sendFileAck(self, connection, oa, FileLastSegmentOrSection_getNameOfSection(lastSection), 1 /* POS_ACK_FILE */);

                            self->lastSendTime = Hal_getMonotonicTimeInMs();

                            if (self->fileReceiver)
                            {
                                self->fileReceiver->finished(self->fileReceiver, CS101_FILE_ERROR_SUCCESS);
                            }

                            self->state = UNSELECTED_IDLE;
                        }
                        else if (lsq == 2 /* FILE_TRANSFER_WITH_DEACT */)
                        {
                            /* master aborted transfer */

                            if (self->fileReceiver)
                            {
                                self->fileReceiver->finished(self->fileReceiver, CS101_FILE_ERROR_ABORTED_BY_REMOTE);
                            }

                            self->state = UNSELECTED_IDLE;
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected LSQ\n");
                        }
                    }
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }

            break;

        case F_AF_NA_1: /*  124 - ACK file, ACK section */

            DEBUG_PRINT("Received file/section ACK F_AF_NA_1\n");

            if (self->state != UNSELECTED_IDLE)
            {
                FileACK ack = (FileACK) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (ack)
                {
                    uint8_t afq = FileACK_getAFQ(ack);

                    if (afq == 1 /* POS_ACK_FILE */)
                    {
                        DEBUG_PRINT("Received positive file ACK\n");

                        if (self->state == WAITING_FOR_FILE_ACK)
                        {
                            if (self->selectedFile)
                                self->selectedFile->transferComplete(self->selectedFile, true);

                            /* TODO remove file from list of available files */

                            self->selectedFile = NULL;
                            self->selectedConnection = NULL;

                            self->state = UNSELECTED_IDLE;
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected file transfer state --> abort file transfer\n");

                            self->state = SEND_ABORT;
                        }
                    }
                    else if ((afq & 0x0f) == 2 /* NEG_ACK_FILE */)
                    {
                        DEBUG_PRINT("Received negative file ACK - stop transfer\n");

                        if (self->state == WAITING_FOR_FILE_ACK)
                        {
                            if (self->selectedFile)
                                self->selectedFile->transferComplete(self->selectedFile, false);

                            self->selectedFile = NULL;
                            self->selectedConnection = NULL;

                            self->state = UNSELECTED_IDLE;
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected file transfer state --> abort file transfer\n");

                            self->state = SEND_ABORT;
                        }
                    }
                    else if ((afq & 0x0f) == 4 /* NEG_ACK_SECTION */)
                    {
                        DEBUG_PRINT("Received negative file section ACK - repeat section\n");

                        if (self->state == WAITING_FOR_SECTION_ACK)
                        {
                            self->currentSectionOffset = 0;
                            self->sectionChecksum = 0;

                            sendSectionReady(self, connection, oa);

                            self->lastSendTime = Hal_getMonotonicTimeInMs();
                            self->state = TRANSMIT_SECTION;

                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected file transfer state --> abort file transfer\n");

                            self->state = SEND_ABORT;
                        }
                    }
                    else if ((afq & 0x0f) == 3 /* POS_ACK_SECTION */)
                    {
                        DEBUG_PRINT("Received positive section ACK\n");

                        if (self->state == WAITING_FOR_SECTION_ACK)
                        {
                            self->currentSectionNumber++;

                            int nextSectionSize = self->selectedFile->getSectionSize(self->selectedFile, self->currentSectionNumber - 1);

                            self->currentSectionOffset = 0;

                            if (nextSectionSize <= 0)
                            {
                                DEBUG_PRINT("Received positive file section ACK - send last section indication\n");

                                sendLastSection(self, connection, oa);

                                self->lastSendTime = Hal_getMonotonicTimeInMs();
                                self->state = WAITING_FOR_FILE_ACK;
                            }
                            else
                            {
                                DEBUG_PRINT("Received positive file section ACK - send next section ready indication");

                                self->currentSectionSize = nextSectionSize;

                                sendSectionReady(self, connection, oa);

                                self->lastSendTime = Hal_getMonotonicTimeInMs();
                                self->state = WAITING_FOR_SECTION_CALL;
                            }

                            self->sectionChecksum = 0;
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected file transfer state --> abort file transfer\n");

                            self->state = SEND_ABORT;
                        }
                    }
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }
            else
            {
                CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
                IMasterConnection_sendASDU(connection, asdu);
            }

            break;

        case F_SC_NA_1: /* 122 - Call/Select directory/file/section */

            DEBUG_PRINT("Received call/select F_SC_NA_1\n");

            if (CS101_ASDU_getCOT(asdu) == CS101_COT_FILE_TRANSFER)
            {
                FileCallOrSelect sc = (FileCallOrSelect) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

                if (sc)
                {
                    uint8_t scq = FileCallOrSelect_getSCQ(sc);
                    int ioa = InformationObject_getObjectAddress((InformationObject) sc);
                    uint16_t nof = FileCallOrSelect_getNOF(sc);

                    if (scq == 1 /* SELECT_FILE */) 
                    {
                        DEBUG_PRINT("Received SELECT FILE\n");

                        if (self->state == UNSELECTED_IDLE)
                        {
                            CS101_IFileProvider file = NULL;

                            int errCode = 0;

                            if (self->filesAvailable)
                                file = self->filesAvailable->getFile(self->filesAvailable->parameter, CS101_ASDU_getCA(asdu), ioa, nof, &errCode);

                            if (file == NULL)
                            {
                                if (errCode == 1)
                                {
                                    CS101_ASDU_setNegative(asdu, true);
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
                                    IMasterConnection_sendASDU(connection, asdu);
                                }
                                else if (errCode == 2)
                                {
                                    CS101_ASDU_setNegative(asdu, true);
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);
                                    IMasterConnection_sendASDU(connection, asdu);
                                }
                                else
                                {
                                    self->ca = CS101_ASDU_getCA(asdu);
                                    self->ioa = ioa;
                                    self->nof = nof;
                                    sendFileReady(self, connection, oa, 0, false);
                                }
                            }
                            else
                            {
                                /* TODO check if file is already selected by other client */

                                self->selectedFile = file;
                                self->selectedConnection = connection;
                                self->ioa = ioa;
                                self->ca = CS101_ASDU_getCA(asdu);
                                self->nof = nof;

                                int fileSize = file->getFileSize(file);

                                DEBUG_PRINT("Send FILE READY\n");

                                sendFileReady(self, connection, oa, fileSize, true);

                                self->lastSendTime = Hal_getMonotonicTimeInMs();
                                self->state = WAITING_FOR_FILE_CALL;
                            }
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected select file message\n");
                        }

                    }
                    else if (scq == 3 /* DEACTIVATE_FILE */)
                    {
                        DEBUG_PRINT("Received DEACTIVATE FILE\n");

                        if (self->state == UNSELECTED_IDLE)
                        {
                            self->selectedFile = NULL;
                            self->selectedConnection = NULL;

                            self->state = UNSELECTED_IDLE;
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected DEACTIVATE FILE message\n");
                        }
                    }
                    else if (scq == 2 /* REQUEST_FILE */)
                    {
                        DEBUG_PRINT("Received CALL FILE\n");

                        if (self->state == WAITING_FOR_FILE_CALL)
                        {
                            /* TODO check if NoF matches */

                            if ((ioa != self->ioa) || (CS101_ASDU_getCA(asdu) != self->ca))
                            {
                                DEBUG_PRINT("Call for file that is not selected!\n");

                                if (CS101_ASDU_getCA(asdu) != self->ca)
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
                                else
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);

                                CS101_ASDU_setNegative(asdu, true);

                                IMasterConnection_sendASDU(connection, asdu);
                            }
                            else
                            {
                                self->currentSectionNumber = 1;
                                self->currentSectionOffset = 0;
                                self->fileChecksum = 0;
                                self->currentSectionSize = self->selectedFile->getSectionSize(self->selectedFile, 0);

                                DEBUG_PRINT("Send SECTION READY\n");

                                sendSectionReady(self, connection, oa);

                                self->lastSendTime = Hal_getMonotonicTimeInMs();
                                self->state = WAITING_FOR_SECTION_CALL;
                            }
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected CALL FILE message\n");
                        }

                    }
                    else if (scq == 6 /* REQUEST_SECTION */ )
                    {
                        uint8_t nos = FileCallOrSelect_getNameOfSection(sc);

                        DEBUG_PRINT("Received CALL SECTION (NoS = %i)\n", nos);

                        if (self->state == WAITING_FOR_SECTION_CALL)
                        {
                            /* TODO check if NoF matches */

                            if ((ioa != self->ioa) || (CS101_ASDU_getCA(asdu) != self->ca))
                            {
                                DEBUG_PRINT("Call for file that is not selected!\n");

                                if (CS101_ASDU_getCA(asdu) != self->ca)
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
                                else
                                    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);

                                CS101_ASDU_setNegative(asdu, true);

                                IMasterConnection_sendASDU(connection, asdu);

                            }
                            else
                            {
                                if (CS101_ASDU_isNegative(asdu))
                                {
                                    self->currentSectionNumber++;
                                    self->currentSectionOffset = 0;

                                    self->currentSectionSize = self->selectedFile->getSectionSize(self->selectedFile, self->currentSectionNumber - 1);

                                    if (self->currentSectionSize > 0)
                                    {
                                        DEBUG_PRINT("Send F_SR_NA_1 (section ready) (NoS = %i)\n", self->currentSectionNumber);

                                        sendSectionReady(self, connection, oa);

                                        self->lastSendTime = Hal_getMonotonicTimeInMs();
                                        self->state = WAITING_FOR_SECTION_CALL;
                                    }
                                    else
                                    {
                                        DEBUG_PRINT("Send F_LS_NA_1 (last section))\n");

                                        sendLastSection(self, connection, oa);

                                        self->lastSendTime = Hal_getMonotonicTimeInMs();
                                        self->state = WAITING_FOR_FILE_ACK;
                                    }
                                }
                                else
                                {
                                    /* positive */
                                    self->currentSectionSize = self->selectedFile->getSectionSize(self->selectedFile, nos - 1);

                                    if (self->currentSectionSize > 0)
                                    {
                                        self->currentSectionNumber = nos;
                                        self->currentSectionOffset = 0;

                                        self->state = TRANSMIT_SECTION;
                                    }
                                    else
                                    {
                                        DEBUG_PRINT("Unexpected number of section -> send negative confirm\n");

                                        CS101_ASDU_setNegative(asdu, true);

                                        IMasterConnection_sendASDU(connection, asdu);

                                        self->lastSendTime = Hal_getMonotonicTimeInMs();
                                    }
                                }
                            }
                        }
                        else
                        {
                            DEBUG_PRINT("Unexpected CALL SECTION message\n");
                        }
                    }
                }
                else
                    result = CS101_PLUGIN_RESULT_INVALID_ASDU;
            }
            else if (CS101_ASDU_getCOT(asdu) == CS101_COT_REQUEST)
            {
                /* call directory */

                /* TODO send directory */
            }
            else
            {
                DEBUG_PRINT("Received unexpected COT = %i\n", CS101_ASDU_getCOT(asdu));
            }

            break;

        default:
            DEBUG_PRINT("Received unexpected type ID %i in file service\n", typeId);
            break;
        }

        result = CS101_PLUGIN_RESULT_HANDLED;
    }

    return result;
}

static void
CS101_FileServer_runTask(void* parameter, IMasterConnection connection)
{
    CS101_FileServer self = (CS101_FileServer) parameter;

    if (self->state != UNSELECTED_IDLE)
    {
        if (self->state == TRANSMIT_SECTION)
        {
            if (self->selectedConnection == connection)
            {
                if (self->selectedFile)
                {
                    if (sendSegment(self, connection, self->oa) == false)
                    {
                        sendLastSegment(self, connection, self->oa);

                        self->fileChecksum += self->sectionChecksum;
                        self->sectionChecksum = 0;

                        self->lastSendTime = Hal_getMonotonicTimeInMs();
                        self->state = WAITING_FOR_SECTION_ACK;
                    }
                }
            }
        }

        /* check for timeout */
        if (Hal_getMonotonicTimeInMs() > self->lastSendTime + self->timeout)
        {
            DEBUG_PRINT ("Abort file transfer due to timeout\n");

             self->state = UNSELECTED_IDLE;
        }
    }
}

bool (*handleAsdu) (void* parameter, IMasterConnection connection, CS101_ASDU asdu);
void (*runTask) (void* parameter, IMasterConnection connection);

void* parameter;

CS101_FileServer
CS101_FileServer_create(CS101_AppLayerParameters alParams)
{
    CS101_FileServer self = (CS101_FileServer) GLOBAL_CALLOC(1, sizeof(struct sCS101_FileServer));

    if (self)
    {
        self->timeout = 3000;
        self->state = UNSELECTED_IDLE;

        self->maxSegmentSize = FileSegment_GetMaxDataSize(alParams);

        self->plugin.parameter = self;
        self->plugin.handleAsdu = CS101_FileServer_handleAsdu;
        self->plugin.runTask = CS101_FileServer_runTask;
    }

    return self;
}

void
CS101_FileServer_destroy(CS101_FileServer self)
{
    GLOBAL_FREEMEM(self);
}

void
CS101_FileServer_setFilesAvailableIfc(CS101_FileServer self, CS101_FilesAvailable ifc)
{
    self->filesAvailable = ifc;
}

void
CS101_FileServer_setFileReadyHandler(CS101_FileServer self, CS101_FileReadyHandler handler, void* parameter)
{
    self->fileReadyHandler = handler;
    self->fileReadyHandlerParameter = parameter;
}

CS101_SlavePlugin
CS101_FileServer_getSlavePlugin(CS101_FileServer self)
{
    return &(self->plugin);
}
