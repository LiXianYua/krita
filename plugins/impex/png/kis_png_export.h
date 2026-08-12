/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PNG_EXPORT_H_
#define _KIS_PNG_EXPORT_H_

#include <KisImportExportFilter.h>

class KisPNGExport : public KisImportExportFilter
{
    Q_OBJECT

public:

    KisPNGExport(QObject *parent, const QVariantList &);
    ~KisPNGExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP configuration = 0) override;

    KisPropertiesConfigurationSP defaultConfiguration(const QByteArray& from = "", const QByteArray& to = "") const override;
    void initializeCapabilities() override;
};

#endif
