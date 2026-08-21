/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_entry.h"
#include <PkString.h>

#include <kis_debug.h>

#include "kis_meta_data_value.h"
#include "kis_meta_data_schema.h"

using namespace KisMetaData;

struct Entry::Private {
    PkString name;
    const Schema* schema;
    Value value;
    bool valid;
};

Entry::Entry() :
        d(new Private)
{
    d->schema = 0;
    d->valid = false;
}

Entry::Entry(const Schema* schema, PkString name, const Value& value) :
        d(new Private)
{
    Q_ASSERT(!name.isEmpty());
    if (!isValidName(name)) {
        errKrita << "Invalid metadata name:" << name;
        d->name = PkString("INVALID: %1").arg(name);
    }
    else {
        d->name = name;
    }
    d->schema = schema;
    d->value = value;
    d->valid = true;
}

Entry::Entry(const Entry& e) : d(new Private())
{
    d->valid = false;
    *this = e;
}

Entry::~Entry()
{
    delete d;
}

PkString Entry::name() const
{
    return d->name;
}

const Schema* Entry::schema() const
{
    Q_ASSERT(d->schema);
    return d->schema;
}

void Entry::setSchema(const KisMetaData::Schema* schema)
{
    Q_ASSERT(schema);
    d->schema = schema;
}

PkString Entry::qualifiedName() const
{
    Q_ASSERT(d->schema);
    return d->schema->generateQualifiedName(d->name);
}

const Value& Entry::value() const
{
    return d->value;
}

Value& Entry::value()
{
    return d->value;
}

bool Entry::isValid() const
{
    return d->valid;
}

namespace {
bool isAsciiLetter(char16_t c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}
bool isAsciiLetterOrNumber(char16_t c)
{
    return isAsciiLetter(c) || (c >= u'0' && c <= u'9');
}
}

bool Entry::isValidName(const PkString& _name)
{
    if (_name.size() < 1) {
        dbgMetaData << "Too small";
        return false;
    }
    if (!isAsciiLetter(_name[0])) {
        dbgMetaData << _name << " doesn't start by a letter";
        return false;
    }
    for (int i = 1; i < _name.size(); ++i) {
        char16_t c = _name[i];
        if (!isAsciiLetterOrNumber(c)) {
            dbgMetaData << _name << " " << i << "th character isn't a letter or a digit";
            return false;
        }
    }
    return true;
}


bool Entry::operator==(const Entry& e) const
{
    return qualifiedName() == e.qualifiedName();
}

Entry& Entry::operator=(const Entry & e)
{
    if (e.isValid()) {
        Q_ASSERT(!isValid() || *this == e);
        d->name = e.d->name;
        d->schema = e.d->schema;
        d->value = e.d->value;
        d->valid = true;
    }
    return *this;
}

PkDebug operator<<(PkDebug debug, const Entry &c)
{
    debug.nospace() << "Name: " << c.name() << " Qualified name: " << c.qualifiedName() << " Value: " << c.value();
    return debug.space();
}
