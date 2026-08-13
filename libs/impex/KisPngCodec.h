/*
 *  SPDX-FileCopyrightText: 2005, 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PNG_CODEC_H
#define KIS_PNG_CODEC_H

#include <QColor>
#include <QList>
#include <QRect>
#include <QString>

#include "KisImportExportErrorCode.h"
#include "kis_annotation.h"
#include "kis_types.h"
#include "kritaimpex_export.h"

class QIODevice;
class KoColorSpace;
class KoDocumentInfo;
class KoStore;
class KisUndoStore;

namespace KisMetaData
{
class Filter;
class Store;
}

struct KRITAIMPEX_EXPORT KisPNGOptions
{
    KisPNGOptions()
        : compression(0)
        , interlace(false)
        , alpha(true)
        , exif(true)
        , iptc(true)
        , xmp(true)
        , tryToSaveAsIndexed(true)
        , saveSRGBProfile(false)
        , forceSRGB(false)
        , storeMetaData(false)
        , storeAuthor(false)
        , saveAsHDR(false)
        , transparencyFillColor(Qt::white)
        , downsample(false)
    {
    }

    int compression;
    bool interlace;
    bool alpha;
    bool exif;
    bool iptc;
    bool xmp;
    bool tryToSaveAsIndexed;
    bool saveSRGBProfile;
    bool forceSRGB;
    bool storeMetaData;
    bool storeAuthor;
    bool saveAsHDR;
    QList<const KisMetaData::Filter *> filters;
    QColor transparencyFillColor;
    bool downsample;
};

/**
 * The document services needed by the PNG codec.  UI document classes adapt
 * to this interface; the codec itself has no window, view, or application
 * dependency.
 */
class KRITAIMPEX_EXPORT KisImportExportDocumentContext
{
public:
    virtual ~KisImportExportDocumentContext() = default;

    virtual KisUndoStore *createUndoStore() = 0;
    virtual KoDocumentInfo *documentInfo() const = 0;
};

struct KRITAIMPEX_EXPORT KisPngImportProfileRequest
{
    QString sourcePath;
    QString colorModelId;
    QString colorDepthId;
};

/**
 * Interaction boundary used only when a 16-bit PNG has no embedded profile.
 * A headless caller leaves the policy null and gets the existing default
 * profile; a UI adapter may choose a compatible profile by name.
 */
class KRITAIMPEX_EXPORT KisPngImportProfilePolicy
{
public:
    virtual ~KisPngImportProfilePolicy() = default;

    virtual QString chooseColorProfile(const KisPngImportProfileRequest &request) = 0;
};

struct KRITAIMPEX_EXPORT KisPngCodecContext
{
    KisImportExportDocumentContext *documentContext = nullptr;
    KisPngImportProfilePolicy *importProfilePolicy = nullptr;
};

/**
 * Headless PNG reader/writer.  File-format work is owned by kritaimpex;
 * application interaction is supplied through KisPngImportProfilePolicy.
 */
class KRITAIMPEX_EXPORT KisPngCodec
{
public:
    explicit KisPngCodec(const KisPngCodecContext &context = KisPngCodecContext());
    ~KisPngCodec();

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

    KisImageSP image() const;

    static bool saveDeviceToStore(const QString &filename,
                                  const QRect &imageRect,
                                  qreal xRes,
                                  qreal yRes,
                                  KisPaintDeviceSP device,
                                  KoStore *store,
                                  KisMetaData::Store *metaData = nullptr);

    static bool isColorSpaceSupported(const KoColorSpace *colorSpace);

    void cancel();

private:
    KisPngCodecContext m_context;
    KisImageSP m_image;
    bool m_stop {false};
    QString m_path;
};

#endif
