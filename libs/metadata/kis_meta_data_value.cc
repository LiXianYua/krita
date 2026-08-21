/*
 *  SPDX-FileCopyrightText: 2007, 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_value.h"

#include <PkDebug.h>
#include <PkDateTime.h>
#include <PkPoint.h>
#include <PkStringList.h>
#include <PkVariant.h>

#include <algorithm>

#include <kis_debug.h>

using namespace KisMetaData;

struct Value::Private {
    Private() : type(Invalid) {}
    union {
        PkVariant* variant;
        PkList<Value>* array;
        PkMap<PkString, Value>* structure;
        KisMetaData::Rational* rational;
    } value;
    ValueType type;
    PkMap<PkString, Value> propertyQualifiers;
};

Value::Value() : d(new Private)
{
    d->type = Invalid;
}


Value::Value(const PkVariant& variant) : d(new Private)
{
    d->type = Value::Variant;
    d->value.variant = new PkVariant(variant);
}

Value::Value(const PkList<Value>& array, ValueType type) : d(new Private)
{
    Q_ASSERT(type == OrderedArray || type == UnorderedArray || type == AlternativeArray || type == LangArray);
    d->value.array = new PkList<Value>(array);
    d->type = type; // TODO: I am hesitating about LangArray to keep them as array or convert them to maps
}

Value::Value(const PkMap<PkString, Value>& structure) : d(new Private)
{
    d->type = Structure;
    d->value.structure = new PkMap<PkString, Value>(structure);
}

Value::Value(const KisMetaData::Rational& signedRational) : d(new Private)
{
    d->type = Value::Rational;
    d->value.rational = new KisMetaData::Rational(signedRational);
}


Value::Value(const Value& v) : d(new Private)
{
    d->type = Invalid;
    *this = v;
}

Value& Value::operator=(const Value & v)
{
    d->type = v.d->type;
    d->propertyQualifiers = v.d->propertyQualifiers;
    switch (d->type) {
    case Invalid:
        break;
    case Variant:
        d->value.variant = new PkVariant(*v.d->value.variant);
        break;
    case OrderedArray:
    case UnorderedArray:
    case AlternativeArray:
    case LangArray:
        d->value.array = new PkList<Value>(*v.d->value.array);
        break;
    case Structure:
        d->value.structure = new PkMap<PkString, Value>(*v.d->value.structure);
        break;
    case Rational:
        d->value.rational = new KisMetaData::Rational(*v.d->value.rational);
    }
    return *this;
}


Value::~Value()
{
    delete d;
}

void Value::addPropertyQualifier(const PkString& _name, const Value& _value)
{
    d->propertyQualifiers[_name] = _value;
}

const PkMap<PkString, Value>& Value::propertyQualifiers() const
{
    return d->propertyQualifiers;
}

Value::ValueType Value::type() const
{
    return d->type;
}

double Value::asDouble() const
{
    switch (type()) {
    case Variant:
        return d->value.variant->toDouble();
    case Rational:
        return d->value.rational->numerator / (double)d->value.rational->denominator;
    default:
        return 0.0;
    }
    return 0.0;
}

int Value::asInteger() const
{
    switch (type()) {
    case Variant:
        return d->value.variant->toInt();
    case Rational:
        return d->value.rational->numerator / d->value.rational->denominator;
    default:
        return 0;
    }
    return 0;
}

PkVariant Value::asVariant() const
{
    switch (type()) {
    case Variant:
        return *d->value.variant;
    case Rational:
        return PkVariant(PkString("%1 / %2").arg(d->value.rational->numerator).arg(d->value.rational->denominator));
    default: break;
    }
    return PkVariant();
}

bool Value::setVariant(const PkVariant& variant)
{
    switch (type()) {
    case KisMetaData::Value::Invalid:
        *this = KisMetaData::Value(variant);
        return true;
    case Rational: {
        // TODO: erm... did someone forgot to write actual code here?

        // for now just safe assert and return a failure
        KIS_SAFE_ASSERT_RECOVER_NOOP(0 && "Rational metadata values are not implemented!");
        return false;
    }
    case KisMetaData::Value::Variant: {
        if (d->value.variant->type() == variant.type()) {
            *d->value.variant = variant;
            return true;
        }
    }
    return true;
    default:
        break;
    }
    return false;
}

bool Value::setStructureVariant(const PkString& fieldNAme, const PkVariant& variant)
{
    if (type() == Structure) {
        return (*d->value.structure)[fieldNAme].setVariant(variant);
    }
    return false;
}

bool Value::setArrayVariant(int index, const PkVariant& variant)
{
    if (isArray()) {
        for (int i = d->value.array->size(); i <= index; ++i) {
            d->value.array->append(Value());
        }
        (*d->value.array)[index].setVariant(variant);
    }
    return false;
}

KisMetaData::Rational Value::asRational() const
{
    if (d->type == Rational) {
        return *d->value.rational;
    }
    return KisMetaData::Rational();
}

PkList<Value> Value::asArray() const
{
    if (isArray()) {
        return *d->value.array;
    }
    return PkList<Value>();
}


bool Value::isArray() const
{
    return type() == OrderedArray || type() == UnorderedArray || type() == AlternativeArray;
}

PkMap<PkString, KisMetaData::Value> Value::asStructure() const
{
    if (type() == Structure) {
        return *d->value.structure;
    }
    return PkMap<PkString, KisMetaData::Value>();
}

PkDebug operator<<(PkDebug debug, const Value &v)
{
    switch (v.type()) {
    case Value::Invalid:
        debug.nospace() << "invalid value";
        break;
    case Value::Variant:
        debug.nospace() << "Variant: " << v.asVariant().toString();
        break;
    case Value::OrderedArray:
    case Value::UnorderedArray:
    case Value::AlternativeArray:
    case Value::LangArray:
        debug.nospace() << "Array: " << v.toString();
        break;
    case Value::Structure:
        debug.nospace() << "Structure: " << v.toString();
        break;
    case Value::Rational:
        debug.nospace() << "Rational: " << v.asRational().numerator << " / " << v.asRational().denominator;
        break;
    }
    return debug.space();
}

bool Value::operator==(const Value& rhs) const
{
    if (d->type != rhs.d->type) return false;
    switch (d->type) {
    case Value::Invalid:
        return true;
    case Value::Variant:
        return asVariant() == rhs.asVariant();
    case Value::OrderedArray:
    case Value::UnorderedArray:
    case Value::AlternativeArray:
    case Value::LangArray:
        return asArray() == rhs.asArray();
    case Value::Structure:
        return asStructure() == rhs.asStructure();
    case Value::Rational:
        return asRational() == rhs.asRational();
    }
    return false;
}

Value& Value::operator+=(const Value & v)
{
    switch (d->type) {
    case Value::Invalid:
        Q_ASSERT(v.type() == Value::Invalid);
        break;
    case Value::Variant:
        Q_ASSERT(v.type() == Value::Variant);
        {
            PkVariant v1 = *d->value.variant;
            PkVariant v2 = *v.d->value.variant;
            Q_ASSERT(pkCanConvert(v2.type(), v1.type()));
            switch (v1.type()) {
            default:
                warnMetaData << "KisMetaData: Merging metadata of type" << v1.type() << "is unsupported!";
                break;
            case PkVariant::Date:
                *d->value.variant = PkVariant(std::max(v1.toDate(), v2.toDate()));
                break;
            case PkVariant::DateTime:
                *d->value.variant = PkVariant(std::max(v1.toDate(), v2.toDate()));
                break;
            case PkVariant::Double:
                *d->value.variant = PkVariant(v1.toDouble() + v2.toDouble());
                break;
            case PkVariant::Int:
                *d->value.variant = PkVariant(v1.toInt() + v2.toInt());
                break;
            case PkVariant::List: {
                PkVariantList l1 = v1.toList();
                const PkVariantList l2 = v2.toList();
                l1.insert(l1.end(), l2.begin(), l2.end());
                *d->value.variant = PkVariant(l1);
            }
            break;
            case PkVariant::LongLong:
                *d->value.variant = PkVariant(v1.toLongLong() + v2.toLongLong());
                break;
            case PkVariant::Point:
                *d->value.variant = PkVariant(v1.toPoint() + v2.toPoint());
                break;
            case PkVariant::PointF:
                *d->value.variant = PkVariant(v1.toPointF() + v2.toPointF());
                break;
            case PkVariant::String:
                *d->value.variant = PkVariant(v1.toString() + v2.toString());
                break;
            case PkVariant::StringList: {
                PkStringList sl = v1.toStringList();
                sl += v2.toStringList();
                *d->value.variant = PkVariant(sl);
            }
            break;
            case PkVariant::Time: {
                PkTime t1 = v1.toTime();
                PkTime t2 = v2.toTime();
                int h = t1.hour() + t2.hour();
                int m = t1.minute() + t2.minute();
                int s = t1.second() + t2.second();
                int ms = t1.msec() + t2.msec();
                if (ms > 999) {
                    ms -= 999; s++;
                }
                if (s > 60) {
                    s -= 60; m++;
                }
                if (m > 60) {
                    m -= 60; h++;
                }
                if (h > 24) {
                    h -= 24;
                }
                *d->value.variant = PkVariant(PkTime(h, m, s, ms));
            }
            break;
            case PkVariant::UInt:
                *d->value.variant = PkVariant(v1.toUInt() + v2.toUInt());
                break;
            case PkVariant::ULongLong:
                *d->value.variant = PkVariant(v1.toULongLong() + v2.toULongLong());
                break;
            }

        }
        break;
    case Value::OrderedArray:
    case Value::UnorderedArray:
    case Value::AlternativeArray: {
        if (v.isArray()) {
            *(d->value.array) += *(v.d->value.array);
        } else {
            d->value.array->append(v);
        }
    }
    break;
    case Value::LangArray: {
        Q_ASSERT(v.type() == Value::LangArray);
    }
    break;
    case Value::Structure: {
        Q_ASSERT(v.type() == Value::Structure);
        break;
    }
    case Value::Rational: {
        Q_ASSERT(v.type() == Value::Rational);
        d->value.rational->numerator =
            (d->value.rational->numerator
             * v.d->value.rational->denominator)
            + (v.d->value.rational->numerator
               * d->value.rational->denominator);
        d->value.rational->denominator *= v.d->value.rational->denominator;
        break;
    }
    }
    return *this;
}

PkMap<PkString, KisMetaData::Value> Value::asLangArray() const
{
    Q_ASSERT(d->type == LangArray);
    PkMap<PkString, KisMetaData::Value> langArray;
    for (const KisMetaData::Value& val : *d->value.array) {
        Q_ASSERT(val.d->propertyQualifiers.contains("xml:lang"));  // TODO probably worth to have an assert for this in the constructor as well
        KisMetaData::Value valKeyVal = val.d->propertyQualifiers.value("xml:lang");
        Q_ASSERT(valKeyVal.type() == Variant);
        PkVariant valKeyVar = valKeyVal.asVariant();
        Q_ASSERT(valKeyVar.type() == PkVariant::String);
        langArray[valKeyVar.toString()] = val;
    }
    return langArray;
}

PkString Value::toString() const
{
    switch (type()) {
    case Value::Invalid:
        return PkString("Invalid value.");
    case Value::Variant:
        return d->value.variant->toString();
        break;
    case Value::OrderedArray:
    case Value::UnorderedArray:
    case Value::AlternativeArray:
    case Value::LangArray: {
        PkString r = PkString("[%1]{ ").arg(d->value.array->size());
        for (int i = 0; i < d->value.array->size(); ++i) {
            const Value& val = d->value.array->at(i);
            r += val.toString();
            if (i != d->value.array->size() - 1) {
                r += PkString(",");
            }
            r += PkString(" ");
        }
        r += PkString("}");
        return r;
    }
    case Value::Structure: {
        PkString r = PkString("{ ");
        PkList<PkString> fields = d->value.structure->keys();
        for (int i = 0; i < fields.count(); ++i) {
            const PkString& field = fields[i];
            const Value& val = d->value.structure->value(field);
            r += field + " => " + val.toString();
            if (i != d->value.array->size() - 1) {
                r += PkString(",");
            }
            r += PkString(" ");
        }
        r += PkString("}");
        return r;
    }
    break;
    case Value::Rational:
        return PkString("%1 / %2").arg(d->value.rational->numerator).arg(d->value.rational->denominator);
    }
    return PkString("Invalid value.");
}
