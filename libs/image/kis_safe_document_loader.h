/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SAFE_DOCUMENT_LOADER_H
#define __KIS_SAFE_DOCUMENT_LOADER_H

#include <functional>

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkSize.h>
#include <PkString.h>
#include "kis_paint_device.h"
#include "kis_types.h"
#include "kritaimage_export.h"

class KRITAIMAGE_EXPORT KisSafeDocumentLoader : public PkObject
{
public:
    struct LoadResult {
        KisPaintDeviceSP paintDevice;
        qreal xRes = 0.0;
        qreal yRes = 0.0;
        PkSize size;

        explicit operator bool() const
        {
            return bool(paintDevice);
        }
    };

    using ImageLoader = std::function<LoadResult(const PkString &path)>;

    KisSafeDocumentLoader(const PkString &path = PkString(), PkObject *parent = nullptr);
    KisSafeDocumentLoader(const PkString &path, ImageLoader imageLoader, PkObject *parent = nullptr);
    ~KisSafeDocumentLoader() override;

    static void setDefaultImageLoader(ImageLoader imageLoader);

    void setPath(const PkString &path);
    void reloadImage();

signals:
    void loadingFinished(KisPaintDeviceSP paintDevice, qreal xRes, qreal yRes, PkSize size);
    void loadingFailed();
    void fileExistsStateChanged(bool fileExists);

private:
    void fileChanged(PkString path);
    void slotFileExistsStateChanged(PkString path, bool fileExists);
    void fileChangedCompressed(bool sync = false);
    void delayedLoadStart();

private:
    struct Private;
    Private * const m_d;
};

#endif /* __KIS_SAFE_DOCUMENT_LOADER_H */
