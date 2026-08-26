/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_READER_H
#define __KIS_ASL_READER_H

#include "kritapsdutils_export.h"
#include "psd.h"

class PkXmlDocument;
class PkStream;
class PkTransform;

class KRITAPSDUTILS_EXPORT KisAslReader
{
public:
    PkXmlDocument readFile(PkStream &device);

    static PkXmlDocument readLfx2PsdSection(PkStream &device, psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
    static PkXmlDocument readFillLayer(PkStream &device, psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
    static PkXmlDocument readTypeToolObjectSettings(PkStream &device, PkTransform &transform, psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
    static PkXmlDocument readVectorStroke(PkStream &device, psd_byte_order byteOrder = psd_byte_order::psdBigEndian); 
    static PkXmlDocument readVectorOriginationData(PkStream &device, psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
    static PkXmlDocument readPsdSectionPattern(PkStream &device, qint64 bytesLeft, psd_byte_order byteOrder = psd_byte_order::psdBigEndian);
};

#endif /* __KIS_ASL_READER_H */
