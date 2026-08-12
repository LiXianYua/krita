/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_RAW_IMPORT_H_
#define KIS_RAW_IMPORT_H_

#include <KisImportExportFilter.h>

namespace KDcrawIface
{
class RawDecodingSettings;
} // namespace KDcrawIface

class KisRawImport : public KisImportExportFilter
{
    Q_OBJECT

public:
    KisRawImport(QObject *parent, const QVariantList &);
    ~KisRawImport() override;

    KisImportExportErrorCode convert(KisDocument *document, QIODevice *io,  KisPropertiesConfigurationSP configuration = 0) override;
};

#endif // KIS_RAW_IMPORT_H_

