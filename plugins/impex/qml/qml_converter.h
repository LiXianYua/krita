/*
 *  SPDX-FileCopyrightText: 2013 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _QML_CONVERTER_H_
#define _QML_CONVERTER_H_

#include <PkStream.h>
#include <PkString.h>
#include <PkVector.h>

#include "kis_types.h"
#include <KisImportExportErrorCode.h>

class QMLConverter
{
public:
    QMLConverter() = default;
    ~QMLConverter() = default;

    KisImportExportErrorCode buildFile(const PkString &filename, const PkString &realFilename, PkStream *io, KisImageSP image);
};

#endif
