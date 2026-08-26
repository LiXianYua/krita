/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_WRITER_H
#define __KIS_ASL_WRITER_H

#include "kritapsdutils_export.h"
#include "psd.h"

class PkXmlDocument;
class PkStream;
class PkTransform;
class PkRectF;

class KRITAPSDUTILS_EXPORT KisAslWriter
{
public:
    KisAslWriter(psd_byte_order byteOrder = psd_byte_order::psdBigEndian);

    void writeFile(PkStream &device, const PkXmlDocument &doc);
    void writeFillLayerSectionEx(PkStream &device, const PkXmlDocument &doc);
    void writePsdLfx2SectionEx(PkStream &device, const PkXmlDocument &doc);
    void writeTypeToolObjectSettings(PkStream &device, const PkXmlDocument &doc, const PkXmlDocument &warpDoc, const PkTransform tf, const PkRectF bounds);
    void writeVectorStrokeDataEx(PkStream &device, const PkXmlDocument &doc);
    void writeVectorOriginationDataEx(PkStream &device, const PkXmlDocument &doc);

private:
    psd_byte_order m_byteOrder;
};

#endif /* __KIS_ASL_WRITER_H */
