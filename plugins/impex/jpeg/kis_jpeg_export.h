/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_JPEG_EXPORT_H_
#define _KIS_JPEG_EXPORT_H_

#include <QVariant>

#include <KisImportExportFilter.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_filter_registry_model.h>


class KisJPEGExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    KisJPEGExport(QObject *parent, const QVariantList &);
    ~KisJPEGExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const QByteArray& from = "", const QByteArray& to = "") const override;
    void initializeCapabilities() override;
};

#endif
