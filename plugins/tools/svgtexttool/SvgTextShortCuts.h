/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SVGTEXTSHORTCUTS_H
#define SVGTEXTSHORTCUTS_H

#include <PkStringList.h>

class KoSvgTextProperties;
/**
 * @brief The SvgTextShortCuts class
 *
 * Class to handle text property shortcuts generically.
 *
 * Many text property shortcuts are about toggling/enabling a single property.
 * Given there's a huge amount of them, it thus makes sense to generalize the
 * actions by looking up their stable action identifiers in the table below.
 *
 */
class SvgTextShortCuts
{
public:
    static PkStringList possibleActions();
    static bool isAction(const PkString &name);

    static bool actionEnabled(const PkString &name, const PkList<KoSvgTextProperties> currentProperties);

    static KoSvgTextProperties getModifiedProperties(const PkString &name,
                                                     bool checked,
                                                     PkList<KoSvgTextProperties> currentProperties);

private:
};

#endif // SVGTEXTSHORTCUTS_H
