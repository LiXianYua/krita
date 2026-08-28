/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef LabU16ColorSpace_H_
#define LabU16ColorSpace_H_

#include "LcmsColorSpace.h"
#include "KoColorModelStandardIds.h"

#define TYPE_LABA_16 (COLORSPACE_SH(PT_Lab) | CHANNELS_SH(3) | BYTES_SH(2) | EXTRA_SH(1))

struct KoLabF32Traits;

class LabU16ColorSpace : public LcmsColorSpace<KoLabU16Traits>
{
public:

    LabU16ColorSpace(const PkString &name, KoColorProfile *p);

    bool willDegrade(ColorSpaceIndependence independence) const override;

    static PkString colorSpaceId()
    {
        return PkString("LABA");
    }

    KoID colorModelId() const override
    {
        return LABAColorModelID;
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
    quint8 scaleToU8(const quint8 * srcPixel, qint32 channelIndex) const override;
    void convertChannelToVisualRepresentation(const quint8 *src, quint8 *dst, quint32 nPixels, const qint32 selectedChannelIndex) const override;
    void convertChannelToVisualRepresentation(const quint8 *src, quint8 *dst, quint32 nPixels, const PkBitArray selectedChannels) const override;
};

class LabU16ColorSpaceFactory : public LcmsColorSpaceFactory
{
public:
    LabU16ColorSpaceFactory()
        : LcmsColorSpaceFactory(TYPE_LABA_16, cmsSigLabData)
    {
    }

    bool userVisible() const override
    {
        return true;
    }

    PkString id() const override
    {
        return LabU16ColorSpace::colorSpaceId();
    }

    PkString name() const override
    {
        return PkString("%1 (%2)").arg(LABAColorModelID.name()).arg(Integer16BitsColorDepthID.name());
    }

    KoID colorModelId() const override
    {
        return LABAColorModelID;
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
        return new LabU16ColorSpace(name(), p->clone());
    }

    PkString defaultProfile() const override
    {
        return "Lab identity built-in";
    }
};

#endif
