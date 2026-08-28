/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KORGBU16COLORSPACE_H_
#define KORGBU16COLORSPACE_H_

#include "LcmsColorSpace.h"
#include "KoColorModelStandardIds.h"

struct KoBgrU16Traits;

class RgbU16ColorSpace : public LcmsColorSpace<KoBgrU16Traits>
{
public:
    RgbU16ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    KoID colorModelId() const override
    {
        return RGBAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer16BitsColorDepthID;
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
        return PkString("RGBA16");
    }

    void fillGrayBrushWithColorAndLightnessOverlay(quint8 *dst, const PkRgb *brush, quint8 *brushColor, qint32 nPixels) const override;
    void fillGrayBrushWithColorAndLightnessWithStrength(quint8* dst, const PkRgb* brush, quint8* brushColor, qreal strength, qint32 nPixels) const override;
    void modulateLightnessByGrayBrush(quint8 *dst, const PkRgb *brush, qreal strength, qint32 nPixels) const override;
};

class RgbU16ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    RgbU16ColorSpaceFactory() : LcmsColorSpaceFactory(TYPE_BGRA_16, cmsSigRgbData)
    {
    }
    PkString id() const override
    {
        return RgbU16ColorSpace::colorSpaceId();
    }
    PkString name() const override
    {
        return PkString("%1 (%2)").arg(RGBAColorModelID.name()).arg(Integer16BitsColorDepthID.name());
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
        return Integer16BitsColorDepthID;
    }
    int referenceDepth() const override
    {
        return 16;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new RgbU16ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "sRGB-elle-V2-g10.icc";//this is a linear space, because 16bit is enough to only enjoy advantages of linear space
    }
};

#endif
