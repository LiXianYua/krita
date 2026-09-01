/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_grid_config.h"

#include <QDomElement>
#include <QLocale>
#include <QVariant>
#include <QtMath>

#include <type_traits>

#include <KConfigGroup>
#include <KSharedConfig>
#include "kis_algebra_2d.h"
#include <KisStaticInitializer.h>

KIS_DECLARE_STATIC_INITIALIZER {
    qRegisterMetaType<KisGridConfig>("KisGridConfig");
}

Q_GLOBAL_STATIC(KisGridConfig, staticDefaultObject)

namespace {

PkTransform toPkTransform(const QTransform &transform)
{
    return PkTransform(transform.m11(), transform.m12(), transform.m13(),
                       transform.m21(), transform.m22(), transform.m23(),
                       transform.m31(), transform.m32(), transform.m33());
}

PkPoint toPkPoint(const QPoint &point)
{
    return PkPoint(point.x(), point.y());
}

QPoint toQPoint(const PkPoint &point)
{
    return QPoint(point.x(), point.y());
}

template <typename T>
QString scalarToString(T value)
{
    if constexpr (std::is_same<T, double>::value || std::is_same<T, qreal>::value) {
        return QString::number(value, 'g', 15);
    } else if constexpr (std::is_enum<T>::value) {
        return QString::number(static_cast<typename std::underlying_type<T>::type>(value));
    } else {
        return QString::number(value);
    }
}

template <typename T>
void saveQtValue(QDomElement *parent, const QString &tag, T value)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("value"));
    element.setAttribute(QStringLiteral("value"), scalarToString(value));
}

void saveQtValue(QDomElement *parent, const QString &tag, const QPoint &point)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("point"));
    element.setAttribute(QStringLiteral("x"), scalarToString(point.x()));
    element.setAttribute(QStringLiteral("y"), scalarToString(point.y()));
}

void saveQtValue(QDomElement *parent, const QString &tag, const QColor &color)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("qcolor"));
    element.setAttribute(QStringLiteral("value"), color.name(QColor::HexArgb));
}

bool findOnlyQtElement(const QDomElement &parent, const QString &tag, QDomElement *element)
{
    const QDomNodeList list = parent.elementsByTagName(tag);
    if (list.size() != 1 || !list.at(0).isElement()) {
        return false;
    }
    *element = list.at(0).toElement();
    return true;
}

bool hasQtType(const QDomElement &element, const QString &type)
{
    return element.attribute(QStringLiteral("type"), QStringLiteral("unknown-type")) == type;
}

int qtToInt(const QString &text, bool *ok = nullptr)
{
    bool okLocale = false;
    int value = text.toInt(&okLocale);

    if (!okLocale) {
        value = QLocale(QLocale::German).toInt(text, &okLocale);
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
bool loadQtValue(const QDomElement &element, T *value)
{
    if (!hasQtType(element, QStringLiteral("value"))) return false;
    *value = QVariant(element.attribute(QStringLiteral("value"), QStringLiteral("no-value"))).value<T>();
    return true;
}

bool loadQtValue(const QDomElement &element, double *value)
{
    if (!hasQtType(element, QStringLiteral("value"))) return false;
    const QString text = element.attribute(QStringLiteral("value"), QStringLiteral("0"));
    bool ok = false;
    *value = text.toDouble(&ok);
    if (!ok) {
        *value = QLocale(QLocale::German).toDouble(text, &ok);
    }
    if (!ok) {
        *value = 0.0;
    }
    return true;
}

bool loadQtValue(const QDomElement &element, QPoint *point)
{
    if (!hasQtType(element, QStringLiteral("point"))) return false;
    point->setX(qtToInt(element.attribute(QStringLiteral("x"), QStringLiteral("0"))));
    point->setY(qtToInt(element.attribute(QStringLiteral("y"), QStringLiteral("0"))));
    return true;
}

bool loadQtValue(const QDomElement &element, QColor *color)
{
    if (!hasQtType(element, QStringLiteral("qcolor"))) return false;
    color->setNamedColor(element.attribute(QStringLiteral("value"), QStringLiteral("#FFFF0000")));
    return true;
}

template <typename T>
bool loadQtValue(const QDomElement &parent, const QString &tag, T *value)
{
    QDomElement element;
    return findOnlyQtElement(parent, tag, &element) && loadQtValue(element, value);
}

}

const KisGridConfig& KisGridConfig::defaultGrid()
{
    staticDefaultObject->loadStaticData();
    return *staticDefaultObject;
}

void KisGridConfig::transform(const QTransform &transform)
{
    if (transform.type() >= QTransform::TxShear) return;

    const PkTransform pkTransform = toPkTransform(transform);
    KisAlgebra2D::DecomposedMatrix m(pkTransform);

    if (m_gridType == GRID_RECTANGULAR) {
        PkTransform t = m.scaleTransform();

        const qreal eps = 1e-3;
        const qreal wrappedRotation = KisAlgebra2D::wrapValue(m.angle, 90.0);
        if (wrappedRotation <= eps || wrappedRotation >= 90.0 - eps) {
            t *= m.rotateTransform();
        }

        m_spacing = toQPoint(KisAlgebra2D::abs(t.map(toPkPoint(m_spacing))));
        // Transform map may round spacing down to 0, but it must be at least 1
        m_spacing.setX(qMax(1, m_spacing.x()));
        m_spacing.setY(qMax(1, m_spacing.y()));

    } else if (m_gridType == GRID_ISOMETRIC_LEGACY) {
        if (qFuzzyCompare(m.scaleX, m.scaleY)) {
            m_cellSpacing = qRound(qAbs(m_cellSpacing * m.scaleX));
        }
    }
    m_offset = toQPoint(KisAlgebra2D::wrapValue(pkTransform.map(toPkPoint(m_offset)),
                                                toPkPoint(m_spacing)));
}

void KisGridConfig::loadStaticData()
{
    const KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());

    m_lineTypeMain = LineTypeInternal(qBound(0, cfg.readEntry("gridmainstyle", 0), 2));
    m_lineTypeSubdivision = LineTypeInternal(qMin(cfg.readEntry("gridsubdivisionstyle", 1), 2));
    m_lineTypeIsoVertical = LineTypeInternal(qBound(0, cfg.readEntry("gridisoverticalstyle", 0), 3));

    m_colorMain = cfg.readEntry("gridmaincolor", QColor(99, 99, 99));
    m_colorSubdivision = cfg.readEntry("gridsubdivisioncolor", QColor(150, 150, 150));
    m_colorIsoVertical = cfg.readEntry("gridisoverticalcolor", QColor(150, 150, 150));

    m_spacing = cfg.readEntry("defaultGridSpacing", QPoint(16, 16));
}

