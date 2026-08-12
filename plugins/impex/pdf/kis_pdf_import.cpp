
/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_pdf_import.h"

// poppler's headers
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <poppler-qt5.h>
#else
#include <poppler-qt6.h>
#endif

// Qt's headers
#include <QFile>
#include <QImage>
#include <QRadioButton>

// For ceil()
#include <math.h>

// KDE's headers
#include <kis_debug.h>
#include <kis_paint_device.h>
#include <kpluginfactory.h>

// calligra's headers
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>

// krita's headers
#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_transaction.h>

// plugins's headers
#include <KisImportExportErrorCode.h>

K_PLUGIN_FACTORY_WITH_JSON(PDFImportFactory, "krita_pdf_import.json",
                           registerPlugin<KisPDFImport>();)

KisPDFImport::KisPDFImport(QObject *parent, const QVariantList &)
    : KisImportExportFilter(parent)
{
}

KisPDFImport::~KisPDFImport()
{
}

KisImportExportErrorCode KisPDFImport::convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP /*configuration*/)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    Poppler::Document* pdoc = Poppler::Document::loadFromData(io->readAll());
#else
    std::unique_ptr<Poppler::Document> pdoc = Poppler::Document::loadFromData(io->readAll());
#endif
    if (!pdoc) {
        dbgFile << "Error when reading the PDF";
        return ImportExportCodes::ErrorWhileReading;
    }

    pdoc->setRenderHint(Poppler::Document::Antialiasing, true);
    pdoc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    if (pdoc->isLocked()) {
        return ImportExportCodes::ErrorWhileReading;
    }

    // 从 KisPDFImportWidget::updateMaxCanvasSize()/updateResolution() 搬来：
    // 取选中页里最大的页面尺寸（单位 pt），/72 得英寸，再乘分辨率并向上取整得像素。
    const int resolution = 300;          // pdfimportwidgetbase.ui 里 intResolution 的默认值
    QList<int> pages;
    pages.push_back(0);                  // KisPDFImportWidget 构造时的默认：仅第一页

    double maxWidthInch = 0., maxHeightInch = 0.;
    for (QList<int>::const_iterator it = pages.constBegin(); it != pages.constEnd(); ++it) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        Poppler::Page *p = pdoc->page(*it);
#else
        std::unique_ptr<Poppler::Page> p = pdoc->page(*it);
#endif
        QSizeF size = p->pageSizeF();
        if (size.width() > maxWidthInch) {
            maxWidthInch = size.width();
        }
        if (size.height() > maxHeightInch) {
            maxHeightInch = size.height();
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        delete p;
#endif
    }
    maxWidthInch /= 72.;
    maxHeightInch /= 72.;

    const int width = (int) ceil(maxWidthInch * resolution);
    const int height = (int) ceil(maxHeightInch * resolution);

    // Create the krita image
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(document->createUndoStore(), width, height, cs, "built image");
    image->setResolution(resolution / 72.0, resolution / 72.0);

    // create a layer
    for (QList<int>::const_iterator it = pages.constBegin(); it != pages.constEnd(); ++it) {
        KisPaintLayer* layer = new KisPaintLayer(image.data(),
                i18n("Page %1", *it + 1),
                quint8_MAX);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        Poppler::Page* page = pdoc->page(*it);
#else
        std::unique_ptr<Poppler::Page> page = pdoc->page(*it);
#endif

        QImage img = page->renderToImage(resolution, resolution, 0, 0, width, height);
        layer->paintDevice()->convertFromQImage(img, 0, 0, 0);

        image->addNode(layer, image->rootLayer(), 0);
        setProgress(qreal(*it + 1) * 100 / pages.count());
    }

    document->setCurrentImage(image);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    delete pdoc;
#endif
    return ImportExportCodes::OK;
}

#include "kis_pdf_import.moc"
