/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIS_REFERENCE_IMAGE_DOCUMENT_FALLBACK_H
#define KIS_REFERENCE_IMAGE_DOCUMENT_FALLBACK_H

#include <kritaimpex_export.h>

class KisReferenceImage;
class KoStore;
class PkImage;
class PkString;

/**
 * Loads reference-image data with the document importer as a fallback for
 * formats that the libpng-based decoder cannot decode.
 */
KRITAIMPEX_EXPORT PkImage loadReferenceImageFileWithDocumentFallback(const PkString &filename);
KRITAIMPEX_EXPORT bool loadReferenceImageWithDocumentFallback(KisReferenceImage *reference,
                                                              KoStore *store);

#endif // KIS_REFERENCE_IMAGE_DOCUMENT_FALLBACK_H
