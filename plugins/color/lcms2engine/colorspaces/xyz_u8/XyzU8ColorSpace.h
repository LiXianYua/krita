/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger (cberger@cberger.net)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KIS_XYZ_U8_COLORSPACE_H_
#define KIS_XYZ_U8_COLORSPACE_H_

#include <LcmsColorSpace.h>

#include <KoColorModelStandardIds.h>

#define TYPE_XYZA_8 (COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1))

struct KoXyzU8Traits;

class XyzU8ColorSpace : public LcmsColorSpace<KoXyzU8Traits>
{
public:

    XyzU8ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    static PkString colorSpaceId()
    {
        return PkString("XYZA8");
    }

    KoID colorModelId() const override
    {
        return XYZAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer8BitsColorDepthID;
    }

    virtual KoColorSpace *clone() const;

    void colorToXML(const quint8 *pixel, PkXmlDocument &doc, PkXmlElement &colorElt) const override;

    void colorFromXML(quint8* pixel, const PkXmlElement& elt) const override;
    void toHSY(const PkVector<double> &channelValues, qreal *hue, qreal *sat, qreal *luma) const override;
    PkVector <double> fromHSY(qreal *hue, qreal *sat, qreal *luma) const override;
    void toYUV(const PkVector<double> &channelValues, qreal *y, qreal *u, qreal *v) const override;
    PkVector <double> fromYUV(qreal *y, qreal *u, qreal *v) const override;

};

class XyzU8ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:

    XyzU8ColorSpaceFactory() : LcmsColorSpaceFactory(TYPE_XYZA_8, cmsSigXYZData)
    {
    }

    PkString id() const override
    {
        return XyzU8ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(XYZAColorModelID.name()).arg(Integer8BitsColorDepthID.name());
    }

    bool userVisible() const override
    {
        return true;
    }

    KoID colorModelId() const override
    {
        return XYZAColorModelID;
    }

    KoID colorDepthId() const override
    {
        return Integer8BitsColorDepthID;
    }

    int referenceDepth() const override
    {
        return 8;
    }

    KoColorSpace *createColorSpace(const KoColorProfile *p) const override
    {
        return new XyzU8ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "XYZ identity built-in";
    }
};

#endif
