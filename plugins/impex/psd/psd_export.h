/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _PSD_EXPORT_H_
#define _PSD_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class psdExport : public KisImportExportFilter {
    public:
        psdExport(PkObject *parent, const PkVariantList &);
        ~psdExport() override;
    public:
        KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
        void initializeCapabilities() override;
        bool exportSupportsGuides() const override;
};

#endif
