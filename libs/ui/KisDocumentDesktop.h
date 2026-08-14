/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_DESKTOP_H
#define KIS_DOCUMENT_DESKTOP_H

#include <KoCanvasResourcesInterface.h>
#include <kis_types.h>

#include "kritaui_export.h"

class KoCanvasResourceProvider;

namespace KisDocumentDesktop
{
KRITAUI_EXPORT KoCanvasResourcesInterfaceSP canvasResourcesForImage(
    KisImageSP requestedImage,
    KisImageSP activeImage,
    KoCanvasResourceProvider *resourceManager);
}

KRITAUI_EXPORT void initializeKisDocumentDesktopServices();
KRITAUI_EXPORT void clearKisDocumentDesktopServices();

#endif // KIS_DOCUMENT_DESKTOP_H
