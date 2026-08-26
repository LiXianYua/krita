/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "kritapsdutils_export.h"

#include <PkAuxTypes.h>
#include <psd.h>

class KRITAPSDUTILS_EXPORT Compression
{
public:
    static PkByteArray uncompress(int unpacked_len, PkByteArray bytes, psd_compression_type compressionType, int row_size = 0, int color_depth = 0);
    static PkByteArray compress(PkByteArray bytes, psd_compression_type compressionType, int row_size = 0, int color_depth = 0);
};

#endif // PSD_COMPRESSION_H
