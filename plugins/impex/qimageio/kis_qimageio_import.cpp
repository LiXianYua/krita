/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimageio_import.h"

#include <kpluginfactory.h>
#include <KisDocument.h>


K_PLUGIN_FACTORY_WITH_JSON(KisQImageIOImportFactory, "krita_qimageio_import.json", registerPlugin<KisQImageIOImport>();)

KisQImageIOImport::KisQImageIOImport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisQImageIOImport::~KisQImageIOImport()
{
}

KisImportExportErrorCode KisQImageIOImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP /*configuration*/)
{
    (void)io;
    document->setErrorMessage(PkString("Generic QImageIO formats have no native headless codec"));
    return ImportExportCodes::FormatFeaturesUnsupported;

}

#include "kis_qimageio_import.moc"
