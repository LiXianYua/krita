/*
 *  SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_IMPORT_EXPORT_ERROR_CODES_H
#define KIS_IMPORT_EXPORT_ERROR_CODES_H


#include <PkString.h>
#include <PkDebug.h>
#include <kritaimpex_export.h>


// 原文件系统错误枚举的零 Qt 替代。枚举值顺序与 Qt 5.15 对齐
// （NoError=0 ... CopyError=14），便于将来 Task 4/8 从 PkFileStream /
// std::error_code 转换时静态映射。
enum PkFileError
{
    PkNoError = 0,
    PkReadError,
    PkWriteError,
    PkFatalError,
    PkResourceError,
    PkOpenError,
    PkAbortError,
    PkTimeOutError,
    PkUnspecifiedError,
    PkRemoveError,
    PkRenameError,
    PkPositionError,
    PkResizeError,
    PkPermissionsError,
    PkCopyError
};


namespace ImportExportCodes
{

    enum ErrorCodeID
    {
        InternalError, // error that shouldn't happen; only inside ASSERTS

        // Reading
        FileNotExist, // there is no file with that name in that location,
        NoAccessToRead, // Krita has no reading access to the file,
        ErrorWhileReading, // there was an error that occurred during reading,
        FileFormatIncorrect, // file format cannot be parsed,
        FormatFeaturesUnsupported, // file format can be parsed, but some features are unsupported,
        FormatColorSpaceUnsupported, // file format can be parsed, but color space of the image is unsupported

        // Writing
        CannotCreateFile, // file cannot be created
        NoAccessToWrite, // Krita has no writing access to the file
        ErrorWhileWriting, // there was an error that occurred during writing (can be insufficient memory, too, just we don't know)
        InsufficientMemory, // there is not enough memory left
        FileFormatNotSupported, // this file format is not supported by Krita

        // Both
        Cancelled, // cancelled by a user

        // Other
        Failure, // unspecified error

        // Could not save because a save operation was already going on
        Busy,

        // OK
        OK, // everything went ok

    };

};

class KisImportExportErrorCode;

struct KRITAIMPEX_EXPORT KisImportExportComplexError
{
    virtual PkString errorMessage() const = 0;
    KisImportExportComplexError(PkFileError error);

    friend inline PkDebug &operator<<(PkDebug &d,
                                      const KisImportExportErrorCode &errorCode);
    virtual ~KisImportExportComplexError() = default;

protected:
    PkString qtErrorMessage() const;
    PkFileError m_error;
};

struct KRITAIMPEX_EXPORT KisImportExportErrorCannotWrite : KisImportExportComplexError
{

    KisImportExportErrorCannotWrite(PkFileError error);
    PkString errorMessage() const override;

    ~KisImportExportErrorCannotWrite() override = default;

    bool operator==(KisImportExportErrorCannotWrite other);

private:
    KisImportExportErrorCannotWrite();

    //friend KisImportExportErrorCode::KisImportExportErrorCode(KisImportExportErrorCannotWrite code);
    friend class KisImportExportErrorCode;

};

struct KRITAIMPEX_EXPORT KisImportExportErrorCannotRead : KisImportExportComplexError
{

    KisImportExportErrorCannotRead(PkFileError error);
    PkString errorMessage() const override;

    ~KisImportExportErrorCannotRead() override = default;

    bool operator==(KisImportExportErrorCannotRead other);

private:
    KisImportExportErrorCannotRead();

    //friend KisImportExportErrorCode::KisImportExportErrorCode(KisImportExportErrorCannotRead code);
    friend class KisImportExportErrorCode;

};



class KRITAIMPEX_EXPORT KisImportExportErrorCode
{
public:
    // required by kis_async_action_feedback
    KisImportExportErrorCode();

    KisImportExportErrorCode(ImportExportCodes::ErrorCodeID code);
    KisImportExportErrorCode(KisImportExportErrorCannotRead code);
    KisImportExportErrorCode(KisImportExportErrorCannotWrite code);

    PkString errorMessage() const;
    bool isOk() const;
    bool isCancelled() const;
    bool isInternalError() const;

    friend inline PkDebug &operator<<(PkDebug &d,
                                      const KisImportExportErrorCode &errorCode);

    bool operator==(KisImportExportErrorCode errorCode);

private:
    enum ErrorFieldUsed
    {
        None,
        CodeId,
        CannotRead,
        CannotWrite
    };

    ErrorFieldUsed errorFieldUsed;

    ImportExportCodes::ErrorCodeID codeId {ImportExportCodes::OK};
    KisImportExportErrorCannotRead cannotRead;
    KisImportExportErrorCannotWrite cannotWrite;
};

inline PkDebug &operator<<(PkDebug &d, const KisImportExportErrorCode &errorCode)
{
    switch (errorCode.errorFieldUsed) {
    case KisImportExportErrorCode::None:
        d << "None of the error fields is in use.";
        break;
    case KisImportExportErrorCode::CannotRead:
        d << "Cannot read: " << errorCode.cannotRead.m_error;
        break;
    case KisImportExportErrorCode::CannotWrite:
        d << "Cannot write: " << errorCode.cannotRead.m_error;
        break;
    case KisImportExportErrorCode::CodeId:
        d << "Error code = " << errorCode.codeId;
    }
    d << " " << errorCode.errorMessage();
    return d;
}

#endif // KIS_IMPORT_EXPORT_ERROR_CODES_H
