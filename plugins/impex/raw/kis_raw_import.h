/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_RAW_IMPORT_H_
#define KIS_RAW_IMPORT_H_

#include <KisImportExportFilter.h>

class KisRawImport : public KisImportExportFilter
{
public:
    KisRawImport(PkObject *parent, const PkVariantList &);
    ~KisRawImport() override;

    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif // KIS_RAW_IMPORT_H_
