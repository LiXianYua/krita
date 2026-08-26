/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_object_catcher.h"

#include <PkAuxTypes.h>
#include <KoColor.h>
#include <PkPoint.h>
#include <PkString.h>

#include <resources/KoAbstractGradient.h>

#include <kis_debug.h>

KisAslObjectCatcher::KisAslObjectCatcher()
    : m_arrayMode(false)
{
}

KisAslObjectCatcher::~KisAslObjectCatcher()
{
}

void KisAslObjectCatcher::addDouble(const PkString &path, double value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "double" << value;
}

void KisAslObjectCatcher::addInteger(const PkString &path, int value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "int" << value;
}

void KisAslObjectCatcher::addEnum(const PkString &path, const PkString &typeId, const PkString &value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "enum" << ppVar(typeId) << ppVar(value);
}

void KisAslObjectCatcher::addUnitFloat(const PkString &path, const PkString &unit, double value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "unitfloat" << ppVar(unit) << ppVar(value);
}

void KisAslObjectCatcher::addText(const PkString &path, const PkString &value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "text" << value;
}

void KisAslObjectCatcher::addBoolean(const PkString &path, bool value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "bool" << value;
}

void KisAslObjectCatcher::addColor(const PkString &path, const KoColor &value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "color" << value;
}

void KisAslObjectCatcher::addPoint(const PkString &path, const PkPointF &value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "point" << value;
}

void KisAslObjectCatcher::addCurve(const PkString &path, const PkString &name, const PkVector<PkPointF> &points)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "curve" << name << ppVar(points.size());
}

void KisAslObjectCatcher::addPattern(const PkString &path, const KoPatternSP value, const PkString &patternUuid)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "pattern" << value << " uuid " << patternUuid;
}

void KisAslObjectCatcher::addPatternRef(const PkString &path, const PkString &patternUuid, const PkString &patternName)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "pattern-ref" << ppVar(patternUuid) << ppVar(patternName);
}

void KisAslObjectCatcher::addGradient(const PkString &path, KoAbstractGradientSP value)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "gradient" << value;
}

void KisAslObjectCatcher::addRawData(const PkString &path, PkByteArray ba)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "bytearray";
}

void KisAslObjectCatcher::addTransform(const PkString &path, const PkTransform &transform)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "transform" << transform;
}

void KisAslObjectCatcher::addRect(const PkString &path, const PkRectF &rect)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "corner rect" << rect;
}

void KisAslObjectCatcher::addUnitRect(const PkString &path, const PkString &unit, const PkRectF &rect)
{
    dbgKrita << "Unhandled:" << (m_arrayMode ? "[A]" : "[ ]") << path << "unit rect" << unit << "rect" << rect;
}

void KisAslObjectCatcher::newStyleStarted()
{
    dbgKrita << "Unhandled:"
             << "new style started";
}

void KisAslObjectCatcher::setArrayMode(bool value)
{
    m_arrayMode = value;
}
