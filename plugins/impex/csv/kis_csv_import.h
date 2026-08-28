/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_CSV_IMPORT_H_
#define _KIS_CSV_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisCSVImport : public KisImportExportFilter
{
public:
    KisCSVImport(PkObject *parent, const PkVariantList &);
    ~KisCSVImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
