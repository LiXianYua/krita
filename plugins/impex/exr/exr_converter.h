/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _EXR_CONVERTER_H_
#define _EXR_CONVERTER_H_

#include <stdio.h>
#include <memory>

#include "kis_types.h"
#include <KisImportExportErrorCode.h>

class KisDocument;

class EXRConverter
{
public:
    EXRConverter(KisDocument *doc, bool showNotifications);
    ~EXRConverter();
public:
    KisImportExportErrorCode buildImage(const PkString &filename);
    KisImportExportErrorCode buildFile(const PkString &filename, KisPaintLayerSP layer);
    KisImportExportErrorCode buildFile(const PkString &filename, KisGroupLayerSP layer, bool flatten=false);
    /**
     * Retrieve the constructed image
     */
    KisImageSP image();
    PkString errorMessage() const;
private:
    KisImportExportErrorCode decode(const PkString &filename);

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

#endif
