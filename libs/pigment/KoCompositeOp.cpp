/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "KoCompositeOp.h"

#include <klocalizedstring.h>
#include <KoID.h>

#include "KoColorSpace.h"
#include "KoCompositeOpRegistry.h"

static PkString compositeOpDisplayName(const PkString &id)
{
    return KoCompositeOpRegistry::instance().getCompositeOpDisplayName(id);
}

static PkString categoryDisplayName(const PkString &id)
{
    return KoCompositeOpRegistry::instance().getCategoryDisplayName(id);
}

#define LAZY_STATIC_CATEGORY_DISPLAY_NAME(n) \
    []() { \
        static const PkString name = categoryDisplayName(PkString(n)); \
        return name; \
    }()

PkString KoCompositeOp::categoryArithmetic() { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("arithmetic"); }
PkString KoCompositeOp::categoryBinary()     { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("binary");     }
PkString KoCompositeOp::categoryModulo()     { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("modulo");     }
PkString KoCompositeOp::categoryNegative()   { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("negative");   }
PkString KoCompositeOp::categoryLight()      { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("light");      }
PkString KoCompositeOp::categoryDark()       { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("dark");       }
PkString KoCompositeOp::categoryHSY()        { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("hsy");        }
PkString KoCompositeOp::categoryHSI()        { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("hsi");        }
PkString KoCompositeOp::categoryHSL()        { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("hsl");        }
PkString KoCompositeOp::categoryHSV()        { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("hsv");        }
PkString KoCompositeOp::categoryMix()        { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("mix");        }
PkString KoCompositeOp::categoryMisc()       { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("misc");       }
PkString KoCompositeOp::categoryQuadratic()  { return LAZY_STATIC_CATEGORY_DISPLAY_NAME("quadratic");  }

KoCompositeOp::ParameterInfo::ParameterInfo()
    : opacity(1.0f)
    , flow(1.0f)
    , lastOpacity(&opacity)
{
}

KoCompositeOp::ParameterInfo::ParameterInfo(const ParameterInfo &rhs)
{
    copy(rhs);
}

KoCompositeOp::ParameterInfo& KoCompositeOp::ParameterInfo::operator=(const ParameterInfo &rhs)
{
    copy(rhs);
    return *this;
}

void KoCompositeOp::ParameterInfo::setOpacityAndAverage(float _opacity, float _averageOpacity)
{
    if (qFuzzyCompare(_opacity, _averageOpacity)) {
        opacity = _opacity;
        lastOpacity = &opacity;
    } else {
        opacity = _opacity;
        _lastOpacityData = _averageOpacity;
        lastOpacity = &_lastOpacityData;
    }
}

void KoCompositeOp::ParameterInfo::copy(const ParameterInfo &rhs)
{
    dstRowStart = rhs.dstRowStart;
    dstRowStride = rhs.dstRowStride;
    srcRowStart = rhs.srcRowStart;
    srcRowStride = rhs.srcRowStride;
    maskRowStart = rhs.maskRowStart;
    maskRowStride = rhs.maskRowStride;
    rows = rhs.rows;
    cols = rhs.cols;
    opacity = rhs.opacity;
    flow = rhs.flow;
    _lastOpacityData = rhs._lastOpacityData;
    channelFlags = rhs.channelFlags;

    lastOpacity = rhs.lastOpacity == &rhs.opacity ?
        &opacity : &_lastOpacityData;
}

void KoCompositeOp::ParameterInfo::updateOpacityAndAverage(float value) {
    const float exponent = 0.1;

    opacity = value;

    if (*lastOpacity < opacity) {
        lastOpacity = &opacity;
    } else {
        _lastOpacityData = exponent * opacity + (1.0 - exponent) * (*lastOpacity);
        lastOpacity = &_lastOpacityData;
    }
}

struct Q_DECL_HIDDEN KoCompositeOp::Private {
    const KoColorSpace * colorSpace;
    PkString id;
    PkString description;
    PkString category;
    PkBitArray defaultChannelFlags;
};

KoCompositeOp::KoCompositeOp() : d(new Private)
{

}

KoCompositeOp::~KoCompositeOp()
{
    delete d;
}

KoCompositeOp::KoCompositeOp(const KoColorSpace * cs, const PkString& id, const PkString & category)
        : d(new Private)
{
    d->colorSpace = cs;
    d->id = id;
    d->description = compositeOpDisplayName(id);
    d->category = category;
    if (d->category.isEmpty()) {
        d->category = categoryMisc();
    }
}

void KoCompositeOp::composite(quint8 *dstRowStart, qint32 dstRowStride,
                               const quint8 *srcRowStart, qint32 srcRowStride,
                               const quint8 *maskRowStart, qint32 maskRowStride,
                               qint32 rows, qint32 numColumns,
                               float opacity, const PkBitArray& channelFlags) const
{
    KoCompositeOp::ParameterInfo params;
    params.dstRowStart   = dstRowStart;
    params.dstRowStride  = dstRowStride;
    params.srcRowStart   = srcRowStart;
    params.srcRowStride  = srcRowStride;
    params.maskRowStart  = maskRowStart;
    params.maskRowStride = maskRowStride;
    params.rows          = rows;
    params.cols          = numColumns;
    params.opacity       = opacity;
    params.flow          = 1.0f;
    params.channelFlags  = channelFlags;
    composite(params);
}


void KoCompositeOp::composite(const KoCompositeOp::ParameterInfo& params) const
{
    composite(params.dstRowStart           , params.dstRowStride ,
              params.srcRowStart           , params.srcRowStride ,
              params.maskRowStart          , params.maskRowStride,
              params.rows                  , params.cols         ,
              params.opacity, params.channelFlags );
}

PkString KoCompositeOp::category() const
{
    return d->category;
}

PkString KoCompositeOp::id() const
{
    return d->id;
}

PkString KoCompositeOp::description() const
{
    return d->description;
}

const KoColorSpace * KoCompositeOp::colorSpace() const
{
    return d->colorSpace;
}
