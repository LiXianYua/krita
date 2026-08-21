/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KORGBU16COLORSPACE_H
#define KORGBU16COLORSPACE_H

#include <PkColor.h>
#include <PkString.h>
#include <PkVector.h>

#include "KoSimpleColorSpace.h"
#include "KoSimpleColorSpaceFactory.h"
#include "KoColorModelStandardIds.h"

struct KoBgrU16Traits;

/**
 * The alpha mask is a special color strategy that treats all pixels as
 * alpha value with a color common to the mask. The default color is white.
 */
class KoRgbU16ColorSpace : public KoSimpleColorSpace<KoBgrU16Traits>
{

public:

    KoRgbU16ColorSpace();
    ~KoRgbU16ColorSpace() override;

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

class KoRgbU16ColorSpaceFactory : public KoSimpleColorSpaceFactory
{

public:
    KoRgbU16ColorSpaceFactory()
            : KoSimpleColorSpaceFactory(KoRgbU16ColorSpace::colorSpaceId(),
                                        PkString("RGB (16-bit integer/channel, unmanaged)"),
                                        true,
                                        RGBAColorModelID,
                                        Integer16BitsColorDepthID) {
    }

    KoColorSpace *createColorSpace(const KoColorProfile *) const override {
        return new KoRgbU16ColorSpace();
    }

};


#endif
