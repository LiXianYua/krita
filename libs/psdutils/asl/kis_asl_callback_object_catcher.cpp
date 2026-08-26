/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_callback_object_catcher.h"

#include <PkHash.h>

#include <PkColor.h>
#include <PkPoint.h>
#include <PkString.h>

#include <KoColor.h>

#include "kis_debug.h"

typedef PkHash<PkString, ASLCallbackDouble> MapHashDouble;
typedef PkHash<PkString, ASLCallbackInteger> MapHashInt;

struct EnumMapping {
    EnumMapping(const PkString &_typeId, ASLCallbackString _map)
        : typeId(_typeId)
        , map(_map)
    {
    }

    PkString typeId;
    ASLCallbackString map;
};

typedef PkHash<PkString, EnumMapping> MapHashEnum;

struct UnitFloatMapping {
    UnitFloatMapping() {

    }
    UnitFloatMapping(const PkString &_unit, ASLCallbackDouble _map)
    {
        unitMap.insert(_unit, _map);
    }

    PkMap<PkString, ASLCallbackDouble> unitMap;
};

struct UnitRectMapping {
    UnitRectMapping(const PkString &_unit, ASLCallbackRect _map)
        : unit(_unit)
        , map(_map)
    {
    }

    PkString unit;
    ASLCallbackRect map;
};

typedef PkHash<PkString, UnitFloatMapping> MapHashUnitFloat;
typedef PkHash<PkString, UnitRectMapping> MapHashUnitRect;

typedef PkHash<PkString, ASLCallbackString> MapHashText;
typedef PkHash<PkString, ASLCallbackBoolean> MapHashBoolean;
typedef PkHash<PkString, ASLCallbackColor> MapHashColor;
typedef PkHash<PkString, ASLCallbackPoint> MapHashPoint;
typedef PkHash<PkString, ASLCallbackCurve> MapHashCurve;
typedef PkHash<PkString, ASLCallbackPattern> MapHashPattern;
typedef PkHash<PkString, ASLCallbackPatternRef> MapHashPatternRef;
typedef PkHash<PkString, ASLCallbackGradient> MapHashGradient;
typedef PkHash<PkString, ASLCallbackRawData> MapHashRawData;
typedef PkHash<PkString, ASLCallbackTransform> MapHashTransform;
typedef PkHash<PkString, ASLCallbackRect> MapHashRect;

struct KisAslCallbackObjectCatcher::Private {
    MapHashDouble mapDouble;
    MapHashInt mapInteger;
    MapHashEnum mapEnum;
    MapHashUnitFloat mapUnitFloat;
    MapHashText mapText;
    MapHashBoolean mapBoolean;
    MapHashColor mapColor;
    MapHashPoint mapPoint;
    MapHashCurve mapCurve;
    MapHashPattern mapPattern;
    MapHashPatternRef mapPatternRef;
    MapHashGradient mapGradient;
    MapHashRawData mapRawData;
    MapHashTransform mapTransform;
    MapHashRect mapRect;
    MapHashUnitRect mapUnitRect;

    ASLCallbackNewStyle newStyleCallback;
};

KisAslCallbackObjectCatcher::KisAslCallbackObjectCatcher()
    : m_d(new Private)
{
}

KisAslCallbackObjectCatcher::~KisAslCallbackObjectCatcher()
{
}

template<class HashType, typename T>
inline void passToCallback(const PkString &path, const HashType &hash, const T &value)
{
    typename HashType::const_iterator it = hash.constFind(path);
    if (it != hash.constEnd()) {
        (*it)(value);
    } else {
        warnKrita << "Unhandled:" << path << typeid(hash).name() << value;
    }
}

template<class HashType, typename T1, typename T2>
inline void passToCallback(const PkString &path, const HashType &hash, const T1 &value1, const T2 &value2)
{
    typename HashType::const_iterator it = hash.constFind(path);
    if (it != hash.constEnd()) {
        (*it)(value1, value2);
    } else {
        warnKrita << "Unhandled:" << path << typeid(hash).name() << value1 << value2;
    }
}

void KisAslCallbackObjectCatcher::addDouble(const PkString &path, double value)
{
    passToCallback(path, m_d->mapDouble, value);
}

void KisAslCallbackObjectCatcher::addInteger(const PkString &path, int value)
{
    passToCallback(path, m_d->mapInteger, value);
}

void KisAslCallbackObjectCatcher::addEnum(const PkString &path, const PkString &typeId, const PkString &value)
{
    MapHashEnum::const_iterator it = m_d->mapEnum.constFind(path);
    if (it != m_d->mapEnum.constEnd()) {
        if (it->typeId == typeId) {
            it->map(value);
        } else {
            warnKrita << "KisAslCallbackObjectCatcher::addEnum: inconsistent typeId" << ppVar(typeId) << ppVar(it->typeId);
        }
    }
}

void KisAslCallbackObjectCatcher::addUnitFloat(const PkString &path, const PkString &unit, double value)
{
    MapHashUnitFloat::const_iterator it = m_d->mapUnitFloat.constFind(path);
    if (it != m_d->mapUnitFloat.constEnd()) {
        if (it->unitMap.contains(unit)) {
            ASLCallbackDouble map = it->unitMap.value(unit);
            map(value);
        } else {
            warnKrita << "KisAslCallbackObjectCatcher::addUnitFloat: inconsistent unit" << ppVar(unit) << ppVar(it->unitMap.keys());
        }
    }
}

