/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_BMP_IMPORT_H_
#define _KIS_BMP_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisQImageIOImport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisQImageIOImport(QObject *parent, const PkVariantList &);
    ~KisQImageIOImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
