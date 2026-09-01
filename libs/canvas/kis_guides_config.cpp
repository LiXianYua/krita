/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
   SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QColor>
#include <QDomDocument>
#include <QList>
#include <QPen>
#include <QString>
#include <QTransform>
#include <QVector>

#include <PkFlakeBridge.h>

#include "kis_guides_config.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include "kis_dom_utils.h"
#include "kis_algebra_2d.h"
#include "kis_global.h"
#include <KisStaticInitializer.h>

KIS_DECLARE_STATIC_INITIALIZER {
    qRegisterMetaType<KisGuidesConfig>("KisGuidesConfig");
}

class Q_DECL_HIDDEN KisGuidesConfig::Private
{
public:

    bool operator==(const Private &rhs) {
        return horzGuideLines == rhs.horzGuideLines &&
            vertGuideLines == rhs.vertGuideLines &&
            showGuides == rhs.showGuides &&
            snapToGuides == rhs.snapToGuides &&
            lockGuides == rhs.lockGuides &&
            guidesColor == rhs.guidesColor &&
            guidesLineType == rhs.guidesLineType &&
            rulersMultiple2 == rhs.rulersMultiple2 &&
            unitType == rhs.unitType;
    }

    QList<qreal> horzGuideLines;
    QList<qreal> vertGuideLines;

    bool showGuides {false};
    bool snapToGuides {false};
    bool lockGuides {false};
    bool rulersMultiple2 {false};

    KoUnit::Type unitType {KoUnit::Pixel};

    QColor guidesColor;
    LineTypeInternal guidesLineType {LINE_SOLID};

    Qt::PenStyle toPenStyle(LineTypeInternal type);
};

KisGuidesConfig::KisGuidesConfig()
    : d(new Private())
{
    loadStaticData();
}

KisGuidesConfig::~KisGuidesConfig()
{
}

KisGuidesConfig::KisGuidesConfig(const KisGuidesConfig &rhs)
    : d(new Private(*rhs.d))
{
}

KisGuidesConfig& KisGuidesConfig::operator=(const KisGuidesConfig &rhs)
{
    if (&rhs != this) {
        *d = *rhs.d;
    }

    return *this;
}

bool KisGuidesConfig::operator==(const KisGuidesConfig &rhs) const
{
    return *d == *rhs.d;
}

bool KisGuidesConfig::hasSamePositionAs(const KisGuidesConfig &rhs) const
{
    return horizontalGuideLines() == rhs.horizontalGuideLines() &&
        verticalGuideLines() == rhs.verticalGuideLines();
}

void KisGuidesConfig::setHorizontalGuideLines(const QList<qreal> &lines)
{
    d->horzGuideLines = lines;
}

void KisGuidesConfig::setVerticalGuideLines(const QList<qreal> &lines)
{
    d->vertGuideLines = lines;
}

void KisGuidesConfig::addGuideLine(Qt::Orientation o, qreal pos)
{
    if (o == Qt::Horizontal) {
        d->horzGuideLines.append(pos);
    } else {
        d->vertGuideLines.append(pos);
    }
}

void KisGuidesConfig::removeAllGuides()
{
    QList<qreal> emptyGuides ;
    setVerticalGuideLines(emptyGuides);
    setHorizontalGuideLines(emptyGuides);
}

bool KisGuidesConfig::showGuides() const
{
    return d->showGuides;
}

void KisGuidesConfig::setShowGuides(bool value)
{
    d->showGuides = value;
}

bool KisGuidesConfig::lockGuides() const
{
    return d->lockGuides;
}

void KisGuidesConfig::setLockGuides(bool value)
{
    d->lockGuides = value;
}

bool KisGuidesConfig::snapToGuides() const
{
    return d->snapToGuides;
}

void KisGuidesConfig::setSnapToGuides(bool value)
{
    d->snapToGuides = value;
}

bool KisGuidesConfig::rulersMultiple2() const
{
    return d->rulersMultiple2;
}

void KisGuidesConfig::setRulersMultiple2(bool value)
{
    d->rulersMultiple2 = value;
}

KoUnit::Type KisGuidesConfig::unitType() const
{
    return d->unitType;
}

void KisGuidesConfig::setUnitType(const KoUnit::Type type)
{
    d->unitType = type;
}

KisGuidesConfig::LineTypeInternal
KisGuidesConfig::guidesLineType() const
{
    return d->guidesLineType;
}

void KisGuidesConfig::setGuidesLineType(LineTypeInternal value)
{
    d->guidesLineType = value;
}


QColor KisGuidesConfig::guidesColor() const
{
    return d->guidesColor;
}

void KisGuidesConfig::setGuidesColor(const QColor &value)
{
    d->guidesColor = value;
}

 Qt::PenStyle KisGuidesConfig::Private::toPenStyle(LineTypeInternal type) {
    return type == LINE_SOLID ? Qt::SolidLine :
        type == LINE_DASHED ? Qt::DashLine :
        type == LINE_DOTTED ? Qt::DotLine :
        Qt::DashDotDotLine;
}

QPen KisGuidesConfig::guidesPen() const
{
    return QPen(d->guidesColor, 0, d->toPenStyle(d->guidesLineType));
}

const QList<qreal>& KisGuidesConfig::horizontalGuideLines() const
{
    return d->horzGuideLines;
}

const QList<qreal>& KisGuidesConfig::verticalGuideLines() const
{
    return d->vertGuideLines;
}

bool KisGuidesConfig::hasGuides() const
{
    return !d->horzGuideLines.isEmpty() || !d->vertGuideLines.isEmpty();
}

