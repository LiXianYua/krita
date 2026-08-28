/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_import.h"
#include "svg_import_policy.h"

#include <kpluginfactory.h>

K_PLUGIN_FACTORY_WITH_JSON(SVGImportFactory, "krita_svg_import.json", registerPlugin<KisSVGImport>();)

KisSVGImport::KisSVGImport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisSVGImport::~KisSVGImport()
{
}

KisImportExportErrorCode KisSVGImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)document;
    (void)io;
    (void)configuration;

    // The only available downstream shape parser still exposes Qt stream,
    // string, geometry, and container types outside this task's ownership.
    // Keep the 100 PPI policy seam for a future toolkit-free parser, but do not
    // claim an import succeeded without producing an image.
    (void)deterministicSvgImportPolicy();
    return ImportExportCodes::FormatFeaturesUnsupported;
}

#include <kis_svg_import.moc>
