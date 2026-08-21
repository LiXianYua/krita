/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

// PkXmlCompat 预激活：本 TU 的 include 闭包含未剥的 KoColorSpaceRegistry.h
// （其容器/字符串类型全部映射 Pk），未剥头在同一 Pk 映射下解析。
#include <PkXmlCompat.h>

#include "KoColorDisplayRendererInterface.h"

#include <KoColorSpaceRegistry.h>
#include <KoChannelInfo.h>
#include <KoColorConversionTransformation.h>
#include <KoColorSpace.h>

KoColorDisplayRendererInterface::KoColorDisplayRendererInterface()
{
}

KoColorDisplayRendererInterface::~KoColorDisplayRendererInterface()
{
}

PkImage KoDumbColorDisplayRenderer::toQImage(const KoColorSpace *srcColorSpace, const quint8 *data, PkSize size, bool proofPaintColors) const
{
    Q_UNUSED(proofPaintColors); // dumb converter doesn't know a painting color space
    return srcColorSpace->convertToQImage(data, size.width(), size.height(), 0,
                                          KoColorConversionTransformation::internalRenderingIntent(),
                                          KoColorConversionTransformation::internalConversionFlags());
}

PkColor KoDumbColorDisplayRenderer::toQColor(const KoColor &c, bool proofToPaintColors) const
{
    Q_UNUSED(proofToPaintColors);
    return c.toQColor();
}

KoColor KoDumbColorDisplayRenderer::approximateFromRenderedQColor(const PkColor &c) const
{
    KoColor color;
    color.fromQColor(c);
    return color;
}

KoColor KoDumbColorDisplayRenderer::fromHsv(int h, int s, int v, int a) const
{
    h = qBound(0, h, 359);
    s = qBound(0, s, 255);
    v = qBound(0, v, 255);
    a = qBound(0, a, 255);
    PkColor qcolor(PkColor::fromHsv(h, s, v, a));
    return KoColor(qcolor, KoColorSpaceRegistry::instance()->rgb8());
}

void KoDumbColorDisplayRenderer::getHsv(const KoColor &srcColor, int *h, int *s, int *v, int *a) const
{
    PkColor qcolor = toQColor(srcColor);
    // PkColor 无 getHsv（对齐 Qt 语义：灰 = hue -1），逐分量读取。
    if (h) *h = qcolor.hue();
    if (s) *s = qcolor.saturation();
    if (v) *v = qcolor.value();
    if (a) *a = qcolor.alpha();
}

KoColorDisplayRendererInterface* KoDumbColorDisplayRenderer::instance()
{
    static KoDumbColorDisplayRenderer s_instance;
    return &s_instance;
}

qreal KoDumbColorDisplayRenderer::minVisibleFloatValue(const KoChannelInfo *chaninfo) const
{
    Q_ASSERT(chaninfo);
    return chaninfo->getUIMin();
}

qreal KoDumbColorDisplayRenderer::maxVisibleFloatValue(const KoChannelInfo *chaninfo) const
{
    Q_ASSERT(chaninfo);
    return chaninfo->getUIMax();
}

const KoColorSpace* KoDumbColorDisplayRenderer::getPaintingColorSpace() const
{
    return KoColorSpaceRegistry::instance()->rgb8();
}
