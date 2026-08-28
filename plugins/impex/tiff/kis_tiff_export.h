/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_TIFF_EXPORT_H_
#define _KIS_TIFF_EXPORT_H_

#include <PkVariant.h>

#include <tiffio.h>

#include <KisImportExportFilter.h>

class KisTIFFExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisTIFFExport(QObject *parent, const PkVariantList &);
    ~KisTIFFExport() override;
    bool supportsIO() const override { return false; }
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = "", const PkByteArray& to = "") const override;
    void initializeCapabilities() override;

private:
    TIFFErrorHandler oldErrHandler = nullptr;
    TIFFErrorHandler oldWarnHandler = nullptr;
};

#endif
