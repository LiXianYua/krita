/*
 *  SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisImportExportErrorCode.h"
#include <kis_assert.h>



KisImportExportComplexError::KisImportExportComplexError(PkFileError error) : m_error(error) { }


PkString KisImportExportComplexError::qtErrorMessage() const
{
    // Error descriptions in most cases taken from https://doc.qt.io/qt-5/qfiledevice.html
    PkString unspecifiedError = PkString("An unspecified error occurred.");
    switch (m_error) {
        case PkNoError :
            // Returning this file error may mean that something is wrong in our code.
            // Successful operation should return ImportExportCodes::OK instead.
            return PkString("The action has been completed successfully.");
        case PkReadError :
            return PkString("An error occurred when reading from the file.");
        case PkWriteError :
            return PkString("An error occurred when writing to the file.");
        case PkFatalError :
            return PkString("A fatal error occurred.");
        case PkResourceError :
            return PkString("Out of resources (e.g. out of memory).");
        case PkOpenError :
            return PkString("The file could not be opened.");
        case PkAbortError :
            return PkString("The operation was aborted.");
        case PkTimeOutError :
            return PkString("A timeout occurred.");
        case PkUnspecifiedError :
            return unspecifiedError;
        case PkRemoveError :
            return PkString("The file could not be removed.");
        case PkRenameError :
            return PkString("The file could not be renamed.");
        case PkPositionError :
            return PkString("The position in the file could not be changed.");
        case PkResizeError :
            return PkString("The file could not be resized.");
        case PkPermissionsError :
            return PkString("Permission denied. Krita is not allowed to read or write to the file.");
        case PkCopyError :
            return PkString("The file could not be copied.");
    }
    return unspecifiedError;
}

KisImportExportErrorCannotRead::KisImportExportErrorCannotRead() : KisImportExportComplexError(PkFileError()) { }

KisImportExportErrorCannotRead::KisImportExportErrorCannotRead(PkFileError error) : KisImportExportComplexError(error) {
    KIS_SAFE_ASSERT_RECOVER(error != PkNoError) {m_error = PkReadError; }
}

PkString KisImportExportErrorCannotRead::errorMessage() const
{
    return PkString("Cannot open file for reading. Reason: %1").arg(qtErrorMessage());
}

bool KisImportExportErrorCannotRead::operator==(KisImportExportErrorCannotRead other)
{
    return other.m_error == m_error;
}

KisImportExportErrorCannotWrite::KisImportExportErrorCannotWrite() : KisImportExportComplexError(PkFileError()) { }

KisImportExportErrorCannotWrite::KisImportExportErrorCannotWrite(PkFileError error) : KisImportExportComplexError(error) {
    KIS_SAFE_ASSERT_RECOVER(error != PkNoError) {m_error = PkWriteError; }
}

PkString KisImportExportErrorCannotWrite::errorMessage() const
{
    return PkString("Cannot open file for writing. Reason: %1").arg(qtErrorMessage());
}

bool KisImportExportErrorCannotWrite::operator==(KisImportExportErrorCannotWrite other)
{
    return other.m_error == m_error;
}





KisImportExportErrorCode::KisImportExportErrorCode()
    : errorFieldUsed(None)
    , cannotRead()
    , cannotWrite()
{ }

KisImportExportErrorCode::KisImportExportErrorCode(ImportExportCodes::ErrorCodeID id)
    : errorFieldUsed(CodeId)
    , codeId(id)
    , cannotRead()
    , cannotWrite()
{ }

KisImportExportErrorCode::KisImportExportErrorCode(KisImportExportErrorCannotRead error)
    : errorFieldUsed(CannotRead)
    , codeId(ImportExportCodes::Failure)
    , cannotRead(error)
    , cannotWrite()
{ }

KisImportExportErrorCode::KisImportExportErrorCode(KisImportExportErrorCannotWrite error)
    : errorFieldUsed(CannotWrite)
    , codeId(ImportExportCodes::Failure)
    , cannotRead()
    , cannotWrite(error)
{ }


bool KisImportExportErrorCode::isOk() const
{
    // if cannotRead or cannotWrite is "NoError", it means that something is wrong in our code
    return errorFieldUsed == CodeId && codeId == ImportExportCodes::OK;
}

bool KisImportExportErrorCode::isCancelled() const
{
    return errorFieldUsed == CodeId && codeId == ImportExportCodes::Cancelled;
}

bool KisImportExportErrorCode::isInternalError() const
{
    return errorFieldUsed == CodeId && codeId == ImportExportCodes::InternalError;
}

PkString KisImportExportErrorCode::errorMessage() const
{
    PkString internal = PkString("Unexpected error.");
    if (errorFieldUsed == CannotRead) {
        return cannotRead.errorMessage();
    } else if (errorFieldUsed == CannotWrite) {
        return cannotWrite.errorMessage();
    } else if (errorFieldUsed == CodeId) {
        switch (codeId) {
            // Reading
            case ImportExportCodes::FileNotExist:
                return PkString("The file doesn't exist.");
            case ImportExportCodes::NoAccessToRead:
                return PkString("Permission denied: Krita is not allowed to read the file.");
            case ImportExportCodes::FileFormatIncorrect:
                return PkString("The file format cannot be parsed.");
            case ImportExportCodes::FormatFeaturesUnsupported:
                return PkString("The file format contains unsupported features.");
            case ImportExportCodes::FormatColorSpaceUnsupported:
                return PkString("The file format contains unsupported color space.");
            case ImportExportCodes::ErrorWhileReading:
                return PkString("Error occurred while reading from the file.");

            // Writing
            case ImportExportCodes::CannotCreateFile:
                return PkString("The file cannot be created.");
            case ImportExportCodes::NoAccessToWrite:
                return PkString("Permission denied: Krita is not allowed to write to the file.");
            case ImportExportCodes::InsufficientMemory:
                return PkString("There is not enough disk space left to save the file.");
            case ImportExportCodes::ErrorWhileWriting:
                return PkString("Error occurred while writing to the file.");
            case ImportExportCodes::FileFormatNotSupported:
                return PkString("Krita does not support this file format.");

            // Both
            case ImportExportCodes::Cancelled:
                return PkString("The action was cancelled by the user.");

            // Other
            case ImportExportCodes::Failure:
                return PkString("Unknown error.");
            case ImportExportCodes::InternalError:
                return internal;
            case ImportExportCodes::Busy:
                return PkString("Image is busy.");
            // OK
            case ImportExportCodes::OK:
                return PkString("The action has been completed successfully.");
            default:
                return internal;

        }
    }
    return internal; // errorFieldUsed = None
}



bool KisImportExportErrorCode::operator==(KisImportExportErrorCode errorCode)
{
    if (errorFieldUsed != errorCode.errorFieldUsed) {
        return false;
    }
    if (errorFieldUsed == CodeId) {
        return codeId == errorCode.codeId;
    }
    if (errorFieldUsed == CannotRead) {
        return cannotRead == errorCode.cannotRead;
    }
    return cannotWrite == errorCode.cannotWrite;
}