void KisAslCallbackObjectCatcher::addText(const PkString &path, const PkString &value)
{
    passToCallback(path, m_d->mapText, value);
}

void KisAslCallbackObjectCatcher::addBoolean(const PkString &path, bool value)
{
    passToCallback(path, m_d->mapBoolean, value);
}

void KisAslCallbackObjectCatcher::addColor(const PkString &path, const KoColor &value)
{
    passToCallback(path, m_d->mapColor, value);
}

void KisAslCallbackObjectCatcher::addPoint(const PkString &path, const PkPointF &value)
{
    passToCallback(path, m_d->mapPoint, value);
}

void KisAslCallbackObjectCatcher::addCurve(const PkString &path, const PkString &name, const PkVector<PkPointF> &points)
{
    MapHashCurve::const_iterator it = m_d->mapCurve.constFind(path);
    if (it != m_d->mapCurve.constEnd()) {
        (*it)(name, points);
    }
}

void KisAslCallbackObjectCatcher::addPattern(const PkString &path, const KoPatternSP value, const PkString &patternUuid)
{
    passToCallback(path, m_d->mapPattern, value, patternUuid);
}

void KisAslCallbackObjectCatcher::addPatternRef(const PkString &path, const PkString &patternUuid, const PkString &patternName)
{
    MapHashPatternRef::const_iterator it = m_d->mapPatternRef.constFind(path);
    if (it != m_d->mapPatternRef.constEnd()) {
        (*it)(patternUuid, patternName);
    }
}

void KisAslCallbackObjectCatcher::addGradient(const PkString &path, KoAbstractGradientSP value)
{
    passToCallback(path, m_d->mapGradient, value);
}

void KisAslCallbackObjectCatcher::newStyleStarted()
{
    if (m_d->newStyleCallback) {
        m_d->newStyleCallback();
    }
}

void KisAslCallbackObjectCatcher::addRawData(const PkString &path, PkByteArray ba)
{
    passToCallback(path, m_d->mapRawData, ba);
}

void KisAslCallbackObjectCatcher::addTransform(const PkString &path, const PkTransform &transform)
{
    passToCallback(path, m_d->mapTransform, transform);
}

void KisAslCallbackObjectCatcher::addRect(const PkString &path, const PkRectF &rect)
{
    passToCallback(path, m_d->mapRect, rect);
}

void KisAslCallbackObjectCatcher::addUnitRect(const PkString &path, const PkString &unit, const PkRectF &rect)
{
    MapHashUnitRect::const_iterator it = m_d->mapUnitRect.constFind(path);
    if (it != m_d->mapUnitRect.constEnd()) {
        if (it->unit == unit) {
            it->map(rect);
        } else {
            warnKrita << "KisAslCallbackObjectCatcher::addUnitRect: inconsistent unit" << ppVar(unit) << ppVar(it->unit);
        }
    }
}

/*****************************************************************/
/*      Subscription methods                                      */
/*****************************************************************/

void KisAslCallbackObjectCatcher::subscribeDouble(const PkString &path, ASLCallbackDouble callback)
{
    m_d->mapDouble.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeInteger(const PkString &path, ASLCallbackInteger callback)
{
    m_d->mapInteger.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeEnum(const PkString &path, const PkString &typeId, ASLCallbackString callback)
{
    m_d->mapEnum.insert(path, EnumMapping(typeId, callback));
}

void KisAslCallbackObjectCatcher::subscribeUnitFloat(const PkString &path, const PkString &unit, ASLCallbackDouble callback)
{
    if (m_d->mapUnitFloat.contains(path)) {
        UnitFloatMapping mapping = m_d->mapUnitFloat.value(path);
        mapping.unitMap.insert(unit, callback);
        m_d->mapUnitFloat.insert(path, mapping);
    } else {
        m_d->mapUnitFloat.insert(path, UnitFloatMapping(unit, callback));
    }
}

void KisAslCallbackObjectCatcher::subscribeText(const PkString &path, ASLCallbackString callback)
{
    m_d->mapText.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeBoolean(const PkString &path, ASLCallbackBoolean callback)
{
    m_d->mapBoolean.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeColor(const PkString &path, ASLCallbackColor callback)
{
    m_d->mapColor.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribePoint(const PkString &path, ASLCallbackPoint callback)
{
    m_d->mapPoint.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeCurve(const PkString &path, ASLCallbackCurve callback)
{
    m_d->mapCurve.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribePattern(const PkString &path, ASLCallbackPattern callback)
{
    m_d->mapPattern.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribePatternRef(const PkString &path, ASLCallbackPatternRef callback)
{
    m_d->mapPatternRef.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeGradient(const PkString &path, ASLCallbackGradient callback)
{
    m_d->mapGradient.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeNewStyleStarted(ASLCallbackNewStyle callback)
{
    m_d->newStyleCallback = callback;
}

void KisAslCallbackObjectCatcher::subscribeRawData(const PkString &path, ASLCallbackRawData callback)
{
    m_d->mapRawData.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeTransform(const PkString &path, ASLCallbackTransform callback)
{
    m_d->mapTransform.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeRect(const PkString &path, ASLCallbackRect callback)
{
    m_d->mapRect.insert(path, callback);
}

void KisAslCallbackObjectCatcher::subscribeUnitRect(const PkString &path, const PkString &unit, ASLCallbackRect callback)
{
    m_d->mapUnitRect.insert(path, UnitRectMapping(unit, callback));
}
