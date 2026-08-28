/*
 *  SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KIS_HeightMap_EXPORT_H_
#define _KIS_HeightMap_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class KisHeightMapExport : public KisImportExportFilter
{
public:
    KisHeightMapExport(PkObject *parent, const PkVariantList &);
    ~KisHeightMapExport() override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = PkByteArray(), const PkByteArray& to = PkByteArray()) const override;
    void initializeCapabilities() override;
    
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
