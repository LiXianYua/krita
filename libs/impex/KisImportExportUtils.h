/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISIMPORTEXPORTUTILS_H
#define KISIMPORTEXPORTUTILS_H

#include <mutex>

#include <PkFlags.h>
#include <PkString.h>
#include <PkAuxTypes.h>

#include <kritaimpex_export.h>
#include "KisImportExportErrorCode.h"
#include <KisImageBarrierLock.h>

class KisImportUserFeedbackInterface;


namespace KritaUtils {

enum SaveFlag {
    SaveNone = 0,
    SaveShowWarnings = 0x1,
    SaveIsExporting = 0x2,
    SaveInAutosaveMode = 0x4
};

enum BackgroudSavingStartResult {
    Success = 0,
    Failure = 1,
    AnotherSavingInProgress = 2,
    ImageLockFailure = 3,
    Cancelled = 4
};

using SaveFlags = PkFlags<SaveFlag>;

struct ExportFileJob {
    ExportFileJob()
        : flags(SaveNone)
    {
    }

    ExportFileJob(PkString _filePath, PkByteArray _mimeType, SaveFlags _flags = SaveNone)
        : filePath(_filePath), mimeType(_mimeType), flags(_flags)
    {
    }

    bool isValid() const {
        return !filePath.isEmpty();
    }

    PkString filePath;
    PkByteArray mimeType;
    SaveFlags flags;
};

/**
 * When the image has a colorspace that is not suitable for displaying,
 * Krita should convert that into something more useful. This tool function
 * asks the user about the desired working color space and converts into
 * it.
 */
KisImportExportErrorCode KRITAIMPEX_EXPORT
workaroundUnsuitableImageColorSpace(KisImageSP image,
                                    KisImportUserFeedbackInterface *feedbackInterface,
                                    KisImageBarrierLock &lock);

}

#endif // KISIMPORTEXPORTUTILS_H
