/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>
#include <KisImportExportManager.h>
#include <kis_image.h>

namespace
{
class TestDocument final : public KisDocument
{
public:
    TestDocument() : KisDocument(false) {}
};
} // namespace

int main()
{
    TestDocument document;
    if (KisImportExportManager::filterForMimeType(PkString(KIS_MIME_TYPE), KisImportExportManager::Export)) {
        return 2;
    }
    PkImage image(1, 1, PkImage::Format_ARGB32);
    document.setCurrentImage(KisImage::fromQImage(image, document.createUndoStore()));
    const PkByteArray serialized = document.serializeToNativeByteArray();
    return serialized.isEmpty() ? 0 : 1;
}
