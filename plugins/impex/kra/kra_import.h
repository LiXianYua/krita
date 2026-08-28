/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KRA_IMPORT_H_
#define KRA_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KraImport : public KisImportExportFilter
{
public:
    KraImport(PkObject *parent, const PkVariantList &);
    ~KraImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
