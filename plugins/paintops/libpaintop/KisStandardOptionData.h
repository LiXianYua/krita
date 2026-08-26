/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSTANDARDOPTIONDATA_H
#define KISSTANDARDOPTIONDATA_H

#include <KisCurveOptionData.h>


struct KisOpacityOptionData : KisCurveOptionData
{
    KisOpacityOptionData(const PkString &prefix = PkString())
        : KisCurveOptionData(
              prefix,
              KoID("Opacity", PkString("Opacity")),
              KisKritaSensorPack::Checkability::NotCheckable)
    {
    }
};

struct KisFlowOptionData : KisCurveOptionData
{
    KisFlowOptionData(const PkString &prefix = PkString())
        : KisCurveOptionData(
              prefix,
              KoID("Flow", PkString("Flow")),
              KisKritaSensorPack::Checkability::NotCheckable)
    {
    }
};


struct KisRatioOptionData : KisCurveOptionData
{
    KisRatioOptionData(const PkString &prefix = PkString())
        : KisCurveOptionData(
              prefix,
              KoID("Ratio", PkString("Ratio")))
    {
    }
};

struct KisSoftnessOptionData : KisCurveOptionData
{
    KisSoftnessOptionData()
        : KisCurveOptionData(
              KoID("Softness", PkString("Softness")),
              Checkability::Checkable, std::nullopt,
              std::make_pair(0.1, 1.0))
    {}
};

struct KisRotationOptionData : KisCurveOptionData
{
    KisRotationOptionData(const PkString &prefix = PkString())
        : KisCurveOptionData(
              prefix,
              KoID("Rotation", PkString("Rotation")))
    {
    }
};

struct KisDarkenOptionData : KisCurveOptionData
{
    KisDarkenOptionData()
        : KisCurveOptionData(
              KoID("Darken", PkString("Darken")))
    {}
};

struct KisMixOptionData : KisCurveOptionData
{
    KisMixOptionData()
        : KisCurveOptionData(
              KoID("Mix", PkString("Mix")))
    {}
};

struct KisHueOptionData : KisCurveOptionData
{
    KisHueOptionData()
        : KisCurveOptionData(
              KoID("h", PkString("Hue")))
    {}
};

struct KisSaturationOptionData : KisCurveOptionData
{
    KisSaturationOptionData()
        : KisCurveOptionData(
              KoID("s", PkString("Saturation")))
    {}
};

struct KisValueOptionData : KisCurveOptionData
{
    KisValueOptionData()
        : KisCurveOptionData(
              KoID("v", PkString("Value")))
    {}
};

struct KisRateOptionData : KisCurveOptionData
{
    KisRateOptionData()
        : KisCurveOptionData(
              KoID("Rate", PkString("Rate")))
    {}
};

struct KisStrengthOptionData : KisCurveOptionData
{
    KisStrengthOptionData()
        : KisCurveOptionData(
              KoID("Texture/Strength/", PkString("Strength")))
    {}
};

struct KisLightnessStrengthOptionData : KisCurveOptionData
{
    KisLightnessStrengthOptionData()
        : KisCurveOptionData(
              KoID("LightnessStrength", PkString("Lightness Strength")))
    {
    }
};


#endif // KISSTANDARDOPTIONDATA_H
