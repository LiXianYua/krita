/*
 * SPDX-FileCopyrightText: 2021 Halla Rempt <halla@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KRZ_EXPORT_H_
#define _KRZ_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KrzExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KrzExport(PkObject *parent, const PkVariantList &);
    ~KrzExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
    PkString verify(const PkString &fileName) const override;
};

#endif
