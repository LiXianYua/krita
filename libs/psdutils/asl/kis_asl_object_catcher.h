/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_OBJECT_CATCHER_H
#define __KIS_ASL_OBJECT_CATCHER_H

#include <PkVector.h>

#include <KoPattern.h>

class PkString;
class KoColor;
class PkPointF;
class KoAbstractGradient;
class PkByteArray;
class PkTransform;
class PkRectF;

#include "kritapsdutils_export.h"

template<class T>
class PkSharedPointer;
typedef PkSharedPointer<KoAbstractGradient> KoAbstractGradientSP;

class KRITAPSDUTILS_EXPORT KisAslObjectCatcher
{
public:
    KisAslObjectCatcher();
    virtual ~KisAslObjectCatcher();

    virtual void addDouble(const PkString &path, double value);
    virtual void addInteger(const PkString &path, int value);
    virtual void addEnum(const PkString &path, const PkString &typeId, const PkString &value);
    virtual void addUnitFloat(const PkString &path, const PkString &unit, double value);
    virtual void addText(const PkString &path, const PkString &value);
    virtual void addBoolean(const PkString &path, bool value);
    virtual void addColor(const PkString &path, const KoColor &value);
    virtual void addPoint(const PkString &path, const PkPointF &value);
    virtual void addCurve(const PkString &path, const PkString &name, const PkVector<PkPointF> &points);
    virtual void addPattern(const PkString &path, const KoPatternSP pattern, const PkString &patternUuid);
    virtual void addPatternRef(const PkString &path, const PkString &patternUuid, const PkString &patternName);
    virtual void addGradient(const PkString &path, KoAbstractGradientSP gradient);
    virtual void addRawData(const PkString &path, PkByteArray ba);
    virtual void addTransform(const PkString &path, const PkTransform &transform);
    virtual void addRect(const PkString &path, const PkRectF &rect);
    virtual void addUnitRect(const PkString &path, const PkString &unit, const PkRectF &rect);

    virtual void newStyleStarted();

    void setArrayMode(bool value);

protected:
    bool m_arrayMode;
};

#endif /* __KIS_ASL_OBJECT_CATCHER_H */
