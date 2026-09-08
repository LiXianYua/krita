/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_grid_config.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>
#include <PkXmlNodeList.h>
#include <cmath>
#include <cstdio>
#include <type_traits>

#include "kis_algebra_2d.h"

template <typename T>
PkString scalarToString(T value)
{
    if constexpr (std::is_same<T, double>::value || std::is_same<T, qreal>::value) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.15g", value);
        return PkString(buffer);
    } else if constexpr (std::is_enum<T>::value) {
        return PkString::number(static_cast<int>(value));
    } else {
        return PkString::number(value);
    }
}

template <typename T>
void savePkValue(PkXmlElement *parent, const PkString &tag, T value)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute("type", "value");
    element.setAttribute("value", scalarToString(value));
}

void savePkValue(PkXmlElement *parent, const PkString &tag, const PkPoint &point)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute("type", "point");
    element.setAttribute("x", scalarToString(point.x()));
    element.setAttribute("y", scalarToString(point.y()));
}

void savePkValue(PkXmlElement *parent, const PkString &tag, const PkColor &color)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute("type", "qcolor");
    element.setAttribute("value", color.name(PkColor::HexArgb));
}

bool findOnlyPkElement(const PkXmlElement &parent, const PkString &tag, PkXmlElement *element)
{
    const PkXmlNodeList list = parent.elementsByTagName(tag);
    if (list.size() != 1 || !list.at(0).isElement()) {
        return false;
    }
    *element = list.at(0).toElement();
    return true;
}

bool hasPkType(const PkXmlElement &element, const PkString &type)
{
    return element.attribute("type", "unknown-type") == type;
}

int pkToInt(PkString text, bool *ok = nullptr)
{
    bool okLocale = false;
    int value = text.toInt(&okLocale);

    if (!okLocale) {
        text.replace(u',', u'.');
        value = static_cast<int>(text.toDouble(&okLocale));
    }

    if (!okLocale && ok == nullptr) {
        value = 0;
    }

    if (ok != nullptr) {
        *ok = okLocale;
    }

    return value;
}

template <typename T>
bool loadPkValue(const PkXmlElement &element, T *value)
{
    if (!hasPkType(element, "value")) return false;
    bool ok = false;
    const int loaded = pkToInt(element.attribute("value", "no-value"), &ok);
    if (!ok) return false;
    *value = static_cast<T>(loaded);
    return true;
}

bool loadPkValue(const PkXmlElement &element, double *value)
{
    if (!hasPkType(element, "value")) return false;
    PkString text = element.attribute("value", "0");
    bool ok = false;
    *value = text.toDouble(&ok);
    if (!ok) {
        text.replace(u',', u'.');
        *value = text.toDouble(&ok);
    }
    if (!ok) {
        *value = 0.0;
    }
    return true;
}

bool loadPkValue(const PkXmlElement &element, PkPoint *point)
{
    if (!hasPkType(element, "point")) return false;
    point->setX(pkToInt(element.attribute("x", "0")));
    point->setY(pkToInt(element.attribute("y", "0")));
    return true;
}

bool loadPkValue(const PkXmlElement &element, PkColor *color)
{
    if (!hasPkType(element, "qcolor")) return false;
    color->setNamedColor(element.attribute("value", "#FFFF0000"));
    return true;
}

template <typename T>
bool loadPkValue(const PkXmlElement &parent, const PkString &tag, T *value)
{
    PkXmlElement element;
    return findOnlyPkElement(parent, tag, &element) && loadPkValue(element, value);
}


const KisGridConfig& KisGridConfig::defaultGrid()
{
    static KisGridConfig object;
    object.loadStaticData();
    return object;
}

void KisGridConfig::transform(const PkTransform &transform)
{
    if (transform.type() >= PkTransform::TxShear) return;

    KisAlgebra2D::DecomposedMatrix m(transform);

    if (m_gridType == GRID_RECTANGULAR) {
        PkTransform t = m.scaleTransform();

        const qreal eps = 1e-3;
        const qreal wrappedRotation = KisAlgebra2D::wrapValue(m.angle, 90.0);
        if (wrappedRotation <= eps || wrappedRotation >= 90.0 - eps) {
            t *= m.rotateTransform();
        }

        m_spacing = KisAlgebra2D::abs(t.map(m_spacing));
        // Transform map may round spacing down to 0, but it must be at least 1
        m_spacing.setX(pkMax(1, m_spacing.x()));
        m_spacing.setY(pkMax(1, m_spacing.y()));

    } else if (m_gridType == GRID_ISOMETRIC_LEGACY) {
        if (pkQtFuzzyCompare(m.scaleX, m.scaleY)) {
            m_cellSpacing = pkRound(pkAbs(m_cellSpacing * m.scaleX));
        }
    }
    m_offset = KisAlgebra2D::wrapValue(transform.map(m_offset), m_spacing);
}

