/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISREFERENCEIMAGEDESKTOP_H
#define KISREFERENCEIMAGEDESKTOP_H

#include <kritaui_export.h>

class KisReferenceImage;
class KoStore;

/**
 * Loads embedded or linked reference-image data with the desktop document
 * importer as a fallback for formats that QImageReader cannot decode.
 */
KRITAUI_EXPORT bool loadReferenceImageWithDocumentFallback(KisReferenceImage *reference,
                                                           KoStore *store);

#endif // KISREFERENCEIMAGEDESKTOP_H
