/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_TGA_EXPORT_H_
#define _KIS_TGA_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisTGAExport : public KisImportExportFilter
{
public:
    KisTGAExport(PkObject *parent, const PkVariantList &);
    ~KisTGAExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
};

#endif
