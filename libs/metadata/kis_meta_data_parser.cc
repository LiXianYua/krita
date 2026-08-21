/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_parser.h"

#include "kis_meta_data_value.h"

using namespace KisMetaData;

Parser::~Parser()
{
}

#include "kis_meta_data_parser_p.h"

#include <PkDateTime.h>
#include <PkVariant.h>

#include <regex>
#include <string>


Value IntegerParser::parse(const PkString& _v) const
{
    return Value(_v.toInt());
}

Value TextParser::parse(const PkString& _v) const
{
    return Value(_v);
}

Value DateParser::parse(const PkString& _v) const
{
    if (_v.size() <= 4) {
        return Value(PkDateTime::fromString(_v.PkToUtf8(), "yyyy"));
    } else if (_v.size() <= 7) {
        return Value(PkDateTime::fromString(_v.PkToUtf8(), "yyyy-MM"));
    } else if (_v.size() <= 10) {
        return Value(PkDateTime::fromString(_v.PkToUtf8(), "yyyy-MM-dd"));
    } else if (_v.size() <= 16) {
        return Value(PkDateTime::fromString(_v.PkToUtf8(), "yyyy-MM-ddThh:mm"));
    } else if (_v.size() <= 19) {
        return Value(PkDateTime::fromString(_v.PkToUtf8(), "yyyy-MM-ddThh:mm:ss"));
    } else {
        return Value(PkDateTime::fromString(_v.PkToUtf8()));
    }
}

Value RationalParser::parse(const PkString& _v) const
{
    std::regex regexp("(\\-?\\d+)[ ]*/[ ]*(\\d+)");
    std::smatch match;
    const std::string text = _v.PkToUtf8();

    if (std::regex_match(text, match, regexp) && match.size() > 2)
        return Value(Rational(std::stoi(match[1].str()), std::stoi(match[2].str())));
    return Value();
}
