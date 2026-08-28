/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_Brush_IMPORT_H_
#define _KIS_Brush_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisBrushImport : public KisImportExportFilter
{
public:
    KisBrushImport(PkObject *parent, const PkVariantList &);
    ~KisBrushImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
