/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EXR_IMPORT_H_
#define EXR_IMPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class exrImport : public KisImportExportFilter
{
    Q_OBJECT
public:
    exrImport(QObject *parent, const PkVariantList &);
    ~exrImport() override;
    bool supportsIO() const override { return false; }
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif
