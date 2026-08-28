/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_JPEG_IMPORT_H_
#define _KIS_JPEG_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisJPEGImport : public KisImportExportFilter
{
public:
    KisJPEGImport(PkObject *parent, const PkVariantList &);
    ~KisJPEGImport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
