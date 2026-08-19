/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_DOM_UTILS_H
#define __KIS_DOM_UTILS_H

#include <float.h>

#include <PkPoint.h>
#include <PkXmlElement.h>

#include <PkStream.h>

#include "kis_global.h"

#include "kritaglobal_export.h"
#include "kis_debug.h"
#include "krita_container_utils.h"
#include "KisPortingUtils.h"

class PkVector3D;

namespace KisDomUtils {

    inline PkString toString(const PkString &value) {
        return value;
    }

    template<typename T>
        inline PkString toString(T value) {
        return PkString::number(value);
    }

    inline PkString toString(float value) {
        PkString str;
        PkTextStream stream;
        KisPortingUtils::setUtf8OnStream(stream);
        stream.setString(&str, PkStream::WriteOnly);
        stream.setRealNumberPrecision(FLT_DIG);
        stream << value;
        return str;
    }

    inline PkString toString(double value) {
        PkString str;
        PkTextStream stream;
        KisPortingUtils::setUtf8OnStream(stream);
        stream.setString(&str, PkStream::WriteOnly);
        stream.setRealNumberPrecision(15);
        stream << value;
        return str;
    }

    inline int toInt(const PkString &str, bool *ok=nullptr) {
        bool ok_locale = false;
        int value = 0;

        PkLocale c(PkLocale::German);

        value = str.toInt(&ok_locale);
        if (!ok_locale) {
            value = c.toInt(str, &ok_locale);
        }

        if (!ok_locale && ok == nullptr) {
            warnKrita << "WARNING: KisDomUtils::toInt failed:" << ppVar(str);
            value = 0;
        }

        if (ok != nullptr) {
            *ok = ok_locale;
        }

        return value;
    }

    inline double toDouble(const PkString &str, bool *ok=nullptr) {
        bool ok_locale = false;
        double value = 0;

        PkLocale c(PkLocale::German);

        /**
         * A special workaround to handle ','/'.' decimal point
         * in different locales. Added for backward compatibility,
         * because we used to save qreals directly using
         *
         * e.setAttribute("w", (qreal)value),
         *
         * which did local-aware conversion.
         */

        value = str.toDouble(&ok_locale);
        if (!ok_locale) {
            value = c.toDouble(str, &ok_locale);
        }

        if (!ok_locale && ok == nullptr) {
            warnKrita << "WARNING: KisDomUtils::toDouble failed:" << ppVar(str);
            value = 0.0;
        }

        if (ok != nullptr) {
            *ok = ok_locale;
        }

        return value;
    }

    inline PkString qColorToQString(PkColor color)
    {
        // color channels will usually have 0-255
        PkString customColor = PkString::number(color.red()).append(",")
                             .append(PkString::number(color.green())).append(",")
                             .append(PkString::number(color.blue())).append(",")
                             .append(PkString::number(color.alpha()));

        return customColor;
    }

    inline PkColor qStringToQColor(PkString colorString)
    {
        PkStringList colorComponents = colorString.split(',');
        return PkColor(colorComponents[0].toInt(), colorComponents[1].toInt(), colorComponents[2].toInt(), colorComponents[3].toInt());
    }

/**
 * Save a value of type PkRect into an XML tree. A child for \p parent
 * is created and assigned a tag \p tag.  The corresponding value can
 * be fetched from the XML using loadValue() later.
 *
 * \see loadValue()
 */
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkRect &rc);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkRectF &rc);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkSize &size);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkPoint &pt);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkPointF &pt);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkVector3D &pt);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkTransform &t);
void KRITAGLOBAL_EXPORT saveValue(PkXmlElement *parent, const PkString &tag, const PkColor &t);

/**
 * Save a value of a scalar type into an XML tree. A child for \p parent
 * is created and assigned a tag \p tag.  The corresponding value can
 * be fetched from the XML using loadValue() later.
 *
 * \see loadValue()
 */
template <typename T>
void saveValue(PkXmlElement *parent, const PkString &tag, T value)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "value");
    e.setAttribute("value", toString(value));
}

/**
 * Save a vector of values into an XML tree. A child for \p parent is
 * created and assigned a tag \p tag.  The values in the array should
 * have a type supported by saveValue() overrides. The corresponding
 * vector can be fetched from the XML using loadValue() later.
 *
 * \see loadValue()
 */
