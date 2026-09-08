/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_grid_config_test.h"

#include <simpletest.h>
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <QColor>
#include <QDomDocument>
#include <QDomElement>

#include <type_traits>

namespace {

bool domNodesEquivalent(const QDomNode &lhs, const QDomNode &rhs)
{
    if (lhs.nodeType() != rhs.nodeType() || lhs.nodeName() != rhs.nodeName() ||
        lhs.nodeValue() != rhs.nodeValue()) {
        return false;
    }

    if (lhs.isElement()) {
        const QDomElement lhsElement = lhs.toElement();
        const QDomElement rhsElement = rhs.toElement();
        const QDomNamedNodeMap lhsAttributes = lhsElement.attributes();
        const QDomNamedNodeMap rhsAttributes = rhsElement.attributes();
        if (lhsAttributes.count() != rhsAttributes.count()) {
            return false;
        }
        for (int i = 0; i < rhsAttributes.count(); ++i) {
            const QDomNode attribute = rhsAttributes.item(i);
            if (!lhsElement.hasAttribute(attribute.nodeName()) ||
                lhsElement.attribute(attribute.nodeName()) != attribute.nodeValue()) {
                return false;
            }
        }
    }

    const QDomNodeList lhsChildren = lhs.childNodes();
    const QDomNodeList rhsChildren = rhs.childNodes();
    if (lhsChildren.count() != rhsChildren.count()) {
        return false;
    }
    for (int i = 0; i < rhsChildren.count(); ++i) {
        if (!domNodesEquivalent(lhsChildren.item(i), rhsChildren.item(i))) {
            return false;
        }
    }
    return true;
}

template <typename T>
QString qt515ScalarToString(T value)
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
void appendQt515Value(QDomElement *parent, const QString &tag, T value)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("value"));
    element.setAttribute(QStringLiteral("value"), qt515ScalarToString(value));
}

void appendQt515Value(QDomElement *parent, const QString &tag, const PkPoint &point)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("point"));
    element.setAttribute(QStringLiteral("x"), qt515ScalarToString(point.x()));
    element.setAttribute(QStringLiteral("y"), qt515ScalarToString(point.y()));
}

void appendQt515Value(QDomElement *parent, const QString &tag, const PkColor &color)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("qcolor"));
    const QColor qtColor(color.red(), color.green(), color.blue(), color.alpha());
    element.setAttribute(QStringLiteral("value"), qtColor.name(QColor::HexArgb));
}

void appendQt515Value(QDomElement *parent, const QString &tag, const PkString &value)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("value"));
    element.setAttribute(QStringLiteral("value"),
                         QString::fromUtf8(value.PkToUtf8().c_str()));
}

void appendQt515Array(QDomElement *parent, const QString &tag,
                      const PkVector<qreal> &values)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement element = doc.createElement(tag);
    parent->appendChild(element);
    element.setAttribute(QStringLiteral("type"), QStringLiteral("array"));

    int index = 0;
    for (qreal value : values) {
        appendQt515Value(&element, QStringLiteral("item_%1").arg(index++), value);
    }
}

