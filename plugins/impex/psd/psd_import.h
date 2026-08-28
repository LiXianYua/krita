/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PSD_IMPORT_H_
#define PSD_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class psdImport : public KisImportExportFilter {
    public:
        psdImport(PkObject *parent, const PkVariantList &);
        ~psdImport() override;
    public:
        KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