template <template <class...> class Container, typename T, typename ...Args>
typename std::enable_if<KritaUtils::is_container<Container<T, Args...>>::value, void>::type
saveValue(PkXmlElement *parent, const PkString &tag, const Container<T, Args...> &array)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "array");

    int i = 0;
    Q_FOREACH (const T &v, array) {
        saveValue(&e, PkString("item_%1").arg(i++), v);
    }
}

/**
 * Find an element with tag \p tag which is a child of \p parent. The element should
 * be the only element with the provided tag in this parent.
 *
 * \return true is the element with \p tag is found and it is unique
 */
bool KRITAGLOBAL_EXPORT findOnlyElement(const PkXmlElement &parent, const PkString &tag, PkXmlElement *el, PkStringList *errorMessages = 0);

/**
 * Load an object from an XML element, which is a child of \p parent and has
 * a tag \p tag.
 *
 * \return true if the object is successfully loaded and is unique
 *
 * \see saveValue()
 */
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, float *v);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, double *v);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkSize *size);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkRect *rc);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkRectF *rc);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkPoint *pt);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkPointF *pt);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkVector3D *pt);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkTransform *t);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkString *value);
bool KRITAGLOBAL_EXPORT loadValue(const PkXmlElement &e, PkColor *value);

namespace Private {
    bool KRITAGLOBAL_EXPORT checkType(const PkXmlElement &e, const PkString &expectedType);
}

/**
 * Load a scalar value from an XML element, which is a child of \p parent
 * and has a tag \p tag.
 *
 * \return true if the object is successfully loaded and is unique
 *
 * \see saveValue()
 */
template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
loadValue(const PkXmlElement &e, T *value)
{
    if (!Private::checkType(e, "value")) return false;

    PkVariant v(e.attribute("value", "no-value"));
    *value = v.value<T>();
    return true;
}

/**
 * A special adapter method that makes vector- and tag-based methods
 * work with environment parameter uniformly.
 */
template <typename T, typename E>
    typename std::enable_if<std::is_empty<E>::value, bool>::type
loadValue(const PkXmlElement &parent, T *value, const E &/*env*/) {
    return loadValue(parent, value);
}

/**
 * Load an array from an XML element, which is a child of \p parent
 * and has a tag \p tag.
 *
 * \return true if the object is successfully loaded and is unique
 *
 * \see saveValue()
 */

template <template <class ...> class Container, typename T, typename E, typename ...Args>
typename std::enable_if<KritaUtils::is_appendable_container<Container<T, Args...>>::value, bool>::type
loadValue(const PkXmlElement &e, Container<T, Args...> *array, const E &env = std::tuple<>())
{
    if (!Private::checkType(e, "array")) return false;

    PkXmlElement child = e.firstChildElement();
    while (!child.isNull()) {
        T value;
        if (!loadValue(child, &value, env)) return false;
        array->push_back(value);
        child = child.nextSiblingElement();
    }
    return true;
}

template <template <class ...> class Container, typename T, typename E, typename F, typename ...Args>
typename std::enable_if<KritaUtils::is_appendable_container<Container<T, Args...>>::value, bool>::type
loadValue(const PkXmlElement &e, Container<T, Args...> *array, const E &env1, const F &env2)
{
    if (!Private::checkType(e, "array")) return false;

    PkXmlElement child = e.firstChildElement();
    while (!child.isNull()) {
        T value;
        if (!loadValue(child, &value, env1, env2)) return false;
        array->push_back(value);
        child = child.nextSiblingElement();
    }
    return true;
}

template <typename T, typename E = std::tuple<>>
    bool loadValue(const PkXmlElement &parent, const PkString &tag, T *value, const E &env = E())
{
    PkXmlElement e;
    if (!findOnlyElement(parent, tag, &e)) return false;

    return loadValue(e, value, env);
}

template <typename T, typename E, typename F>
    bool loadValue(const PkXmlElement &parent, const PkString &tag, T *value, const E &env1, const F &env2)
{
    PkXmlElement e;
    if (!findOnlyElement(parent, tag, &e)) return false;

    return loadValue(e, value, env1, env2);
}

KRITAGLOBAL_EXPORT PkXmlElement findElementByAttribute(PkXmlNode parent,
                                                      const PkString &tag,
                                                      const PkString &attribute,
                                                      const PkString &value);

KRITAGLOBAL_EXPORT bool removeElements(PkXmlElement &parent, const PkString &tag);

}

#endif /* __KIS_DOM_UTILS_H */
