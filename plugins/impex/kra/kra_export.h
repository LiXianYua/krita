/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KRA_EXPORT_H_
#define _KRA_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KraExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KraExport(PkObject *parent, const PkVariantList &);
    ~KraExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
    PkString verify(const PkString &fileName) const override;
    bool exportSupportsGuides() const override;
};

#endif
