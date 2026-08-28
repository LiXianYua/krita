/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimageio_export.h"

#include <kpluginfactory.h>

#include <KisMimeDatabase.h>
#include <KisExportCheckRegistry.h>
#include <KisImportExportBackend.h>
#include <KisDocument.h>

K_PLUGIN_FACTORY_WITH_JSON(KisQImageIOExportFactory, "krita_qimageio_export.json", registerPlugin<KisQImageIOExport>();)

KisQImageIOExport::KisQImageIOExport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisQImageIOExport::~KisQImageIOExport()
{
}

KisImportExportErrorCode KisQImageIOExport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration)
{
    (void)io;
    (void)configuration;
    document->setErrorMessage(PkString("Generic QImageIO formats have no native headless codec"));
    return ImportExportCodes::FormatFeaturesUnsupported;
}

void KisQImageIOExport::initializeCapabilities()
{
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, KisMimeDatabase::descriptionForMimeType(mimeType()));
    addCapability(KisExportCheckRegistry::instance()->get("ColorModelPerLayerCheck/" + RGBAColorModelID.id() + "/" + Integer8BitsColorDepthID.id())->create(KisExportCheckBase::SUPPORTED));
}

KisPropertiesConfigurationSP KisQImageIOExport::defaultConfiguration(const PkByteArray &, const PkByteArray &) const
{
    return {};
}

#include "kis_qimageio_export.moc"
