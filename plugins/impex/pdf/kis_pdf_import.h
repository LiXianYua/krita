/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PDF_IMPORT_H
#define KIS_PDF_IMPORT_H

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisPDFImport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisPDFImport(QObject *parent, const PkVariantList &);
    ~KisPDFImport() override;

public:
    KisImportExportErrorCode
    convert(KisDocument *document,
            PkStream *io,
            KisPropertiesConfigurationSP configuration = nullptr) override;
};

#endif
