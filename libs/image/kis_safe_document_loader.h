/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SAFE_DOCUMENT_LOADER_H
#define __KIS_SAFE_DOCUMENT_LOADER_H

#include <functional>

#include <QObject>
#include <QSize>
#include "kis_paint_device.h"
#include "kis_types.h"
#include "kritaimage_export.h"

class KRITAIMAGE_EXPORT KisSafeDocumentLoader : public QObject
{
    Q_OBJECT
public:
    struct LoadResult {
        KisPaintDeviceSP paintDevice;
        qreal xRes = 0.0;
        qreal yRes = 0.0;
        QSize size;

        explicit operator bool() const
        {
            return bool(paintDevice);
        }
    };

    using ImageLoader = std::function<LoadResult(const QString &path)>;

    KisSafeDocumentLoader(const QString &path = "", QObject *parent = 0);
    KisSafeDocumentLoader(const QString &path, ImageLoader imageLoader, QObject *parent = 0);
    ~KisSafeDocumentLoader() override;

    static void setDefaultImageLoader(ImageLoader imageLoader);

    void setPath(const QString &path);
    void reloadImage();
private Q_SLOTS:
    void fileChanged(QString);
    void slotFileExistsStateChanged(const QString &path, bool fileExists);
    void fileChangedCompressed(bool sync = false);
    void delayedLoadStart();

Q_SIGNALS:
    void loadingFinished(KisPaintDeviceSP paintDevice, qreal xRes, qreal yRes, const QSize &size);
    void loadingFailed();
    void fileExistsStateChanged(bool fileExists);

private:
    struct Private;
    Private * const m_d;
};

#endif /* __KIS_SAFE_DOCUMENT_LOADER_H */
