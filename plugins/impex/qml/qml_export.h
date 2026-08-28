/*
 *  SPDX-FileCopyrightText: 2013 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _QML_EXPORT_H_
#define _QML_EXPORT_H_

#include <PkVariant.h>

#include <KisImportExportFilter.h>

class QmlExport : public KisImportExportFilter
{
public:
    QmlExport(PkObject *parent, const PkVariantList &);
    ~QmlExport() override;
public:
    KisImportExportErrorCode convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration = 0) override;
    void initializeCapabilities() override;
};

#endif
