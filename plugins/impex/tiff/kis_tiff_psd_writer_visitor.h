/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_TIFF_PSD_WRITER_VISITOR_H
#define _KIS_TIFF_PSD_WRITER_VISITOR_H

#include <tiffio.h>

#include <array>

#include <PkObject.h>
#include <KisImportExportErrorCode.h>
#include <kis_types.h>

#include "kis_tiff_base_writer.h"

struct KisTIFFOptions;

class KisTiffPsdWriter : public PkObject, protected KisTIFFBaseWriter
{
public:
    KisTiffPsdWriter(TIFF *image, KisTIFFOptions *options);
    ~KisTiffPsdWriter() override;

    KisImportExportErrorCode writeImage(KisGroupLayerSP rootLayer);
};

#endif // _KIS_TIFF_PSD_WRITER_VISITOR_H
