/*
 * SPDX-FileCopyrightText: 2014 Manuel Riecke <spell1337@gmail.com>
 *
 * SPDX-License-Identifier: ICS
 */

#ifndef INDEXCOLORPALETTE_H
#define INDEXCOLORPALETTE_H

#include <PkVector.h>
#include <PkColor.h>
#include <PkPair.h>
#include <KoColor.h>

struct LabColor
{
    quint16 L;
    quint16 a;
    quint16 b;
};

struct IndexColorPalette
{
    PkVector<LabColor> m_colors;

    struct
    {
        float L;
        float a;
        float b;
    } similarityFactors;

    IndexColorPalette();
    void insertShades(PkColor clrA, PkColor clrB, int shades);
    void insertShades(KoColor clrA, KoColor clrB, int shades);
    void insertShades(LabColor clrA, LabColor clrB, int shades);

    void insertColor(PkColor clr);
    void insertColor(KoColor clr);
    void insertColor(LabColor clr);

    void mergeMostRedundantColors();

    LabColor getNearestIndex(LabColor clr) const;
    int numColors() const;
    float similarity(LabColor c0, LabColor c1) const;
    PkPair< int, int > getNeighbours(int mainClr) const;
};

#endif // INDEXCOLORPALETTE_H