QDomElement createQt515GridOracle(QDomDocument &doc, const KisGridConfig &config)
{
    QDomElement gridElement = doc.createElement(QStringLiteral("grid"));
    appendQt515Value(&gridElement, QStringLiteral("showGrid"), config.showGrid());
    appendQt515Value(&gridElement, QStringLiteral("snapToGrid"), config.snapToGrid());
    appendQt515Value(&gridElement, QStringLiteral("offsetActive"), config.offsetActive());
    appendQt515Value(&gridElement, QStringLiteral("offset"), config.offset());
    appendQt515Value(&gridElement, QStringLiteral("spacing"), config.spacing());
    appendQt515Value(&gridElement, QStringLiteral("xSpacingActive"), config.xSpacingActive());
    appendQt515Value(&gridElement, QStringLiteral("ySpacingActive"), config.ySpacingActive());
    appendQt515Value(&gridElement, QStringLiteral("offsetAspectLocked"), config.offsetAspectLocked());
    appendQt515Value(&gridElement, QStringLiteral("spacingAspectLocked"), config.spacingAspectLocked());
    appendQt515Value(&gridElement, QStringLiteral("subdivision"), config.subdivision());
    appendQt515Value(&gridElement, QStringLiteral("angleLeft"), config.angleLeft());
    appendQt515Value(&gridElement, QStringLiteral("angleRight"), config.angleRight());
    appendQt515Value(&gridElement, QStringLiteral("angleLeftActive"), config.angleLeftActive());
    appendQt515Value(&gridElement, QStringLiteral("angleRightActive"), config.angleRightActive());
    appendQt515Value(&gridElement, QStringLiteral("angleAspectLocked"), config.angleAspectLocked());
    appendQt515Value(&gridElement, QStringLiteral("cellSpacing"), config.cellSpacing());
    appendQt515Value(&gridElement, QStringLiteral("cellSize"), config.cellSize());
    appendQt515Value(&gridElement, QStringLiteral("gridType"), config.gridType());
    appendQt515Value(&gridElement, QStringLiteral("colorMain"), config.colorMain());
    appendQt515Value(&gridElement, QStringLiteral("colorSubdivision"), config.colorSubdivision());
    appendQt515Value(&gridElement, QStringLiteral("colorVertical"), config.colorVertical());
    appendQt515Value(&gridElement, QStringLiteral("lineTypeMain"), config.lineTypeMain());
    appendQt515Value(&gridElement, QStringLiteral("lineTypeSubdivision"), config.lineTypeSubdivision());
    appendQt515Value(&gridElement, QStringLiteral("lineTypeVertical"), config.lineTypeVertical());
    return gridElement;
}

QDomElement createQt515GuidesOracle(QDomDocument &doc, const KisGuidesConfig &config)
{
    QDomElement guidesElement = doc.createElement(QStringLiteral("guides"));
    appendQt515Value(&guidesElement, QStringLiteral("showGuides"), config.showGuides());
    appendQt515Value(&guidesElement, QStringLiteral("snapToGuides"), config.snapToGuides());
    appendQt515Value(&guidesElement, QStringLiteral("lockGuides"), config.lockGuides());
    appendQt515Value(&guidesElement, QStringLiteral("colorGuides"), config.guidesColor());
    appendQt515Value(&guidesElement, QStringLiteral("lineTypeGuides"), config.guidesLineType());
    appendQt515Array(&guidesElement, QStringLiteral("horizontalGuides"),
                     config.horizontalGuideLines());
    appendQt515Array(&guidesElement, QStringLiteral("verticalGuides"),
                     config.verticalGuideLines());
    appendQt515Value(&guidesElement, QStringLiteral("rulersMultiple2"), config.rulersMultiple2());
    appendQt515Value(&guidesElement, QStringLiteral("unit"), KoUnit(config.unitType()).symbol());
    return guidesElement;
}

}


void KisGridConfigTest::testGridConfig()
{
    KisGridConfig config;
    config.setSpacing(PkPoint(10,13));
    config.setOffset(PkPoint(13,14));
    config.setOffsetAspectLocked(false);
    config.setSubdivision(4);

    QVERIFY(!config.isDefault());

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("TestXMLRoot");
    doc.appendChild(root);
    PkXmlElement el = config.saveDynamicDataToXml(doc, "test_tag");
    root.appendChild(el);

    const PkString xml = doc.toString(-1);
    QVERIFY(xml.contains(PkString("<spacing type=\"point\" x=\"10\" y=\"13\"/>")));
    QVERIFY(xml.contains(PkString("<offset type=\"point\" x=\"13\" y=\"14\"/>")));

    KisGridConfig config2;
    QVERIFY(config2.isDefault());
    QVERIFY(config2.loadDynamicDataFromXml(el));

    QCOMPARE(config2, config);
    QVERIFY(!config2.isDefault());
}