void KisGridConfig::saveStaticData() const
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());
    cfg.writeEntry("gridmainstyle", quint32(m_lineTypeMain));
    cfg.writeEntry("gridsubdivisionstyle", quint32(m_lineTypeSubdivision));
    cfg.writeEntry("gridisoverticalstyle", quint32(m_lineTypeIsoVertical));
    cfg.writeEntry("gridmaincolor", m_colorMain);
    cfg.writeEntry("gridsubdivisioncolor", m_colorSubdivision);
    cfg.writeEntry("gridisoverticalcolor", m_colorIsoVertical);
    cfg.sync();
}

QDomElement KisGridConfig::saveDynamicDataToXml(QDomDocument& doc, const QString &tag) const
{
    QDomElement gridElement = doc.createElement(tag);
    saveQtValue(&gridElement, "showGrid", m_showGrid);
    saveQtValue(&gridElement, "snapToGrid", m_snapToGrid);
    saveQtValue(&gridElement, "offsetActive", m_offsetActive);
    saveQtValue(&gridElement, "offset", m_offset);
    saveQtValue(&gridElement, "spacing", m_spacing);
    saveQtValue(&gridElement, "xSpacingActive", m_xSpacingActive);
    saveQtValue(&gridElement, "ySpacingActive", m_ySpacingActive);
    saveQtValue(&gridElement, "offsetAspectLocked", m_offsetAspectLocked);
    saveQtValue(&gridElement, "spacingAspectLocked", m_spacingAspectLocked);
    saveQtValue(&gridElement, "subdivision", m_subdivision);
    saveQtValue(&gridElement, "angleLeft", m_angleLeft);
    saveQtValue(&gridElement, "angleRight", m_angleRight);
    saveQtValue(&gridElement, "angleLeftActive", m_angleLeftActive);
    saveQtValue(&gridElement, "angleRightActive", m_angleRightActive);
    saveQtValue(&gridElement, "angleAspectLocked", m_angleAspectLocked);
    saveQtValue(&gridElement, "cellSpacing", m_cellSpacing);
    saveQtValue(&gridElement, "cellSize", m_cellSize);
    saveQtValue(&gridElement, "gridType", m_gridType);

    saveQtValue(&gridElement, "colorMain", m_colorMain);
    saveQtValue(&gridElement, "colorSubdivision", m_colorSubdivision);
    saveQtValue(&gridElement, "colorVertical", m_colorIsoVertical);
    saveQtValue(&gridElement, "lineTypeMain", m_lineTypeMain);
    saveQtValue(&gridElement, "lineTypeSubdivision", m_lineTypeSubdivision);
    saveQtValue(&gridElement, "lineTypeVertical", m_lineTypeIsoVertical);

    return gridElement;
}

