/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_csv_export.h"
#include "../kis_impex_static_registration.h"
#include <KisExportCheckRegistry.h>
#include <KisImportExportManager.h>
#include <KoColorSpaceConstants.h>

#include <KisDocument.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>

#include "csv_saver.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisCSVExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("text/csv")}, 1,
        []() -> KisImportExportFilter * { return new KisCSVExport(nullptr, PkVariantList()); });
}

KisCSVExport::KisCSVExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisCSVExport::~KisCSVExport()
{
}

KisImportExportErrorCode KisCSVExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    CSVSaver kpc(document, batchMode());

    KisImportExportErrorCode res = kpc.buildAnimation(io);
    return res;
}

void KisCSVExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("AnimationCheck")->create(KisExportCheckBase::SUPPORTED));
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "CSV");
    addCapability(KisExportCheckRegistry::instance()->get("ColorModelPerLayerCheck/" + RGBAColorModelID.id() + "/" + Integer8BitsColorDepthID.id())->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));
}
