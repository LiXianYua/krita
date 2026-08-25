/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_PROPERTIES_CONFIGURATION_H_
#define _KIS_PROPERTIES_CONFIGURATION_H_

#include <PkString.h>
#include <pk/container/PkMap.h>
#include <PkVariant.h>
#include <pk/container/PkSet.h>
#include <pk/container/PkList.h>
#include <pk/container/PkStringList.h>
#include <kis_debug.h>
#include <kis_cubic_curve.h>
#include <KoColor.h>

class PkXmlElement;
class PkXmlDocument;

#include "kis_serializable_configuration.h"
#include "kritaimage_export.h"
#include "kis_types.h"


/**
 * KisPropertiesConfiguration is a map-based properties class that can
 * be serialized and deserialized.
 *
 * It differs from the base class KisSerializableConfiguration in that
 * it provides a number of convenience methods to get at the data and
 */
class KRITAIMAGE_EXPORT KisPropertiesConfiguration : public KisSerializableConfiguration
{

public:

    /**
     * Create a new properties  config.
     */
    KisPropertiesConfiguration();
    ~KisPropertiesConfiguration() override;

    /**
     * Deep copy the properties \p rhs
     */
    KisPropertiesConfiguration(const KisPropertiesConfiguration& rhs);

    /**
     * Deep copy the properties \p rhs
     */
    KisPropertiesConfiguration& operator=(const KisPropertiesConfiguration& rhs);

public:


    /**
     * Fill the properties  configuration object from the XML encoded representation in s.
     * This function use the "Legacy" style XML of the 1.x .kra file format.
     * @param xml the string that will be parsed as xml
     * @param clear if true, the properties map will be emptied.
     * @return true is the xml document could be parsed
     */
    bool fromXML(const PkString& xml, bool clear = true) override;

    /**
     * Fill the properties  configuration object from the XML encoded representation in s.
     * This function use the "Legacy" style XML  of the 1.x .kra file format.
     *
     * Note: the existing properties will not be cleared
     */
    void fromXML(const PkXmlElement&) override;

    /**
     * Create a serialized version of this properties  config
     * This function use the "Legacy" style XML  of the 1.x .kra file format.
     */
    void toXML(PkXmlDocument&, PkXmlElement&) const override;

    /**
     * Create a serialized version of this properties  config
     * This function use the "Legacy" style XML  of the 1.x .kra file format.
     */
    PkString toXML() const override;

    /**
     * @return true if the map contains a property with the specified name
     */
    virtual bool hasProperty(const PkString& name) const;

    /**
     * Set the property with name to value.
     */
    virtual void setProperty(const PkString & name, const PkVariant & value);

    /**
     * Set value to the value associated with property name
     *
     * XXX: API alert: a setter that is prefixed with get?
     *
     * @return false if the specified property did not exist.
     */
    virtual bool getProperty(const PkString & name, PkVariant & value) const;

    virtual PkVariant getProperty(const PkString & name) const;

    template <typename T>
        T getPropertyLazy(const PkString & name, const T &defaultValue) const {
        PkVariant value = getProperty(name);
        return value.isValid() ? value.value<T>() : defaultValue;
    }

    PkString getPropertyLazy(const PkString & name, const char *defaultValue) const {
        return getPropertyLazy(name, PkString(defaultValue));
    }

    int getInt(const PkString & name, int def = 0) const;

    double getDouble(const PkString & name, double def = 0.0) const;

    float getFloat(const PkString& name, float def = 0.0) const;

    bool getBool(const PkString & name, bool def = false) const;

    PkString getString(const PkString & name, const PkString & def = PkString()) const;

    KisCubicCurve getCubicCurve(const PkString & name, const KisCubicCurve & curve = KisCubicCurve()) const;

    /**
     * @brief getColor fetch the given property as a KoColor.
     *
     * The color can be stored as
     * <ul>
     * <li>A KoColor
     * <li>A PkColor
     * <li>A string that can be parsed as an XML color definition
     * <li>A string that PkColor can convert to a color (see PkColor::fromString)
     * <li>An integer that PkColor can convert to a color
     * </ul>
     *
     * @param name the name of the property
     * @param color the default value to be returned if the @param name does not exist.
     * @return returns the named property as a KoColor if the value can be converted to a color,
     * otherwise a empty KoColor is returned.
     */
    KoColor getColor(const PkString& name, const KoColor& color = KoColor()) const;

    virtual PkMap<PkString, PkVariant> getProperties() const;

    /// Clear the map of properties
    void clearProperties();

    /// Marks a property that should not be saved by toXML
    void setPropertyNotSaved(const PkString & name);

    void removeProperty(const PkString & name);

    /**
     * Get the keys of all the properties in the object
     */
    virtual PkList<PkString> getPropertiesKeys() const;

    /**
     * Get a set of properties, which keys are prefixed with \p prefix. The settings object
     * \p config will have all these properties with the prefix stripped from them.
     */
    void getPrefixedProperties(const PkString &prefix, KisPropertiesConfiguration *config) const;

    /**
     * A convenience override
     */
    void getPrefixedProperties(const PkString &prefix, KisPropertiesConfigurationSP config) const;

    /**
     * Takes all the properties from \p config, adds \p prefix to all their keys and puts them
     * into this properties object
     */
    void setPrefixedProperties(const PkString &prefix, const KisPropertiesConfiguration *config);

    /**
     * A convenience override
     */
    void setPrefixedProperties(const PkString &prefix, const KisPropertiesConfigurationSP config);

    /**
     * After calling `getPropertiesConfiguration()` the resulting properties
     * will contain the prefix they were packed with. The prefix can be requested
     * with the key returned by `extractedPrefixKey()` function.
     */
    static PkString extractedPrefixKey();

    static PkString escapeString(const PkString &string);
    static PkString unescapeString(const PkString &string);

    void setProperty(const PkString &name, const PkStringList &value);
    PkStringList getStringList(const PkString &name, const PkStringList &defaultValue = PkStringList()) const;
    PkStringList getPropertyLazy(const PkString &name, const PkStringList &defaultValue) const;

    /**
     * Structural comparison between two instances.
     */
    virtual bool compareTo(const KisPropertiesConfiguration* rhs) const;

public:

    virtual void dump() const;

private:

    struct Private;
    Private* const d;
};

class KRITAIMAGE_EXPORT KisPropertiesConfigurationFactory : public KisSerializableConfigurationFactory
{
public:
    KisPropertiesConfigurationFactory();
    ~KisPropertiesConfigurationFactory() override;
    KisSerializableConfigurationSP createDefault() override;
    KisSerializableConfigurationSP create(const PkXmlElement& e) override;
private:
    struct Private;
    Private* const d;
};

#endif