void KisGridConfigTest::testGuidesConfig()
{
    KisGuidesConfig config;
    config.setShowGuides(true);
    config.setLockGuides(true);
    config.setSnapToGuides(true);

    config.addGuideLine(Pk::Horizontal, 100.0);
    config.addGuideLine(Pk::Horizontal, 200.0);

    config.addGuideLine(Pk::Vertical, 300.0);
    config.addGuideLine(Pk::Vertical, 400.0);

    QVERIFY(config.hasGuides());

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("TestXMLRoot");
    doc.appendChild(root);
    PkXmlElement el = config.saveToXml(doc, "test_tag");
    root.appendChild(el);

    const PkString xml = doc.toString(-1);
    QVERIFY(xml.contains(PkString("<horizontalGuides")));
    QVERIFY(xml.contains(PkString("<verticalGuides")));

    KisGuidesConfig config2;
    QVERIFY(!config2.hasGuides());
    QVERIFY(config2.loadFromXml(el));

    QCOMPARE(config2, config);
    QVERIFY(config2.hasGuides());
}

void KisGridConfigTest::testPkGridSerializationMatchesQt515()
{
    KisGridConfig config;
    config.setShowGrid(true);
    config.setSnapToGrid(true);
    config.setOffsetActive(true);
    config.setOffset(PkPoint(13, 14));
    config.setSpacing(PkPoint(10, 17));
    config.setXSpacingActive(false);
    config.setYSpacingActive(true);
    config.setOffsetAspectLocked(false);
    config.setSpacingAspectLocked(false);
    config.setSubdivision(4);
    config.setAngleLeft(31.25);
    config.setAngleRight(62.5);
    config.setAngleLeftActive(false);
    config.setAngleRightActive(true);
    config.setAngleAspectLocked(false);
    config.setCellSpacing(37);
    config.setCellSize(41);
    config.setGridType(KisGridConfig::GRID_ISOMETRIC);
    config.setColorMain(PkColor(1, 2, 3, 4));
    config.setColorSubdivision(PkColor(5, 6, 7, 8));
    config.setColorVertical(PkColor(9, 10, 11, 12));
    config.setLineTypeMain(KisGridConfig::LINE_DASHED);
    config.setLineTypeSubdivision(KisGridConfig::LINE_DOTTED);
    config.setLineTypeVertical(KisGridConfig::LINE_NONE);

    QDomDocument qtDoc;
    const QDomElement qtOracle = createQt515GridOracle(qtDoc, config);
    qtDoc.appendChild(qtOracle);

    PkXmlDocument pkDoc;
    const PkXmlElement pkElement = config.saveDynamicDataToXml(pkDoc, PkString("grid"));
    pkDoc.appendChild(pkElement);

    QDomDocument parsedPkDoc;
    QVERIFY(parsedPkDoc.setContent(QString::fromUtf8(pkDoc.toString(-1).PkToUtf8().c_str())));
    QVERIFY(domNodesEquivalent(parsedPkDoc.documentElement(), qtOracle));
}

void KisGridConfigTest::testPkGuidesSerializationMatchesQt515()
{
    KisGuidesConfig config;
    config.setShowGuides(true);
    config.setLockGuides(true);
    config.setSnapToGuides(true);
    config.setRulersMultiple2(true);
    config.setUnitType(KoUnit::Millimeter);
    config.setGuidesLineType(KisGuidesConfig::LINE_DOTTED);
    config.setGuidesColor(PkColor(17, 34, 51, 68));
    config.addGuideLine(Pk::Horizontal, 12.5);
    config.addGuideLine(Pk::Horizontal, 200.25);
    config.addGuideLine(Pk::Vertical, 300.75);
    config.addGuideLine(Pk::Vertical, 400.125);

    QDomDocument qtDoc;
    const QDomElement qtOracle = createQt515GuidesOracle(qtDoc, config);
    qtDoc.appendChild(qtOracle);

    PkXmlDocument pkDoc;
    const PkXmlElement pkElement = config.saveToXml(pkDoc, PkString("guides"));
    pkDoc.appendChild(pkElement);

    QDomDocument parsedPkDoc;
    QVERIFY(parsedPkDoc.setContent(QString::fromUtf8(pkDoc.toString(-1).PkToUtf8().c_str())));
    QVERIFY(domNodesEquivalent(parsedPkDoc.documentElement(), qtOracle));
}

SIMPLE_TEST_MAIN(KisGridConfigTest)
