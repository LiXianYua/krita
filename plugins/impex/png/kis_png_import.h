/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_PNG_IMPORT_H_
#define _KIS_PNG_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisPNGImport : public KisImportExportFilter
{
public:
    KisPNGImport(PkObject *parent, const PkVariantList &);
    ~KisPNGImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
