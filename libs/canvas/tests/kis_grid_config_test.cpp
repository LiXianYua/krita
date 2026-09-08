/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_grid_config_test.h"

#include <simpletest.h>
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include <PkFlakeBridge.h>
#include <PkXmlDocument.h>
#include <QDomDocument>
#include <QDomElement>

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

}


void KisGridConfigTest::testGridConfig()
{
    KisGridConfig config;
    config.setSpacing(QPoint(10,13));
    config.setOffset(QPoint(13,14));
    config.setOffsetAspectLocked(false);
    config.setSubdivision(4);

    QVERIFY(!config.isDefault());

    QDomDocument doc;
    QDomElement root = doc.createElement("TestXMLRoot");
    doc.appendChild(root);
    QDomElement el = config.saveDynamicDataToXml(doc, "test_tag");
    root.appendChild(el);

    QByteArray b = doc.toByteArray(4);
    //printf(b.data());

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

    config.addGuideLine(Qt::Horizontal, 100.0);
    config.addGuideLine(Qt::Horizontal, 200.0);

    config.addGuideLine(Qt::Vertical, 300.0);
    config.addGuideLine(Qt::Vertical, 400.0);

    QVERIFY(config.hasGuides());

    QDomDocument doc;
    QDomElement root = doc.createElement("TestXMLRoot");
    doc.appendChild(root);
    QDomElement el = config.saveToXml(doc, "test_tag");
    root.appendChild(el);

    QByteArray b = doc.toByteArray(4);
    //printf(b.data());

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
    config.setOffset(QPoint(13, 14));
    config.setSpacing(QPoint(10, 17));
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
    config.setColorMain(QColor(1, 2, 3, 4));
    config.setColorSubdivision(QColor(5, 6, 7, 8));
    config.setColorVertical(QColor(9, 10, 11, 12));
    config.setLineTypeMain(KisGridConfig::LINE_DASHED);
    config.setLineTypeSubdivision(KisGridConfig::LINE_DOTTED);
    config.setLineTypeVertical(KisGridConfig::LINE_NONE);

    QDomDocument qtDoc;
    const QDomElement qtOracle = config.saveDynamicDataToXml(qtDoc, QStringLiteral("grid"));

    PkXmlDocument pkDoc;
    const PkXmlElement pkElement = config.saveDynamicDataToXml(pkDoc, PkString("grid"));
    pkDoc.appendChild(pkElement);

    QDomDocument parsedPkDoc;
    QVERIFY(parsedPkDoc.setContent(toQString(pkDoc.toString(-1))));
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
    config.setGuidesColor(QColor(17, 34, 51, 68));
    config.addGuideLine(Qt::Horizontal, 12.5);
    config.addGuideLine(Qt::Horizontal, 200.25);
    config.addGuideLine(Qt::Vertical, 300.75);
    config.addGuideLine(Qt::Vertical, 400.125);

    QDomDocument qtDoc;
    const QDomElement qtOracle = config.saveToXml(qtDoc, QStringLiteral("guides"));

    PkXmlDocument pkDoc;
    const PkXmlElement pkElement = config.saveToXml(pkDoc, PkString("guides"));
    pkDoc.appendChild(pkElement);

    QDomDocument parsedPkDoc;
    QVERIFY(parsedPkDoc.setContent(toQString(pkDoc.toString(-1))));
    QVERIFY(domNodesEquivalent(parsedPkDoc.documentElement(), qtOracle));
}

SIMPLE_TEST_MAIN(KisGridConfigTest)
