/*
 * SPDX-FileCopyrightText: 2023 Rasyuqa A. H. <qampidh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RGBE_EXPORT_H
#define RGBE_EXPORT_H

#include <KisImportExportFilter.h>

class RGBEExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    RGBEExport(QObject *parent, const PkVariantList &);
    ~RGBEExport() override = default;

    KisImportExportErrorCode
    convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP cfg = nullptr) override;
    KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray &from = PkByteArray(),
                                                      const PkByteArray &to = PkByteArray()) const override;
    void initializeCapabilities() override;
};

#endif // RGBE_EXPORT_H
