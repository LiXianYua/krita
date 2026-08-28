/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_WEBP_EXPORT_H_
#define _KIS_WEBP_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisWebPExport : public KisImportExportFilter
{
public:
    KisWebPExport(PkObject *parent, const PkVariantList &);
    ~KisWebPExport() override;

    KisImportExportErrorCode convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray &from, const PkByteArray &to) const override;
    void initializeCapabilities() override;
};

#endif
