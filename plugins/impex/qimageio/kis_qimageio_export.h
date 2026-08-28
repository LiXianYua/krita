/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_BMP_EXPORT_H_
#define _KIS_BMP_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisQImageIOExport : public KisImportExportFilter
{
public:
    KisQImageIOExport(PkObject *parent, const PkVariantList &);
    ~KisQImageIOExport() override;

    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray &, const PkByteArray &) const override;
    void initializeCapabilities() override;
};

#endif
