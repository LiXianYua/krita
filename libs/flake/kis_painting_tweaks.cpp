/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_tweaks.h"

#include <PkPen.h>
#include <PkTransform.h>

#include <cmath>
// [migrate] missing include for Pk/Qt type
#include <PkMap.h>

namespace {
// S-08 Task 6：libs/global 的 kis_global.h 已 Pk 化（S-02-a），在保留 Qt 的
// 绘图闭包内不能引入（pk/geometry 的 qAbs 与 Qt 冲突）。此处仅补本文件用到
// 的 pow2 模板，其余依赖保持 Qt 原生。
template <typename T>
inline T pow2(const T &x) { return x * x; }
}


namespace KisPaintingTweaks {

void initAntsPen(PkPen *antsPen, PkPen *outlinePen,
                 int antLength, int antSpace)
{
    PkVector<qreal> antDashPattern;
    antDashPattern << antLength << antSpace;

    *antsPen = PkPen(Pk::CustomDashLine);
    antsPen->setDashPattern(antDashPattern);
    antsPen->setCosmetic(true);
    antsPen->setColor(Pk::black);

    *outlinePen = PkPen(Pk::SolidLine);
    outlinePen->setCosmetic(true);
    outlinePen->setColor(PkColor(Pk::white));
}

PenBrushSaver::PenBrushSaver(PkPainter *painter)
    : m_pkPainter(painter),
      m_pen(painter ? painter->pen() : PkPen()),
      m_pkBrush(painter ? painter->brush() : PkBrush())
{
}

PenBrushSaver::PenBrushSaver(PkPainter *painter, const PkPen &pen, const PkBrush &brush)
    : PenBrushSaver(painter)
{
    if (m_pkPainter) {
        m_pkPainter->setPen(pen);
        m_pkPainter->setBrush(brush);
    }
}

PenBrushSaver::PenBrushSaver(PkPainter *painter, const std::pair<PkPen, PkBrush> &pair)
    : PenBrushSaver(painter)
{
    if (m_pkPainter) {
        m_pkPainter->setPen(pair.first);
        m_pkPainter->setBrush(pair.second);
    }
}

PenBrushSaver::PenBrushSaver(PkPainter *painter, const std::pair<PkPen, PkBrush> &pair, allow_noop_t)
    : m_pkPainter(painter)
{
    if (m_pkPainter) {
        m_pen = m_pkPainter->pen();
        m_pkBrush = m_pkPainter->brush();
        m_pkPainter->setPen(pair.first);
        m_pkPainter->setBrush(pair.second);
    }
}

PenBrushSaver::~PenBrushSaver()
{
    if (m_pkPainter) {
        m_pkPainter->setPen(m_pen);
        m_pkPainter->setBrush(m_pkBrush);
    }
}

PkColor blendColors(const PkColor &c1, const PkColor &c2, qreal r1)
{
    const qreal r2 = 1.0 - r1;

    return PkColor::fromRgbF(
        c1.redF() * r1 + c2.redF() * r2,
        c1.greenF() * r1 + c2.greenF() * r2,
        c1.blueF() * r1 + c2.blueF() * r2);
}

qreal colorDifference(const PkColor &c1, const PkColor &c2)
{
    const qreal dr = c1.redF() - c2.redF();
    const qreal dg = c1.greenF() - c2.greenF();
    const qreal db = c1.blueF() - c2.blueF();

    return std::sqrt(2 * pow2(dr) + 4 * pow2(dg) + 3 * pow2(db));
}

void dragColor(PkColor *color, const PkColor &baseColor, qreal threshold)
{
    while (colorDifference(*color, baseColor) < threshold) {

        PkColor newColor = *color;

        if (newColor.lightnessF() > baseColor.lightnessF()) {
            newColor = newColor.lighter(120);
        } else {
            newColor = newColor.darker(120);
        }

        if (newColor == *color) {
            break;
        }

        *color = newColor;
    }
}

// This does a simplified linearization and calculates the luma.
// Krita has the ability to precisely calculate this value,
// but that seems overkill when all we want to know is whether
// it passes a certain gray threshold.
static PkMap<qreal, qreal> sRgbTRCToLinear {
    {0.0, 0.0},
    {0.1, 0.01002},
    {0.2, 0.0331},
    {0.3, 0.07324},
    {0.4, 0.13287},
    {0.5, 0.21404},
    {0.6, 0.31855},
    {0.7, 0.44799},
    {0.8, 0.60383},
    {0.9, 0.78741},
    {1.0, 1.0}
};

static PkMap<qreal, qreal> linearToSRGBTRC {
    {0.0, 0.0},
    {0.01002, 0.1},
    {0.0331, 0.2},
    {0.07324, 0.3},
    {0.13287, 0.4},
    {0.21404, 0.5},
    {0.31855, 0.6},
    {0.44799, 0.7},
    {0.60383, 0.8},
    {0.78741, 0.9},
    {1.0, 1.0}
};

qreal luminosityCoarse(const PkColor &c, bool sRGBtrc)
{
    qreal r = c.redF();
    qreal g = c.greenF();
    qreal b = c.blueF();
    if (sRGBtrc) {
        if (r < 1.0) {
            r = sRgbTRCToLinear.upperBound(r).value();
        }
        if (g < 1.0) {
            g = sRgbTRCToLinear.upperBound(g).value();
        }
        if (b < 1.0) {
            b = sRgbTRCToLinear.upperBound(b).value();
        }
    }
    qreal lumi = (r * .2126) + (g * .7152) + (b * .0722);
    if (sRGBtrc && lumi < 1.0) {
        lumi = linearToSRGBTRC.lowerBound(lumi).value();
    }
    return lumi;
}

}
