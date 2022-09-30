/*
 *  Copyright 2016-2022 Michael Zillgith
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

#ifndef LIB60870_C_FILE_SERVICE_FILE_SERVICE_H_
#define LIB60870_C_FILE_SERVICE_FILE_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "iec60870_common.h"
#include "iec60870_slave.h"

typedef enum
{
    CS101_FILE_ERROR_SUCCESS,
    CS101_FILE_ERROR_TIMEOUT,
    CS101_FILE_ERROR_FILE_NOT_READY,
    CS101_FILE_ERROR_SECTION_NOT_READY,
    CS101_FILE_ERROR_UNKNOWN_CA,
    CS101_FILE_ERROR_UNKNOWN_IOA,
    CS101_FILE_ERROR_UNKNOWN_SERVICE,
    CS101_FILE_ERROR_PROTOCOL_ERROR,
    CS101_FILE_ERROR_ABORTED_BY_REMOTE
} CS101_FileErrorCode;

typedef struct sCS101_FileServer* CS101_FileServer;

typedef struct sCS101_IFileReceiver* CS101_IFileReceiver;

struct sCS101_IFileReceiver
{
    void* object; /* user provided context object */

    void (*finished) (CS101_IFileReceiver self, CS101_FileErrorCode result);
    void (*segmentReceived) (CS101_IFileReceiver self, uint8_t sectionName, int offset, int size, uint8_t* data);
};

typedef struct sCS101_IFileProvider* CS101_IFileProvider;

struct sCS101_IFileProvider {
    int ca;
    int ioa;
    uint8_t nof;

    void* object; /* user provided context object */

    uint64_t (*getFileDate) (CS101_IFileProvider self);
    int (*getFileSize) (CS101_IFileProvider self);
    int (*getSectionSize) (CS101_IFileProvider self, int sectionNumber);

    /**
     * \brief Get section data (will be called by the file service implementation)
     *
     * \param offset byte offset of segment in the section
     * \param size of segment (has to be at most the size of the provided data buffer)
     * \param data buffer to store the segment data
     */
    bool (*getSegmentData) (CS101_IFileProvider self, int sectionNumber, int offset, int size, uint8_t* data);

    void (*transferComplete) (CS101_IFileProvider self, bool success);
};

/**
 * \brief Will be called by the \ŕef CS101_FileServer when the master sends a FILE READY (file download announcement) message to the slave
 *
 * \param parameter user provided context parameter
 * \param ca CA of the file
 * \param ioa IOA of the file
 * \param nof the name of file (file type) of the file
 * \param lengthOfFile the length of the file to be sent
 * \param[out] errCode error code of file service (0 - ok, 1 - unknown CA, 2 - unknown IOA, 3-unknown name of file, 4 - file not ready )
 *
 * \return file handle (interface to access the file)
 */
typedef CS101_IFileReceiver (*CS101_FileReadyHandler) (void* parameter, int ca, int ioa, uint16_t nof, int lengthOfFile, int* errCode);

typedef struct sCS101_FilesAvailable* CS101_FilesAvailable;

struct sCS101_FilesAvailable
{
    /**
     * Used to handle request directory call
     *
     * \return The next file or NULL if no more file available
     */
    CS101_IFileProvider (*getNextFile) (void* parameter, CS101_IFileProvider continueAfter);

    CS101_IFileProvider (*getFile) (void* parameter, int ca, int ioa, uint16_t nof, int* errCode);

    void* parameter;
};


/**
 * \brief Handle transparent files (implement CS101_IFileProvider interface)
 */
typedef struct sCS101_TransparentFile* CS101_TransparentFile;

CS101_IFileProvider
CS101_TransparentFile_create(int ca, int ioa, uint8_t nof);

CS101_FileServer
CS101_FileServer_create(CS101_AppLayerParameters alParams);

void
CS101_FileServer_destroy(CS101_FileServer self);

void
CS101_FileServer_setFilesAvailableIfc(CS101_FileServer self, CS101_FilesAvailable ifc);

void
CS101_FileServer_setFileReadyHandler(CS101_FileServer self, CS101_FileReadyHandler handler, void* parameter);

CS101_SlavePlugin
CS101_FileServer_getSlavePlugin(CS101_FileServer self);

#ifdef __cplusplus
}
#endif


#endif /* LIB60870_C_FILE_SERVICE_FILE_SERVICE_H_ */
