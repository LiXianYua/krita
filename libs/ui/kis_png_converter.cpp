/*
 *  SPDX-FileCopyrightText: 2005-2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_converter.h"

#include <QApplication>

#include <KisDocument.h>
#include <KoDocumentInfo.h>

#include "dialogs/kis_dlg_png_import.h"
#include "kis_clipboard.h"
#include "kis_config.h"
#include "kis_cursor_override_hijacker.h"

namespace
{

class KisPngDocumentContextAdapter final : public KisImportExportDocumentContext
{
public:
    explicit KisPngDocumentContextAdapter(KisDocument *document)
        : m_document(document)
    {
    }

    KisUndoStore *createUndoStore() override
    {
        return m_document->createUndoStore();
    }

    KoDocumentInfo *documentInfo() const override
    {
        return m_document->documentInfo();
    }

private:
    KisDocument *m_document;
};

class KisPngImportProfileUiPolicy final : public KisPngImportProfilePolicy
{
public:
    explicit KisPngImportProfileUiPolicy(bool batchMode)
        : m_batchMode(batchMode)
    {
    }

    QString chooseColorProfile(const KisPngImportProfileRequest &request) override
    {
        if (m_batchMode || qAppName().toLower().contains("test")) {
            return QString();
        }

        KisConfig config(true);
        if (config.pasteBehaviour() != KisClipboard::PASTE_ASK) {
            return QString();
        }

        KisDlgPngImport dialog(request.sourcePath,
                              request.colorModelId,
                              request.colorDepthId);
        KisCursorOverrideHijacker cursorOverride;
        Q_UNUSED(cursorOverride);
        dialog.exec();
        return dialog.profile();
    }

private:
    const bool m_batchMode;
};

}

struct KisPNGConverter::Private
{
    Private(KisDocument *document, bool batchMode)
        : documentContext(document)
        , importProfilePolicy(batchMode)
        , codec(KisPngCodecContext {
              document ? &documentContext : nullptr,
              &importProfilePolicy
          })
    {
    }

    KisPngDocumentContextAdapter documentContext;
    KisPngImportProfileUiPolicy importProfilePolicy;
    KisPngCodec codec;
};

KisPNGConverter::KisPNGConverter(KisDocument *document, bool batchMode)
    : d(new Private(document, batchMode))
{
}

KisPNGConverter::~KisPNGConverter() = default;

KisImportExportErrorCode KisPNGConverter::buildImage(const QString &filename)
{
    return d->codec.buildImage(filename);
}

KisImportExportErrorCode KisPNGConverter::buildImage(QIODevice *device)
{
    return d->codec.buildImage(device);
}

KisImportExportErrorCode KisPNGConverter::buildFile(const QString &filename,
                                                     const QRect &imageRect,
                                                     qreal xRes,
                                                     qreal yRes,
                                                     KisPaintDeviceSP device,
                                                     vKisAnnotationSP_it annotationsStart,
                                                     vKisAnnotationSP_it annotationsEnd,
                                                     KisPNGOptions options,
                                                     KisMetaData::Store *metaData)
{
    return d->codec.buildFile(filename,
                              imageRect,
                              xRes,
                              yRes,
                              device,
                              annotationsStart,
                              annotationsEnd,
                              options,
                              metaData);
}

KisImportExportErrorCode KisPNGConverter::buildFile(QIODevice *device,
                                                     const QRect &imageRect,
                                                     qreal xRes,
                                                     qreal yRes,
                                                     KisPaintDeviceSP paintDevice,
                                                     vKisAnnotationSP_it annotationsStart,
                                                     vKisAnnotationSP_it annotationsEnd,
                                                     KisPNGOptions options,
                                                     KisMetaData::Store *metaData)
{
    return d->codec.buildFile(device,
                              imageRect,
                              xRes,
                              yRes,
                              paintDevice,
                              annotationsStart,
                              annotationsEnd,
                              options,
                              metaData);
}

KisImageSP KisPNGConverter::image()
{
    return d->codec.image();
}

bool KisPNGConverter::saveDeviceToStore(const QString &filename,
                                        const QRect &imageRect,
                                        qreal xRes,
                                        qreal yRes,
                                        KisPaintDeviceSP device,
                                        KoStore *store,
                                        KisMetaData::Store *metaData)
{
    return KisPngCodec::saveDeviceToStore(filename,
                                          imageRect,
                                          xRes,
                                          yRes,
                                          device,
                                          store,
                                          metaData);
}

bool KisPNGConverter::isColorSpaceSupported(const KoColorSpace *colorSpace)
{
    return KisPngCodec::isColorSpaceSupported(colorSpace);
}

void KisPNGConverter::cancel()
{
    d->codec.cancel();
}
