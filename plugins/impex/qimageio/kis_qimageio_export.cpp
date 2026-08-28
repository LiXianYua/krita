/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimageio_export.h"
#include "../kis_impex_static_registration.h"
#include <KisMimeDatabase.h>
#include <KisExportCheckRegistry.h>
#include <KisImportExportBackend.h>
#include <KisDocument.h>

extern "C" KRITAIMPEX_EXPORT bool registerKisQImageIOExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/bmp"), PkString("image/x-xpixmap"), PkString("image/x-xbitmap"), PkString("image/vnd.microsoft.icon"), PkString("image/x-portable-pixmap"), PkString("image/x-portable-graymap"), PkString("image/x-portable-bitmap"), PkString("image/webp")}, 1,
        []() -> KisImportExportFilter * { return new KisQImageIOExport(nullptr, PkVariantList()); });
}

KisQImageIOExport::KisQImageIOExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisQImageIOExport::~KisQImageIOExport()
{
}

KisImportExportErrorCode KisQImageIOExport::convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration)
{
    (void)io;
    (void)configuration;
    document->setErrorMessage(PkString("Generic image I/O formats have no native headless codec"));
    return ImportExportCodes::FormatFeaturesUnsupported;
}

void KisQImageIOExport::initializeCapabilities()
{
    const PkByteArray mime = mimeType();
    const PkString mimeString = PkString::PkFromUtf8(mime.constData(), mime.size());
    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, KisMimeDatabase::descriptionForMimeType(mimeString));
    addCapability(KisExportCheckRegistry::instance()->get("ColorModelPerLayerCheck/" + mimeString + "/" + Integer8BitsColorDepthID.id())->create(KisExportCheckBase::SUPPORTED));
}

KisPropertiesConfigurationSP KisQImageIOExport::defaultConfiguration(const PkByteArray &, const PkByteArray &) const
{
    return {};
}