bool KisGridConfig::loadDynamicDataFromXml(const QDomElement &gridElement)
{
    const KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());
    bool result = true;

    result &= loadQtValue(gridElement, "showGrid", &m_showGrid);
    result &= loadQtValue(gridElement, "snapToGrid", &m_snapToGrid);
    result &= loadQtValue(gridElement, "offset", &m_offset);
    result &= loadQtValue(gridElement, "spacing", &m_spacing);
    result &= loadQtValue(gridElement, "offsetAspectLocked", &m_offsetAspectLocked);
    result &= loadQtValue(gridElement, "spacingAspectLocked", &m_spacingAspectLocked);
    result &= loadQtValue(gridElement, "subdivision", &m_subdivision);
    result &= loadQtValue(gridElement, "angleLeft", &m_angleLeft);
    result &= loadQtValue(gridElement, "angleRight", &m_angleRight);
    result &= loadQtValue(gridElement, "cellSpacing", &m_cellSpacing);
    result &= loadQtValue(gridElement, "gridType", (int*)(&m_gridType));

    // following variables may not be present in older files; do not update result variable
    loadQtValue(gridElement, "offsetActive", &m_offsetActive);
    loadQtValue(gridElement, "xSpacingActive", &m_xSpacingActive);
    loadQtValue(gridElement, "ySpacingActive", &m_ySpacingActive);
    loadQtValue(gridElement, "angleLeftActive", &m_angleLeftActive);
    loadQtValue(gridElement, "angleRightActive", &m_angleRightActive);
    loadQtValue(gridElement, "angleAspectLocked", &m_angleAspectLocked);
    loadQtValue(gridElement, "cellSize", &m_cellSize);

    int lineTypeMain = qBound(0, cfg.readEntry("gridmainstyle", 0), 2);
    loadQtValue(gridElement, "lineTypeMain", &lineTypeMain);
    m_lineTypeMain = LineTypeInternal(lineTypeMain);

    int lineTypeSubdivision = qMin(cfg.readEntry("gridsubdivisionstyle", 1), 2);
    loadQtValue(gridElement, "lineTypeSubdivision", &lineTypeSubdivision);
    m_lineTypeSubdivision = LineTypeInternal(lineTypeSubdivision);

    int lineTypeVertical = qBound(0, cfg.readEntry("gridisoverticalstyle", 0), 3);
    loadQtValue(gridElement, "lineTypeVertical", &lineTypeVertical);
    m_lineTypeIsoVertical = LineTypeInternal(lineTypeVertical);

    m_colorMain = cfg.readEntry("gridmaincolor", QColor(99, 99, 99));
    loadQtValue(gridElement, "colorMain", &m_colorMain);

    m_colorSubdivision = cfg.readEntry("gridsubdivisioncolor", QColor(150, 150, 150));
    loadQtValue(gridElement, "colorSubdivision", &m_colorSubdivision);

    m_colorIsoVertical = cfg.readEntry("gridisoverticalcolor", QColor(150, 150, 150));
    loadQtValue(gridElement, "colorVertical", &m_colorIsoVertical);

    updatePenStyle(&m_penMain, m_colorMain, m_lineTypeMain);
    updatePenStyle(&m_penSubdivision, m_colorSubdivision, m_lineTypeSubdivision);
    updatePenStyle(&m_penVertical, m_colorIsoVertical, m_lineTypeIsoVertical);
    updateTrigoCache();

    return result;
}

void KisGridConfig::updatePenStyle(QPen *pen, QColor color, LineTypeInternal type)
{
    pen->setColor(color);

    if (type == LINE_DASHED) {
        QVector<qreal> dashes;
        dashes << 5 << 5;
        pen->setDashPattern(dashes);
    } else if (type == LINE_DOTTED) {
        pen->setStyle(Qt::DotLine);
    } else if (type == LINE_NONE) {
        pen->setStyle(Qt::NoPen);
    } else {
        // assume it's SOLID by default
        pen->setStyle(Qt::SolidLine);
    }
}

void KisGridConfig::updateTrigoCache()
{
    // Here some variable needed to render grid that can be calculated when grid settings in done, instead
    // of doing recalculation on every canvas refresh
    const qreal cosAngleRight = qCos(qDegreesToRadians(m_angleRight));
    const qreal cosAngleLeft = qCos(qDegreesToRadians(m_angleLeft));

    m_trigoCache.tanAngleRight = qTan(qDegreesToRadians(m_angleRight));
    m_trigoCache.correctedAngleRightCellSize = m_cellSize * (qSin(qDegreesToRadians(m_angleLeft)) + cosAngleLeft * m_trigoCache.tanAngleRight);
    if (m_angleRight > 0.0) {
        m_trigoCache.correctedAngleRightOffsetX = m_offset.x() * m_trigoCache.tanAngleRight;
    } else {
        m_trigoCache.correctedAngleRightOffsetX = m_offset.x();
    }

    m_trigoCache.tanAngleLeft = qTan(qDegreesToRadians(m_angleLeft));
    m_trigoCache.correctedAngleLeftCellSize = m_cellSize * (qSin(qDegreesToRadians(m_angleRight)) + cosAngleRight * m_trigoCache.tanAngleLeft);
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
