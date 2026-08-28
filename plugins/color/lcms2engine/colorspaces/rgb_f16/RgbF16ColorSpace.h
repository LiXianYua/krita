/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KORGBF16COLORSPACE_H_
#define KORGBF16COLORSPACE_H_

#include "LcmsColorSpace.h"
#include "KoColorModelStandardIds.h"

struct KoRgbF16Traits;

class RgbF16ColorSpace : public LcmsColorSpace<KoRgbF16Traits>
{
public:
    RgbF16ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    KoID colorModelId() const override
    {
        return RGBAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Float16BitsColorDepthID;
    }

    virtual KoColorSpace *clone() const;

    void colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const override;

    void colorFromXML(quint8* pixel, const PkXmlElement& elt) const override;
    void toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const override;
    PkVector <double> fromHSY(qreal *hue, qreal *sat, qreal *luma) const override;
    void toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const override;
    PkVector <double> fromYUV(qreal *y, qreal *u, qreal *v) const override;

    static PkString colorSpaceId()
    {
        return PkString("RGBAF16");
    }

    bool hasHighDynamicRange() const override
    {
        return true;
    }

    void fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const override;
    void fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const override;
    void modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const override;
};

class RgbF16ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    RgbF16ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_RGBA_HALF_FLT, cmsSigRgbData)
    {
    }

    PkString id() const override
    {
        return RgbF16ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(RGBAColorModelID.name()).arg(Float16BitsColorDepthID.name());
    }

    bool userVisible() const override
    {
        return true;
    }

    KoID colorModelId() const override
    {
        return RGBAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Float16BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 16;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new RgbF16ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "sRGB-elle-V2-g10.icc";
    }

    bool isHdr() const override
    {
        return true;
    }

};

#endif
