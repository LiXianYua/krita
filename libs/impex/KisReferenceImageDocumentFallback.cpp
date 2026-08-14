/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisReferenceImageDocumentFallback.h"

#include <QColorSpace>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>

#include <KisDocument.h>
#include <KisReferenceImage.h>
#include <KoStore.h>

#include "KisDocumentRegistry.h"

QImage loadReferenceImageFileWithDocumentFallback(const QString &filename)
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
        KisDocumentRegistry *registry = KisDocumentRegistry::instance();
        KisDocument *document = registry->createTemporaryDocument();
        if (document->openPath(filename, KisDocument::DontAddToRecent)) {
            image = document->image()->convertToQImage(document->image()->bounds(), 0);
        }
        registry->removeDocument(document);
    }

    // See https://bugs.kde.org/show_bug.cgi?id=416515 -- a JPEG image loaded
    // into a QImage cannot be saved to PNG unless its colorspace is explicit.
    image.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
    return image;
}

bool loadReferenceImageWithDocumentFallback(KisReferenceImage *reference, KoStore *store)
{
    return reference->loadImage(store, loadReferenceImageFileWithDocumentFallback);
}
