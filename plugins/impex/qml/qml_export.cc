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
        []() -> KisImportExportFilter * { return new QMLExport(nullptr, PkVariantList()); });
}

QMLExport::QMLExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

QMLExport::~QMLExport()
{
}

KisImportExportErrorCode QMLExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    KisImageSP image = kisImportExportSavingImage(document);
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    QMLConverter converter;
    return converter.buildFile(filename(), realFilename(), io, image);
}

void QMLExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("MultiLayerCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("LayerOpacityCheck")->create(KisExportCheckBase::SUPPORTED));

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "QML");
}
