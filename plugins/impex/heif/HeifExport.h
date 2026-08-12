/*
 *  SPDX-FileCopyrightText: 2018 Dirk Farin <farin@struktur.de>
 *  SPDX-FileCopyrightText: 2020-2021 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *  SPDX-FileCopyrightText: 2021 Daniel Novomesky <dnovomesky@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEIF_EXPORT_H_
#define HEIF_EXPORT_H_

#include <QVariant>

#include <KisImportExportFilter.h>

#include "kis_heif_export_tools.h"

class HeifExport : public KisImportExportFilter
{
    Q_OBJECT
public:
    HeifExport(QObject *parent, const QVariantList &);
    ~HeifExport() override;

    // This should return true if the library can work with a QIODevice, and doesn't want to open the file by itself
    bool supportsIO() const override { return true; }

    KisImportExportErrorCode convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP configuration = 0) override;
    KisPropertiesConfigurationSP defaultConfiguration(const QByteArray& from = "", const QByteArray& to = "") const override;
    void initializeCapabilities() override;
};

#endif
