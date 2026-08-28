/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _ORA_EXPORT_H_
#define _ORA_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class OraExport : public KisImportExportFilter
{
public:
    OraExport(PkObject *parent, const PkVariantList &);
    ~OraExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
    PkString verify(const PkString &fileName) const override;
};

#endif
