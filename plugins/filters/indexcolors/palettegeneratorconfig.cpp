/*
 * SPDX-FileCopyrightText: 2014 Manuel Riecke <spell1337@gmail.com>
 *
 * SPDX-License-Identifier: ICS
 */

#include "palettegeneratorconfig.h"
#include <PkDataStream.h>
#include <PkMessageLogger.h>

PaletteGeneratorConfig::PaletteGeneratorConfig()
{
    for(int j = 0; j < 4; ++j)
    {
        colors[0][j] = PkColor(Qt::white);
        colors[1][j] = PkColor(Qt::yellow);
        colors[2][j] = PkColor(Qt::gray);
        colors[3][j] = PkColor(Qt::black);
    }

    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            colorsEnabled[i][j] = (j == 0);

    for(int i = 0; i < 3; ++i)
        gradientSteps[i] = 4;

    inbetweenRampSteps = 2;
    diagonalGradients = false;
}

PkByteArray PaletteGeneratorConfig::toByteArray()
{
    PkByteArray retVal;
    PkDataStream stream(&retVal, PkStream::WriteOnly);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(PkDataStream::BigEndian);

    // Version int
    stream << std::int32_t(0);

    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            stream << colors[i][j];

    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            stream << colorsEnabled[i][j];

    for(int i = 0; i < 3; ++i)
        stream << std::int32_t(gradientSteps[i]);

    stream << std::int32_t(inbetweenRampSteps);
    stream << diagonalGradients;
    return retVal;
}

void PaletteGeneratorConfig::fromByteArray(const PkByteArray& str)
{
    PkDataStream stream(str);
    stream.setVersion(PkDataStream::Qt_4_6);
    stream.setByteOrder(PkDataStream::BigEndian);

    std::int32_t version;
    stream >> version;
    if(version == 0)
    {
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                stream >> colors[i][j];

        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                stream >> colorsEnabled[i][j];

        for(int i = 0; i < 3; ++i)
        {
            std::int32_t decoded;
            stream >> decoded;
            gradientSteps[i] = decoded;
        }

        std::int32_t rampSteps;
        stream >> rampSteps;
        inbetweenRampSteps = rampSteps;
        stream >> diagonalGradients;
    }
    else
        qDebug("PaletteGeneratorConfig::FromByteArray: Unsupported data version");
}

IndexColorPalette PaletteGeneratorConfig::generate()
{
    IndexColorPalette pal;
    // Add all colors to the palette
    for(int y = 0; y < 4; ++y)
        for(int x = 0; x < 4; ++x)
            if(colorsEnabled[y][x])
                pal.insertColor(colors[y][x]);

    for(int y = 0; y < 3; ++y)
    {
        for(int x = 0; x < 4; ++x)
            if(colorsEnabled[y][x] && colorsEnabled[y+1][x])
                pal.insertShades(colors[y][x], colors[y+1][x], gradientSteps[y]);
    }

    if(inbetweenRampSteps)
    {
        for(int y = 0; y < 4; ++y)
            for(int x = 0; x < 3; ++x)
                if(colorsEnabled[y][x] && colorsEnabled[y][x+1])
                    pal.insertShades(colors[y][x], colors[y][x+1], inbetweenRampSteps);
    }

    if(diagonalGradients)
    {
        for(int y = 0; y < 3; ++y)
            for(int x = 0; x < 4; ++x)
            {
                if(x+1 < 4)
                    if(colorsEnabled[y][x+1] && colorsEnabled[y+1][x])
                        pal.insertShades(colors[y][x+1], colors[y+1][x], gradientSteps[y]);
                if(x-1 >= 0)
                    if(colorsEnabled[y][x-1] && colorsEnabled[y+1][x])
                        pal.insertShades(colors[y][x-1], colors[y+1][x], gradientSteps[y]);
            }
    }
    return pal;
}
