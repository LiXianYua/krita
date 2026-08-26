/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCOSPARSER_H
#define KISCOSPARSER_H

#include <PkVariant.h>
#include <PkStream.h>
#include "kritapsdutils_export.h"

/**
 * @brief The KisCosParser class
 *
 * PSD text engine data is written in PDF's Carousel Object Structure,
 * a format not unsimilar to (might be a precursor) to JSON.
 * JSON however doesn't differentiate between ints and doubles,
 * so we use PkVariantHash instead.
 *
 * This parser tries to parse the ByteArray as a PkVariantHash, though
 * not every data type is interpreted as such:
 *
 * For one, 'name' objects are interpreted as strings prepended with /
 * Hex strings are kept inside their < and >
 *
 * Code was based off qjsonparser.cpp
 */

class KRITAPSDUTILS_EXPORT KisCosParser
{
public:
    PkVariantHash parseCosToJson(PkByteArray *ba);
private:

    bool parseValue(PkStream &dev, PkVariant &val);
    bool parseObject(PkStream &dev, PkVariantHash &object, bool checkEnd = true);
    bool parseArray(PkStream &dev, PkVariantList &array);
};

#endif // KISCOSPARSER_H
