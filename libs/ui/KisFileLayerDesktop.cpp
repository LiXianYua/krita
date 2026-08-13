/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisFileLayerDesktop.h"

#include <algorithm>

#include <QScopedPointer>

#include "KisDocument.h"
#include "KisPart.h"
#include "kis_file_layer.h"
#include "kis_icon_utils.h"
#include "kis_image.h"
#include "kis_layer_utils.h"
#include "kis_paint_device.h"
#include "kis_safe_document_loader.h"

namespace {

KisSafeDocumentLoader::LoadResult loadFileLayerImage(const QString &path)
{
    QScopedPointer<KisDocument> document(KisPart::instance()->createTemporaryDocument());
    document->setFileBatchMode(true);

    if (!document->openPath(path, KisDocument::DontAddToRecent) || !document->image()) {
        return {};
    }

    // Wait for required updates, if any. BUG: 448256
    KisLayerUtils::forceAllDelayedNodesUpdate(document->image()->root());
    document->image()->waitForDone();

    KisPaintDeviceSP paintDevice = new KisPaintDevice(document->image()->colorSpace());
    const KisPaintDeviceSP projection = document->image()->projection();
    paintDevice->makeCloneFrom(projection, projection->extent());

    return {
        paintDevice,
        document->image()->xRes(),
        document->image()->yRes(),
        document->image()->size(),
    };
}

void openFileLayerSource(const QString &absolutePath)
{
    const QList<QPointer<KisDocument>> documents = KisPart::instance()->documents();
    const bool alreadyOpen = std::any_of(documents.cbegin(),
                                         documents.cend(),
                                         [&absolutePath](const QPointer<KisDocument> &document) {
                                             return document && document->path() == absolutePath;
                                         });

    if (!alreadyOpen) {
        KisPart::instance()->openExistingFile(absolutePath);
    }
}

}

void initializeKisFileLayerDesktopServices()
{
    KisSafeDocumentLoader::setDefaultImageLoader(loadFileLayerImage);
    KisFileLayer::setDefaultFileOpener(openFileLayerSource);
    KisFileLayer::setDefaultIconProvider([]() {
        return KisIconUtils::loadIcon(QStringLiteral("fileLayer"));
    });
}

void clearKisFileLayerDesktopServices()
{
    KisSafeDocumentLoader::setDefaultImageLoader({});
    KisFileLayer::setDefaultFileOpener({});
    KisFileLayer::setDefaultIconProvider({});
}
