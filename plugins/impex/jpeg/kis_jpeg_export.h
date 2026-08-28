/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_JPEG_EXPORT_H_
#define _KIS_JPEG_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_filter_registry_model.h>


class KisJPEGExport : public KisImportExportFilter
{
public:
    KisJPEGExport(PkObject *parent, const PkVariantList &);
    ~KisJPEGExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = PkByteArray(), const PkByteArray& to = PkByteArray()) const override;
    void initializeCapabilities() override;
};

#endif