void KisGridConfig::loadStaticData()
{
    const PkConfigGroup cfg = PkSharedConfig::openConfig()->group(PkString());

    m_lineTypeMain = LineTypeInternal(pkBound(0, cfg.readEntry("gridmainstyle", 0), 2));
    m_lineTypeSubdivision = LineTypeInternal(pkMin(cfg.readEntry("gridsubdivisionstyle", 1), 2));
    m_lineTypeIsoVertical = LineTypeInternal(pkBound(0, cfg.readEntry("gridisoverticalstyle", 0), 3));

    m_colorMain = cfg.readEntry("gridmaincolor", PkColor(99, 99, 99));
    m_colorSubdivision = cfg.readEntry("gridsubdivisioncolor", PkColor(150, 150, 150));
    m_colorIsoVertical = cfg.readEntry("gridisoverticalcolor", PkColor(150, 150, 150));

    m_spacing = cfg.readEntry("defaultGridSpacing", PkPoint(16, 16));
}

void KisGridConfig::saveStaticData() const
{
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(PkString());
    cfg.writeEntry("gridmainstyle", quint32(m_lineTypeMain));
    cfg.writeEntry("gridsubdivisionstyle", quint32(m_lineTypeSubdivision));
    cfg.writeEntry("gridisoverticalstyle", quint32(m_lineTypeIsoVertical));
    cfg.writeEntry("gridmaincolor", m_colorMain);
    cfg.writeEntry("gridsubdivisioncolor", m_colorSubdivision);
    cfg.writeEntry("gridisoverticalcolor", m_colorIsoVertical);
    cfg.sync();
}

PkXmlElement KisGridConfig::saveDynamicDataToXml(PkXmlDocument& doc, const PkString &tag) const
{
    PkXmlElement gridElement = doc.createElement(tag);
    savePkValue(&gridElement, "showGrid", m_showGrid);
    savePkValue(&gridElement, "snapToGrid", m_snapToGrid);
    savePkValue(&gridElement, "offsetActive", m_offsetActive);
    savePkValue(&gridElement, "offset", m_offset);
    savePkValue(&gridElement, "spacing", m_spacing);
    savePkValue(&gridElement, "xSpacingActive", m_xSpacingActive);
    savePkValue(&gridElement, "ySpacingActive", m_ySpacingActive);
    savePkValue(&gridElement, "offsetAspectLocked", m_offsetAspectLocked);
    savePkValue(&gridElement, "spacingAspectLocked", m_spacingAspectLocked);
    savePkValue(&gridElement, "subdivision", m_subdivision);
    savePkValue(&gridElement, "angleLeft", m_angleLeft);
    savePkValue(&gridElement, "angleRight", m_angleRight);
    savePkValue(&gridElement, "angleLeftActive", m_angleLeftActive);
    savePkValue(&gridElement, "angleRightActive", m_angleRightActive);
    savePkValue(&gridElement, "angleAspectLocked", m_angleAspectLocked);
    savePkValue(&gridElement, "cellSpacing", m_cellSpacing);
    savePkValue(&gridElement, "cellSize", m_cellSize);
    savePkValue(&gridElement, "gridType", m_gridType);

    savePkValue(&gridElement, "colorMain", m_colorMain);
    savePkValue(&gridElement, "colorSubdivision", m_colorSubdivision);
    savePkValue(&gridElement, "colorVertical", m_colorIsoVertical);
    savePkValue(&gridElement, "lineTypeMain", m_lineTypeMain);
    savePkValue(&gridElement, "lineTypeSubdivision", m_lineTypeSubdivision);
    savePkValue(&gridElement, "lineTypeVertical", m_lineTypeIsoVertical);

    return gridElement;
}

