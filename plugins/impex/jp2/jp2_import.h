/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef JP2_IMPORT_H_
#define JP2_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class jp2Import : public KisImportExportFilter
{
    Q_OBJECT
public:
    jp2Import(QObject *parent, const PkVariantList &);
    virtual ~jp2Import();
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
