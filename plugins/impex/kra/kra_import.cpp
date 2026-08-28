/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kra_import.h"
#include "../kis_impex_static_registration.h"
#include <KisDocument.h>
#include <kis_image.h>

#include "kra_converter.h"

extern "C" KRITAIMPEX_EXPORT void registerKraImportFilter()
{
    static bool registered = false;
    registerKisImpexFilterOnce(
        registered, {PkString("application/x-krita"), PkString("application/x-krita-archive")}, {}, 1,
        []() -> KisImportExportFilter * { return new KraImport(nullptr, PkVariantList()); });
}

KraImport::KraImport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KraImport::~KraImport()
{
}

KisImportExportErrorCode KraImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KraConverter kraConverter(document);
    KisImportExportErrorCode result = kraConverter.buildImage(io);
    if (result.isOk()) {
        KisNodeSP preActivatedNode = !kraConverter.activeNodes().isEmpty() ? kraConverter.activeNodes().first() : nullptr;
        document->setCurrentImage(kraConverter.image(), true, preActivatedNode);

        if (kraConverter.assistants().size() > 0) {
            document->setAssistants(kraConverter.assistants());
        }
        if (kraConverter.storyboardItemList().size() > 0) {
            document->setStoryboardItemList(kraConverter.storyboardItemList(), true);
        }
        if (kraConverter.storyboardCommentList().size() > 0) {
            document->setStoryboardCommentList(kraConverter.storyboardCommentList(), true);
        }
    }
    return result;
}