bool KisGridConfig::loadDynamicDataFromXml(const PkXmlElement &gridElement)
{
    const PkConfigGroup cfg = PkSharedConfig::openConfig()->group(PkString());
    bool result = true;

    result &= loadPkValue(gridElement, "showGrid", &m_showGrid);
    result &= loadPkValue(gridElement, "snapToGrid", &m_snapToGrid);
    result &= loadPkValue(gridElement, "offset", &m_offset);
    result &= loadPkValue(gridElement, "spacing", &m_spacing);
    result &= loadPkValue(gridElement, "offsetAspectLocked", &m_offsetAspectLocked);
    result &= loadPkValue(gridElement, "spacingAspectLocked", &m_spacingAspectLocked);
    result &= loadPkValue(gridElement, "subdivision", &m_subdivision);
    result &= loadPkValue(gridElement, "angleLeft", &m_angleLeft);
    result &= loadPkValue(gridElement, "angleRight", &m_angleRight);
    result &= loadPkValue(gridElement, "cellSpacing", &m_cellSpacing);
    result &= loadPkValue(gridElement, "gridType", (int*)(&m_gridType));

    // following variables may not be present in older files; do not update result variable
    loadPkValue(gridElement, "offsetActive", &m_offsetActive);
    loadPkValue(gridElement, "xSpacingActive", &m_xSpacingActive);
    loadPkValue(gridElement, "ySpacingActive", &m_ySpacingActive);
    loadPkValue(gridElement, "angleLeftActive", &m_angleLeftActive);
    loadPkValue(gridElement, "angleRightActive", &m_angleRightActive);
    loadPkValue(gridElement, "angleAspectLocked", &m_angleAspectLocked);
    loadPkValue(gridElement, "cellSize", &m_cellSize);

    int lineTypeMain = pkBound(0, cfg.readEntry("gridmainstyle", 0), 2);
    loadPkValue(gridElement, "lineTypeMain", &lineTypeMain);
    m_lineTypeMain = LineTypeInternal(lineTypeMain);

    int lineTypeSubdivision = pkMin(cfg.readEntry("gridsubdivisionstyle", 1), 2);
    loadPkValue(gridElement, "lineTypeSubdivision", &lineTypeSubdivision);
    m_lineTypeSubdivision = LineTypeInternal(lineTypeSubdivision);

    int lineTypeVertical = pkBound(0, cfg.readEntry("gridisoverticalstyle", 0), 3);
    loadPkValue(gridElement, "lineTypeVertical", &lineTypeVertical);
    m_lineTypeIsoVertical = LineTypeInternal(lineTypeVertical);

    m_colorMain = cfg.readEntry("gridmaincolor", PkColor(99, 99, 99));
    loadPkValue(gridElement, "colorMain", &m_colorMain);

    m_colorSubdivision = cfg.readEntry("gridsubdivisioncolor", PkColor(150, 150, 150));
    loadPkValue(gridElement, "colorSubdivision", &m_colorSubdivision);

    m_colorIsoVertical = cfg.readEntry("gridisoverticalcolor", PkColor(150, 150, 150));
    loadPkValue(gridElement, "colorVertical", &m_colorIsoVertical);

    updatePenStyle(&m_penMain, m_colorMain, m_lineTypeMain);
    updatePenStyle(&m_penSubdivision, m_colorSubdivision, m_lineTypeSubdivision);
    updatePenStyle(&m_penVertical, m_colorIsoVertical, m_lineTypeIsoVertical);
    updateTrigoCache();

    return result;
}

void KisGridConfig::updatePenStyle(PkPen *pen, PkColor color, LineTypeInternal type)
{
    pen->setColor(color);

    if (type == LINE_DASHED) {
        pen->setDashPattern({5, 5});
    } else if (type == LINE_DOTTED) {
        pen->setStyle(Pk::DotLine);
    } else if (type == LINE_NONE) {
        pen->setStyle(Pk::NoPen);
    } else {
        // assume it's SOLID by default
        pen->setStyle(Pk::SolidLine);
    }
}

void KisGridConfig::updateTrigoCache()
{
    // Here some variable needed to render grid that can be calculated when grid settings in done, instead
    // of doing recalculation on every canvas refresh
    constexpr qreal degreesToRadians = 3.14159265358979323846 / 180.0;
    const qreal cosAngleRight = std::cos(m_angleRight * degreesToRadians);
    const qreal cosAngleLeft = std::cos(m_angleLeft * degreesToRadians);

    m_trigoCache.tanAngleRight = std::tan(m_angleRight * degreesToRadians);
    m_trigoCache.correctedAngleRightCellSize = m_cellSize * (std::sin(m_angleLeft * degreesToRadians) + cosAngleLeft * m_trigoCache.tanAngleRight);
    if (m_angleRight > 0.0) {
        m_trigoCache.correctedAngleRightOffsetX = m_offset.x() * m_trigoCache.tanAngleRight;
    } else {
        m_trigoCache.correctedAngleRightOffsetX = m_offset.x();
    }

    m_trigoCache.tanAngleLeft = std::tan(m_angleLeft * degreesToRadians);
    m_trigoCache.correctedAngleLeftCellSize = m_cellSize * (std::sin(m_angleRight * degreesToRadians) + cosAngleRight * m_trigoCache.tanAngleLeft);
    if (m_angleLeft > 0.0) {
        m_trigoCache.correctedAngleLeftOffsetX = m_offset.x() * m_trigoCache.tanAngleLeft;
    } else {
        m_trigoCache.correctedAngleLeftOffsetX = m_offset.x();
    }

    if (m_angleRight == m_angleLeft && m_lineTypeIsoVertical != LINE_NONE) {
        m_trigoCache.verticalSpace = m_subdivision * m_cellSize * (cosAngleLeft + cosAngleRight) / 2;
    } else {
        // allow vertical grid line only if angle left and right are the same
        m_trigoCache.verticalSpace = 0;
    }
}
