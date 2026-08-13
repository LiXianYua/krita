/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisReferenceImageDesktop.h"

#include <QColorSpace>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMessageBox>

#include <klocalizedstring.h>

#include <KisDocument.h>
#include <KisPart.h>
#include <kis_coordinates_converter.h>

#include "KisReferenceImage.h"
#include "kis_clipboard.h"

namespace
{

QImage loadReferenceImageFile(const QString &filename)
{
    QImage image;

    if (QFileInfo(filename).exists() && QFileInfo(filename).isReadable()) {
        QImageReader reader(filename);
        reader.setDecideFormatFromContent(true);
        image = reader.read();

        if (image.isNull()) {
            reader.setAutoDetectImageFormat(true);
            image = reader.read();
        }

        if (image.isNull()) {
            image.load(filename);
        }
    }

    if (image.isNull()) {
        KisDocument *document = KisPart::instance()->createTemporaryDocument();
        if (document->openPath(filename, KisDocument::DontAddToRecent)) {
            image = document->image()->convertToQImage(document->image()->bounds(), 0);
        }
        KisPart::instance()->removeDocument(document);
    }

    // See https://bugs.kde.org/show_bug.cgi?id=416515 -- a JPEG image loaded
    // into a QImage cannot be saved to PNG unless its colorspace is explicit.
    image.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
    return image;
}

}

KisReferenceImage *KisReferenceImage::fromFile(const QString &filename,
                                               const KisCoordinatesConverter &converter,
                                               QWidget *parent)
{
    KisReferenceImage *reference = fromQImage(converter, loadReferenceImageFile(filename));
    if (reference) {
        reference->setFilename(filename);
    } else if (parent) {
        QMessageBox::critical(parent,
                              i18nc("@title:window", "Krita"),
                              i18n("Could not load %1.", filename));
    }

    return reference;
}

KisReferenceImage *KisReferenceImage::fromClipboard(const KisCoordinatesConverter &converter)
{
    const auto size = KisClipboard::instance()->clipSize();
    KisPaintDeviceSP clip = KisClipboard::instance()->clip({0, 0, size.width(), size.height()}, true);
    return fromPaintDevice(clip, converter, nullptr);
}

bool loadReferenceImageWithDocumentFallback(KisReferenceImage *reference, KoStore *store)
{
    return reference->loadImage(store, loadReferenceImageFile);
}
