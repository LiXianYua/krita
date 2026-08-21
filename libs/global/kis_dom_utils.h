/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_DOM_UTILS_H
#define __KIS_DOM_UTILS_H

#include <float.h>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <system_error>
#include <type_traits>

#include <PkPoint.h>
#include <PkColor.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include "kis_global.h"

#include "kritaglobal_export.h"
#include "kis_debug.h"
#include "krita_container_utils.h"

class PkVector3D;

namespace KisDomUtils {

    template<typename T>
    inline PkString numberToString(T value, int precision = -1) {
        char buffer[64];
        std::to_chars_result result;

        if constexpr (std::is_floating_point<T>::value) {
            result = std::to_chars(buffer, buffer + sizeof(buffer), value,
                                   std::chars_format::general, precision);
        } else if constexpr (std::is_same<T, bool>::value) {
            buffer[0] = value ? '1' : '0';
            result = {buffer + 1, std::errc()};
        } else {
            result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        }

        if (result.ec != std::errc()) {
            return PkString();
        }
        return PkString::PkFromUtf8(buffer, static_cast<int>(result.ptr - buffer));
    }

    inline PkString toString(const PkString &value) {
        return value;
    }

    template<typename T>
        inline PkString toString(T value) {
        return numberToString(value);
    }

    inline PkString toString(float value) {
        return numberToString(value, FLT_DIG);
    }

    inline PkString toString(double value) {
        return numberToString(value, 15);
    }

    inline int toInt(const PkString &str, bool *ok=nullptr) {
        bool ok_locale = false;
        int value = 0;

        value = str.toInt(&ok_locale);
        if (!ok_locale) {
            const std::string text = str.PkToUtf8();
            auto begin = text.begin();
            auto end = text.end();
            while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
                ++begin;
            }
            while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
                --end;
            }

            std::string normalized;
            auto group = begin;
            if (group != end && (*group == '+' || *group == '-')) {
                normalized.push_back(*group++);
            }

            const auto firstSeparator = std::find(group, end, '.');
            const auto allDigits = [](auto first, auto last) {
                return first != last && std::all_of(first, last, [](char character) {
                    return character >= '0' && character <= '9';
                });
            };

            bool validGrouping = firstSeparator != end &&
                                 firstSeparator - group >= 1 &&
                                 firstSeparator - group <= 3 &&
                                 allDigits(group, firstSeparator);
            if (validGrouping) {
                normalized.append(group, firstSeparator);
                group = firstSeparator;
                while (group != end && validGrouping) {
                    const auto nextSeparator = std::find(group + 1, end, '.');
                    const auto groupEnd = nextSeparator == end ? end : nextSeparator;
                    validGrouping = groupEnd - (group + 1) == 3 &&
                                    allDigits(group + 1, groupEnd);
                    if (validGrouping) {
                        normalized.append(group + 1, groupEnd);
                        group = groupEnd;
                    }
                }
                validGrouping = validGrouping && group == end;
            }

            if (validGrouping) {
                value = PkString::PkFromUtf8(normalized.data(),
                                             static_cast<int>(normalized.size()))
                            .toInt(&ok_locale);
            }
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
            std::string decimal = str.PkToUtf8();
            const auto decimalComma = std::find(decimal.begin(), decimal.end(), ',');
            if (decimalComma != decimal.end()) {
                decimal.erase(std::remove(decimal.begin(), decimal.end(), '.'), decimal.end());
                std::replace(decimal.begin(), decimal.end(), ',', '.');
            }
            value = PkString::PkFromUtf8(decimal.data(), static_cast<int>(decimal.size()))
                        .toDouble(&ok_locale);
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
        PkString customColor = toString(color.red()).append(",")
                             .append(toString(color.green())).append(",")
                             .append(toString(color.blue())).append(",")
                             .append(toString(color.alpha()));

        return customColor;
    }

    inline PkColor qStringToQColor(PkString colorString)
    {
        const std::vector<PkString> colorComponents = colorString.split(',');
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
    for (const T &v : array) {
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

    const PkString serialized = e.attribute("value", "no-value");
    if constexpr (std::is_same<T, bool>::value) {
        *value = serialized == "true" || serialized == "1";
    } else if constexpr (std::is_integral<T>::value) {
        *value = static_cast<T>(toInt(serialized));
    } else {
        *value = static_cast<T>(toDouble(serialized));
    }
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
