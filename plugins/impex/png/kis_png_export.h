/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PNG_EXPORT_H_
#define _KIS_PNG_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisPNGExport : public KisImportExportFilter
{
    Q_OBJECT

public:

    KisPNGExport(PkObject *parent, const PkVariantList &);
    ~KisPNGExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;

    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = PkByteArray(),
                                                       const PkByteArray& to = PkByteArray()) const override;
    void initializeCapabilities() override;
};

#endif
