/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QMessageBox>

#include <klocalizedstring.h>

#include <kis_coordinates_converter.h>
#include <kis_paint_device.h>

#include "KisReferenceImage.h"
#include "KisReferenceImageDocumentFallback.h"
#include "kis_clipboard.h"

KisReferenceImage *KisReferenceImage::fromFile(const QString &filename,
                                               const KisCoordinatesConverter &converter,
                                               QWidget *parent)
{
    KisReferenceImage *reference = fromQImage(converter, loadReferenceImageFileWithDocumentFallback(filename));
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
