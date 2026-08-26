/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_XML_WRITER_H
#define __KIS_ASL_XML_WRITER_H

#include <PkPointer.h>
#include <PkScopedPointer.h>
#include <PkVector.h>

#include <KoPattern.h>
#include <KoSegmentGradient.h>

#include "kritapsdutils_export.h"

class PkString;
class PkColor;
class PkPointF;
class PkXmlDocument;
class PkByteArray;
class PkPolygonF;
class PkTransform;
class PkRectF;

class KoStopGradient;
class KoSegmentGradient;

class KRITAPSDUTILS_EXPORT KisAslXmlWriter
{
public:
    KisAslXmlWriter();
    ~KisAslXmlWriter();

    PkXmlDocument document() const;

    void enterDescriptor(const PkString &key, const PkString &name, const PkString &classId);
    void leaveDescriptor();

    void enterList(const PkString &key);
    void leaveList();

    void writeDouble(const PkString &key, double value);
    void writeInteger(const PkString &key, int value);
    void writeEnum(const PkString &key, const PkString &typeId, const PkString &value);
    void writeUnitFloat(const PkString &key, const PkString &unit, double value);
    void writeText(const PkString &key, const PkString &value);
    void writeBoolean(const PkString &key, bool value);
    void writeColor(const PkString &key, const KoColor &value);
    void writePoint(const PkString &key, const PkPointF &value);
    void writePhasePoint(const PkString &key, const PkPointF &value);
    void writeOffsetPoint(const PkString &key, const PkPointF &value);
    void writeCurve(const PkString &key, const PkString &name, const PkVector<PkPointF> &points);
    PkString writePattern(const PkString &key, const KoPatternSP pattern);
    void writePatternRef(const PkString &key, const KoPatternSP pattern, const PkString &uuid);
    void writeSegmentGradient(const PkString &key, const KoSegmentGradient &gradient);
    void writeStopGradient(const PkString &key, const KoStopGradient &gradient);
    void writeRawData(const PkString key, const PkByteArray *rawData);
    void writeTransform(const PkString &key, const PkTransform &transform);
    void writeUnitRect(const PkString &key, const PkString &unit, const PkRectF &rect);
    void writeFloatRect(const PkString &key, const PkRectF &rect);
    void writePointRect(const PkString &key, const PkPolygonF &transformedRect);

private:
    PkString getSegmentEndpointTypeString(KoGradientSegmentEndpointType segtype);
    void writeGradientImpl(const PkString &key,
                           const PkString &name,
                           PkVector<KoColor> colors,
                           PkVector<qreal> transparencies,
                           PkVector<qreal> positions,
                           PkVector<PkString> types,
                           PkVector<qreal> middleOffsets);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_ASL_XML_WRITER_H */
