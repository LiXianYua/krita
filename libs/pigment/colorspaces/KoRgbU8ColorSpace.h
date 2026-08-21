/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KORGBU8COLORSPACE_H
#define KORGBU8COLORSPACE_H

#include <PkColor.h>
#include <PkString.h>
#include <PkVector.h>

#include "KoSimpleColorSpace.h"
#include "KoSimpleColorSpaceFactory.h"
#include "KoColorModelStandardIds.h"

struct KoBgrU8Traits;

/**
 * The alpha mask is a special color strategy that treats all pixels as
 * alpha value with a color common to the mask. The default color is white.
 */
class KoRgbU8ColorSpace : public KoSimpleColorSpace<KoBgrU8Traits>
{

public:

    KoRgbU8ColorSpace();

    ~KoRgbU8ColorSpace() override;

    static PkString colorSpaceId();

    virtual KoColorSpace* clone() const;

    void fromQColor(const PkColor& color, quint8 *dst) const override;

    void toQColor(const quint8 *src, PkColor *c) const override;
    
    void toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const override;
    PkVector <double> fromHSY(qreal *hue, qreal *sat, qreal *luma) const override;
    void toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const override;
    PkVector <double> fromYUV(qreal *y, qreal *u, qreal *v) const override;

    void fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const override;
    void fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const override;
    void modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const override;
};

class KoRgbU8ColorSpaceFactory : public KoSimpleColorSpaceFactory
{

public:
    KoRgbU8ColorSpaceFactory()
            : KoSimpleColorSpaceFactory(PkString("RGBA"),
                                        PkString("RGB (8-bit integer/channel, unmanaged)"),
                                        true,
                                        RGBAColorModelID,
                                        Integer8BitsColorDepthID) {
    }

    KoColorSpace *createColorSpace(const KoColorProfile *) const override {
        return new KoRgbU8ColorSpace();
    }

};

#endif