void KisGuidesConfig::loadStaticData()
{
    const KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());
    d->guidesLineType = LineTypeInternal(qBound(0, cfg.readEntry("guidesLineStyle", 0), 2));
    d->guidesColor = cfg.readEntry("guidesColor", QColor(99, 99, 99));
}

void KisGuidesConfig::saveStaticData() const
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());
    cfg.writeEntry("guidesLineStyle", quint32(d->guidesLineType));
    cfg.writeEntry("guidesColor", d->guidesColor);
    cfg.sync();
}

QDomElement KisGuidesConfig::saveToXml(QDomDocument& doc, const QString &tag) const
{
    PkXmlDocument pkDoc;
    PkXmlElement guidesElement = pkDoc.createElement(toPkString(tag));
    pkDoc.appendChild(guidesElement);
    KisDomUtils::saveValue(&guidesElement, "showGuides", d->showGuides);
    KisDomUtils::saveValue(&guidesElement, "snapToGuides", d->snapToGuides);
    KisDomUtils::saveValue(&guidesElement, "lockGuides", d->lockGuides);
    KisDomUtils::saveValue(&guidesElement, "colorGuides", toPkColor(d->guidesColor));
    KisDomUtils::saveValue(&guidesElement, "lineTypeGuides", d->guidesLineType);

    PkVector<qreal> horizontalGuides;
    for (qreal value : d->horzGuideLines) horizontalGuides.append(value);
    PkVector<qreal> verticalGuides;
    for (qreal value : d->vertGuideLines) verticalGuides.append(value);
    KisDomUtils::saveValue(&guidesElement, "horizontalGuides", horizontalGuides);
    KisDomUtils::saveValue(&guidesElement, "verticalGuides", verticalGuides);

    KisDomUtils::saveValue(&guidesElement, "rulersMultiple2", d->rulersMultiple2);
    KoUnit tmp(d->unitType);
    KisDomUtils::saveValue(&guidesElement, "unit", tmp.symbol());

    return doc.importNode(toQDomElement(guidesElement), true).toElement();
}

bool KisGuidesConfig::loadFromXml(const QDomElement &parent)
{
    const PkXmlElement pkParent = toPkXmlElement(parent);
    const KConfigGroup cfg = KSharedConfig::openConfig()->group(QString());
    bool result = true;

    result &= KisDomUtils::loadValue(pkParent, "showGuides", &d->showGuides);
    result &= KisDomUtils::loadValue(pkParent, "snapToGuides", &d->snapToGuides);
    result &= KisDomUtils::loadValue(pkParent, "lockGuides", &d->lockGuides);

    PkVector<qreal> hGuides;
    PkVector<qreal> vGuides;

    result &= KisDomUtils::loadValue(pkParent, "horizontalGuides", &hGuides);
    result &= KisDomUtils::loadValue(pkParent, "verticalGuides", &vGuides);

    d->horzGuideLines.clear();
    for (qreal value : hGuides) d->horzGuideLines.append(value);
    d->vertGuideLines.clear();
    for (qreal value : vGuides) d->vertGuideLines.append(value);

    result &= KisDomUtils::loadValue(pkParent, "rulersMultiple2", &d->rulersMultiple2);
    PkString unit;
    result &= KisDomUtils::loadValue(pkParent, "unit", &unit);
    bool ok = false;
    KoUnit tmp = KoUnit::fromSymbol(unit, &ok);
    if (ok) {
        d->unitType = tmp.type();
    }
    result &= ok;

    // following variables may not be present in older files; do not update result variable
    int guidesLineType = qBound(0, cfg.readEntry("guidesLineStyle", 0), 2);
    KisDomUtils::loadValue(pkParent, "lineTypeGuides", &guidesLineType);
    d->guidesLineType = LineTypeInternal(guidesLineType);

    d->guidesColor = cfg.readEntry("guidesColor", QColor(99, 99, 99));
    PkColor guidesColor = toPkColor(d->guidesColor);
    if (KisDomUtils::loadValue(pkParent, "colorGuides", &guidesColor)) {
        d->guidesColor = toQColor(guidesColor);
    }

    return result;
}

bool KisGuidesConfig::isDefault() const
{
    KisGuidesConfig defaultObject;
    defaultObject.loadStaticData();

    return *this == defaultObject;
}

void KisGuidesConfig::transform(const QTransform &transform)
{
    if (transform.type() >= QTransform::TxShear) return;

    KisAlgebra2D::DecomposedMatrix m(toPkTransform(transform));

    PkTransform t = m.scaleTransform();

    const qreal eps = 1e-3;
    int numWraps = 0;
    const qreal wrappedRotation = KisAlgebra2D::wrapValue(m.angle, 90.0);
    if (wrappedRotation <= eps || wrappedRotation >= 90.0 - eps) {
        t *= m.rotateTransform();
        numWraps = qRound(normalizeAngleDegrees(m.angle) / 90.0);
    }

    t *= m.translateTransform();


    QList<qreal> newHorzGuideLines;
    QList<qreal> newVertGuideLines;

    Q_FOREACH (qreal hRuler, d->horzGuideLines) {
        const PkPointF pt = t.map(PkPointF(0, hRuler));

        if (numWraps & 0x1) {
            newVertGuideLines << pt.x();
        } else {
            newHorzGuideLines << pt.y();
        }
    }

    Q_FOREACH (qreal vRuler, d->vertGuideLines) {
        const PkPointF pt = t.map(PkPointF(vRuler, 0));

        if (!(numWraps & 0x1)) {
            newVertGuideLines << pt.x();
        } else {
            newHorzGuideLines << pt.y();
        }
    }

    d->horzGuideLines = newHorzGuideLines;
    d->vertGuideLines = newVertGuideLines;
}
