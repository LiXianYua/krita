/*
 *  SPDX-FileCopyrightText: 2005, 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PNG_CONVERTER_H
#define KIS_PNG_CONVERTER_H

#include <QObject>
#include <QScopedPointer>

#include <KisPngCodec.h>
#include <kritaui_export.h>

class KisDocument;

/**
 * Desktop adapter for KisPngCodec.  The PNG algorithm lives in kritaimpex;
 * this class only supplies document services and the optional profile dialog.
 */
class KRITAUI_EXPORT KisPNGConverter : public QObject
{
    Q_OBJECT

public:
    explicit KisPNGConverter(KisDocument *document, bool batchMode = false);
    ~KisPNGConverter() override;

    KisImportExportErrorCode buildImage(const QString &filename);
    KisImportExportErrorCode buildImage(QIODevice *device);

    KisImportExportErrorCode buildFile(const QString &filename,
                                       const QRect &imageRect,
                                       qreal xRes,
                                       qreal yRes,
                                       KisPaintDeviceSP device,
                                       vKisAnnotationSP_it annotationsStart,
                                       vKisAnnotationSP_it annotationsEnd,
                                       KisPNGOptions options,
                                       KisMetaData::Store *metaData);
    KisImportExportErrorCode buildFile(QIODevice *device,
                                       const QRect &imageRect,
                                       qreal xRes,
                                       qreal yRes,
                                       KisPaintDeviceSP paintDevice,
                                       vKisAnnotationSP_it annotationsStart,
                                       vKisAnnotationSP_it annotationsEnd,
                                       KisPNGOptions options,
                                       KisMetaData::Store *metaData);

    KisImageSP image();

    static bool saveDeviceToStore(const QString &filename,
                                  const QRect &imageRect,
                                  qreal xRes,
                                  qreal yRes,
                                  KisPaintDeviceSP device,
                                  KoStore *store,
                                  KisMetaData::Store *metaData = nullptr);

    static bool isColorSpaceSupported(const KoColorSpace *colorSpace);

public Q_SLOTS:
    void cancel();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

#endif
