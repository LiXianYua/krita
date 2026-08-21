/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_dom_utils.h"

#include <PkTransform.h>
#include <PkVectorND.h>

#include "kis_debug.h"

namespace KisDomUtils {

void saveValue(PkXmlElement *parent, const PkString &tag, const PkSize &size)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "size");

    e.setAttribute("w", toString(size.width()));
    e.setAttribute("h", toString(size.height()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkRect &rc)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "rect");

    e.setAttribute("x", toString(rc.x()));
    e.setAttribute("y", toString(rc.y()));
    e.setAttribute("w", toString(rc.width()));
    e.setAttribute("h", toString(rc.height()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkRectF &rc)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "rectf");

    e.setAttribute("x", toString(rc.x()));
    e.setAttribute("y", toString(rc.y()));
    e.setAttribute("w", toString(rc.width()));
    e.setAttribute("h", toString(rc.height()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkPoint &pt)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "point");

    e.setAttribute("x", toString(pt.x()));
    e.setAttribute("y", toString(pt.y()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkPointF &pt)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "pointf");

    e.setAttribute("x", toString(pt.x()));
    e.setAttribute("y", toString(pt.y()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkVector3D &pt)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "vector3d");

    e.setAttribute("x", toString(pt.x()));
    e.setAttribute("y", toString(pt.y()));
    e.setAttribute("z", toString(pt.z()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkTransform &t)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "transform");

    e.setAttribute("m11", toString(t.m11()));
    e.setAttribute("m12", toString(t.m12()));
    e.setAttribute("m13", toString(t.m13()));

    e.setAttribute("m21", toString(t.m21()));
    e.setAttribute("m22", toString(t.m22()));
    e.setAttribute("m23", toString(t.m23()));

    e.setAttribute("m31", toString(t.m31()));
    e.setAttribute("m32", toString(t.m32()));
    e.setAttribute("m33", toString(t.m33()));
}

void saveValue(PkXmlElement *parent, const PkString &tag, const PkColor &c)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "qcolor");
    e.setAttribute("value", c.name(PkColor::HexArgb));
}

bool findOnlyElement(const PkXmlElement &parent, const PkString &tag, PkXmlElement *el, PkStringList *errorMessages)
{
    PkXmlNodeList list = parent.elementsByTagName(tag);
    if (list.size() != 1 || !list.at(0).isElement()) {
        const PkString msg = PkString("Could not find \"%1\" XML tag in \"%2\"")
                                 .arg(tag, parent.tagName());
        if (errorMessages) {
            *errorMessages << msg;
        } else {
            warnKrita << msg;
        }

        return false;
    }

    *el = list.at(0).toElement();
    return true;
}

bool removeElements(PkXmlElement &parent, const PkString &tag) {
    PkXmlNodeList list = parent.elementsByTagName(tag);
    KIS_SAFE_ASSERT_RECOVER_NOOP(list.size() <= 1);

    for (int i = 0; i < list.size(); i++) {
        parent.removeChild(list.at(i));
    }

    return list.size() > 0;
}

namespace Private {
    bool checkType(const PkXmlElement &e, const PkString &expectedType)
    {
        PkString type = e.attribute("type", "unknown-type");
        if (type != expectedType) {
            warnKrita << PkString("Error: incorrect type (%2) for value %1. Expected %3")
                             .arg(e.tagName(), type, expectedType);
            return false;
        }

        return true;
    }
}

bool loadValue(const PkXmlElement &e, float *v)
{
    if (!Private::checkType(e, "value")) return false;
    *v = toDouble(e.attribute("value", "0"));
    return true;
}

bool loadValue(const PkXmlElement &e, double *v)
{
    if (!Private::checkType(e, "value")) return false;
    *v = toDouble(e.attribute("value", "0"));
    return true;
}

bool loadValue(const PkXmlElement &e, PkSize *size)
{
    if (!Private::checkType(e, "size")) return false;

    size->setWidth(toInt(e.attribute("w", "0")));
    size->setHeight(toInt(e.attribute("h", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkRect *rc)
{
    if (!Private::checkType(e, "rect")) return false;

    rc->setX(toInt(e.attribute("x", "0")));
    rc->setY(toInt(e.attribute("y", "0")));
    rc->setWidth(toInt(e.attribute("w", "0")));
    rc->setHeight(toInt(e.attribute("h", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkRectF *rc)
{
    if (!Private::checkType(e, "rectf")) return false;

    rc->setX(toInt(e.attribute("x", "0")));
    rc->setY(toInt(e.attribute("y", "0")));
    rc->setWidth(toInt(e.attribute("w", "0")));
    rc->setHeight(toInt(e.attribute("h", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkPoint *pt)
{
    if (!Private::checkType(e, "point")) return false;

    pt->setX(toInt(e.attribute("x", "0")));
    pt->setY(toInt(e.attribute("y", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkPointF *pt)
{
    if (!Private::checkType(e, "pointf")) return false;

    pt->setX(toDouble(e.attribute("x", "0")));
    pt->setY(toDouble(e.attribute("y", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkVector3D *pt)
{
    if (!Private::checkType(e, "vector3d")) return false;

    pt->setX(toDouble(e.attribute("x", "0")));
    pt->setY(toDouble(e.attribute("y", "0")));
    pt->setZ(toDouble(e.attribute("z", "0")));

    return true;
}

bool loadValue(const PkXmlElement &e, PkTransform *t)
{
    if (!Private::checkType(e, "transform")) return false;

    qreal m11 = toDouble(e.attribute("m11", "1.0"));
    qreal m12 = toDouble(e.attribute("m12", "0.0"));
    qreal m13 = toDouble(e.attribute("m13", "0.0"));

    qreal m21 = toDouble(e.attribute("m21", "0.0"));
    qreal m22 = toDouble(e.attribute("m22", "1.0"));
    qreal m23 = toDouble(e.attribute("m23", "0.0"));

    qreal m31 = toDouble(e.attribute("m31", "0.0"));
    qreal m32 = toDouble(e.attribute("m32", "0.0"));
    qreal m33 = toDouble(e.attribute("m33", "1.0"));

    t->setMatrix(
        m11, m12, m13,
        m21, m22, m23,
        m31, m32, m33);

    return true;
}

bool loadValue(const PkXmlElement &e, PkString *value)
{
    if (!Private::checkType(e, "value")) return false;
    *value = e.attribute("value", "no-value");
    return true;
}

bool loadValue(const PkXmlElement &e, PkColor *value)
{
    if (!Private::checkType(e, "qcolor")) return false;
    value->setNamedColor(e.attribute("value", "#FFFF0000"));
    return true;
}

PkXmlElement findElementByAttribute(PkXmlNode parent,
                                   const PkString &tag,
                                   const PkString &attribute,
                                   const PkString &value)
{
    PkXmlElement e;
    for (e = parent.firstChildElement(tag); !e.isNull(); e = e.nextSiblingElement(tag)) {
        if (value == e.attribute(attribute, "<undefined>")) {
            return e;
        }
    }

    return PkXmlElement();
}

}
