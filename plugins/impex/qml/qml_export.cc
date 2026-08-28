/*
 *  SPDX-FileCopyrightText: 2013 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "qml_export.h"
#include "../kis_impex_static_registration.h"
#include <KisExportCheckRegistry.h>
#include <KisImportExportBackend.h>
#include <kis_image.h>

#include "qml_converter.h"
#include <KoColorModelStandardIds.h>

extern "C" KRITAIMPEX_EXPORT bool registerQMLExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("text/x-qml")}, 1,
        []() -> KisImportExportFilter * { return new QmlExport(nullptr, PkVariantList()); });
}

QmlExport::QmlExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

QmlExport::~QmlExport()
{
}

KisImportExportErrorCode QmlExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisImageSP image = kisImportExportSavingImage(document);
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    QmlConverter converter;
    return converter.buildFile(filename(), realFilename(), io, image);
}

void QmlExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "\x51ML");
}
