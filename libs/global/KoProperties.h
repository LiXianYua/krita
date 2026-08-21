/*
   SPDX-FileCopyrightText: 2006-2007 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef _KO_PROPERTIES_H
#define _KO_PROPERTIES_H

#include <PkString.h>
#include <PkGlobal.h>
#include <PkMap.h>
#include <PkMapIterator.h>
#include <PkVariant.h>
#include "kritaglobal_export.h"

class PkXmlElement;

/**
 * A KoProperties is the (de-)serializable representation of
 * a key-value map. The serialisation format is XML.
 */
class KRITAGLOBAL_EXPORT KoProperties
{
public:

    /**
     * Create a new properties object
     */
    KoProperties();

    /**
     * Copy constructor
     */
    KoProperties(const KoProperties &other);

    ~KoProperties();

public:

    /**
     * Fill the properties object from the XML dom node.
     *
     * load() does not touch existing properties if loading fails.
     *
     * @param root the root node of the properties subtree.
     */
    void load(const PkXmlElement &root);

    /**
     * Fill the properties object from the XML encoded
     * representation in string.
     *
     * load() does not touch existing properties if loading fails.
     *
     * @param string the stored properties.
     * @return false if loading failing, true if it succeeded
     */
    bool load(const PkString &string);

    /**
     * Returns an iterator over the properties. The iterator is not
     * suitable for adding or removing properties.
     */
    PkMapIterator<PkString, PkVariant> propertyIterator() const;

    /**
     * @return true if this KoProperties object does not contain any
     * properties.
     */
    bool isEmpty() const;

    /**
     * @brief Create a serialized version of these properties (as XML) with root as the root element.
     * @param root as the root element in the generated XML.
     */
    PkString store(const PkString &root) const;

    void save(PkXmlElement &root) const;

    /**
     * Set the property with name to value.
     */
    void setProperty(const PkString &name, const PkVariant &value);

    /**
     * Set value to the value associated with property name
     * @return false if the specified property did not exist.
     */
    bool property(const PkString &name, PkVariant &value) const;

    /**
     * Return a property by name, wrapped in a PkVariant.
     * A typical usage:
     *  @code
     *      KoProperties *props = new KoProperties();
     *      props->setProperty("name", "Marcy");
     *      props->setProperty("age", 25);
     *      PkString name = props->property("name").toString();
     *      int age = props->property("age").toInt();
     *  @endcode
     * @return a property by name, wrapped in a PkVariant.
     * @param name the name (or key) with which the variant was registered.
     * @see intProperty() stringProperty()
     */
    PkVariant property(const PkString &name) const;

    /**
     * Return an integer property by name.
     * A typical usage:
     *  @code
     *      KoProperties *props = new KoProperties();
     *      props->setProperty("age", 25);
     *      int age = props->intProperty("age");
     *  @endcode
     * @return an integer property by name
     * @param name the name (or key) with which the variant was registered.
     * @param defaultValue the default value, should there not be any property by the name this will be returned.
     * @see property() stringProperty()
     */
    int intProperty(const PkString &name, int defaultValue = 0) const;

    /**
     * Return a qreal property by name.
     * @param name the name (or key) with which the variant was registered.
     * @param defaultValue the default value, should there not be any property by the name this will be returned.
     */
    qreal doubleProperty(const PkString &name, qreal defaultValue = 0.0) const;

    /**
     * Return a boolean property by name.
     * @param name the name (or key) with which the variant was registered.
     * @param defaultValue the default value, should there not be any property by the name this will be returned.
     */
    bool boolProperty(const PkString &name, bool defaultValue = false) const;

    /**
     * Return an PkString property by name.
     * A typical usage:
     *  @code
     *      KoProperties *props = new KoProperties();
     *      props->setProperty("name", "Marcy");
     *      PkString name = props->stringProperty("name");
     *  @endcode
     * @return an PkString property by name
     * @param name the name (or key) with which the variant was registered.
     * @see property() intProperty()
     * @param defaultValue the default value, should there not be any property by the name this will be returned.
     */
    PkString stringProperty(const PkString &name, const PkString &defaultValue = PkString()) const;

    /**
     * Returns true if the specified key is present in this properties
     * object.
     */
    bool contains(const PkString &key) const;

    /**
     * Returns the value associated with the specified key if this
     * properties object contains the specified key; otherwise return
     * an empty PkVariant.
     */
    PkVariant value(const PkString &key) const;

    bool operator==(const KoProperties &other) const;

private:

    class Private;
    Private * const d;
};

#endif // _KO_PROPERTIES_H
