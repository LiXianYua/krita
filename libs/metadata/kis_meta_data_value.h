/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _KIS_META_DATA_VALUE_H_
#define _KIS_META_DATA_VALUE_H_

#include <PkDebug.h>
#include <PkGlobal.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkString.h>

#include <kritametadata_export.h>
#include <boost/operators.hpp>

class PkVariant;

namespace KisMetaData
{

struct Rational : public boost::equality_comparable<Rational>
{
    explicit Rational(qint32 n = 0, qint32 d = 1) : numerator(n), denominator(d) {}
    qint32 numerator;
    qint32 denominator;
    bool operator==(const Rational& ur) const {
        return numerator == ur.numerator && denominator == ur.denominator;
    }
};

/**
 * Value is build on top of PkVariant to extend it to support the various types
 * and extensions through property qualifiers.
 */
class KRITAMETADATA_EXPORT Value
{
    struct Private;
public:
    /// Define the possible value type
    enum ValueType {
        Invalid,
        Variant,
        OrderedArray,
        UnorderedArray,
        AlternativeArray,
        LangArray,
        Structure,
        Rational
    };
public:
    Value();
    Value(const PkVariant& value);
    /**
    * @param type is one of OrderedArray, UnorderedArray, AlternativeArray
    * or LangArray
    */
    Value(const PkList<Value>& array, ValueType type = OrderedArray);
    Value(const PkMap<PkString, Value>& structure);
    Value(const KisMetaData::Rational& rational);
    Value(const Value& v);
    Value& operator=(const Value& v);
    ~Value();
public:
    void addPropertyQualifier(const PkString& _name, const Value&);
    const PkMap<PkString, Value>& propertyQualifiers() const;
public:
    /// @return the type of this Value
    ValueType type() const;
    /**
    * @return the value as a double, or null if it's not possible, rationals are evaluated
    */
    double asDouble() const;
    /**
    * @return the value as an integer, or null if it's not possible, rationals are evaluated
    */
    int asInteger() const;
    /**
    * @return the Variant hold by this Value, or an empty PkVariant if this Value is not a Variant
    */
    PkVariant asVariant() const;
    /**
    * Set this Value to the given variant, or does nothing if this Value is not a Variant.
    * @return true if the value was changed
    */
    bool setVariant(const PkVariant& variant);
    bool setStructureVariant(const PkString& fieldNAme, const PkVariant& variant);
    bool setArrayVariant(int index, const PkVariant& variant);
    /**
    * @return the Rational hold by this Value, or a null rational if this Value is not
    * an Rational
    */
    KisMetaData::Rational asRational() const;
    /**
    * @return the array hold by this Value, or an empty array if this Value is not either
    * an OrderedArray, UnorderedArray or AlternativeArray
    */
    PkList<KisMetaData::Value> asArray() const;
    /**
    * @return true if this Value is either an OrderedArray, UnorderedArray or AlternativeArray
    */
    bool isArray() const;
    /**
    * @return the structure hold by this Value, or an empty structure if this Value is not a Structure
    */
    PkMap<PkString, KisMetaData::Value> asStructure() const;
    /**
    * It's a convenient function that build a map from a LangArray using the property
    * qualifier "xml:lang" for the key of the map.
    */
    PkMap<PkString, KisMetaData::Value> asLangArray() const;
    PkString toString() const;
public:
    bool operator==(const Value&) const;
    Value& operator+=(const Value&);
private:
    Private* const d;
};
}


KRITAMETADATA_EXPORT PkDebug operator<<(PkDebug debug, const KisMetaData::Value &v);

#endif
