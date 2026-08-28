/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_GIF_IMPORT_H_
#define _KIS_GIF_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisGIFImport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisGIFImport(QObject *parent, const PkVariantList &);
    ~KisGIFImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
