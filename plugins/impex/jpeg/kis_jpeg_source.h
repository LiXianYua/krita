/*
 *  SPDX-FileCopyrightText: 2009 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_JPEG_SOURCE_H_
#define _KIS_JPEG_SOURCE_H_

#include <stdio.h>

extern "C" {
  #include <jpeglib.h>
}

class PkStream;

namespace KisJPEGSource
{
    void setSource(j_decompress_ptr cinfo, PkStream* inputDevice);
}

#endif
