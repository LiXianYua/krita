/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_Brush_EXPORT_H_
#define _KIS_Brush_EXPORT_H_

#include <PkVariant.h>
#include <KisImportExportFilter.h>
#include <kis_properties_configuration.h>


class KisBrushExport : public KisImportExportFilter
{
public:
    KisBrushExport(PkObject *parent, const PkVariantList &);
    ~KisBrushExport() override;
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = "", const PkByteArray& to = "") const override;

    void initializeCapabilities() override;
};

#endif
