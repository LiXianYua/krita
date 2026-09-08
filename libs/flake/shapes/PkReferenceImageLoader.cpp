/* SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "PkReferenceImageLoader.h"
#include "ImageShapePngData.h"
#include <PkImageFileDecoder.h>
#include <exiv2/exiv2.hpp>
#include <lcms2.h>
#include <array>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <fstream>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace {
// Float matrices, transfer LUT scales and rounding follow Qt 5.15.7's
// qcolormatrix/qcolortrclut/qcolortransform. LittleCMS only parses ICC tags;
// its complete transform has different rounding and is not pixel-equivalent.
using Vector = std::array<float, 3>;
struct Matrix {
    Vector r, g, b;
    Vector map(const Vector &v) const {
        return {{r[0]*v[0]+g[0]*v[1]+b[0]*v[2], r[1]*v[0]+g[1]*v[1]+b[1]*v[2], r[2]*v[0]+g[2]*v[1]+b[2]*v[2]}};
    }
    Matrix operator*(const Matrix &m) const { return {map(m.r), map(m.g), map(m.b)}; }
    Matrix inverted() const {
        const float det = 1.f / (r[0]*(b[2]*g[1]-g[2]*b[1])-r[1]*(b[2]*g[0]-g[2]*b[0])+r[2]*(b[1]*g[0]-g[1]*b[0]));
        return {{{(g[1]*b[2]-b[1]*g[2])*det, (b[1]*r[2]-r[1]*b[2])*det, (r[1]*g[2]-g[1]*r[2])*det}},
                {{(b[0]*g[2]-g[0]*b[2])*det, (r[0]*b[2]-b[0]*r[2])*det, (g[0]*r[2]-r[0]*g[2])*det}},
                {{(g[0]*b[1]-b[0]*g[1])*det, (b[0]*r[1]-r[0]*b[1])*det, (r[0]*g[1]-g[0]*r[1])*det}}};
    }
    bool matches(const Matrix &m) const {
        for (int i=0;i<3;++i) if (std::abs(r[i]-m.r[i])>=1.f/2048 || std::abs(g[i]-m.g[i])>=1.f/2048 || std::abs(b[i]-m.b[i])>=1.f/2048) return false;
        return true;
    }
};
Vector chromaticity(double x, double y) { return {{float(x/y),1,float((1-x-y)/y)}}; }
Matrix fromPrimaries(double rx,double ry,double gx,double gy,double bx,double by,bool d50=false)
{
    Matrix xyz {chromaticity(rx,ry),chromaticity(gx,gy),chromaticity(bx,by)};
    const auto white=d50?chromaticity(.34567,.35850):chromaticity(.31271,.32902);
    const auto scale=xyz.inverted().map(white);
    xyz=xyz*Matrix{{{scale[0],0,0}},{{0,scale[1],0}},{{0,0,scale[2]}}};
    if (d50) return xyz;
    const Matrix brad {{{.8951f,-.7502f,.0389f}},{{.2664f,1.7135f,-.0685f}},{{-.1614f,.0367f,1.0296f}}};
    const Matrix inverse {{{.9869929f,.4323053f,-.0085287f}},{{-.1470543f,.5183603f,.0400428f}},{{.1599627f,.0492912f,.9684867f}}};
    const auto from=brad.map(white),to=brad.map(chromaticity(.34567,.35850));
    const Matrix adaptation {{{to[0]/from[0],0,0}},{{0,to[1]/from[1],0}},{{0,0,to[2]/from[2]}}};
    return (inverse*(adaptation*brad))*xyz;
}
const Matrix srgb=fromPrimaries(.64,.33,.30,.60,.15,.06);
const Matrix adobe=fromPrimaries(.64,.33,.21,.71,.15,.06);
const Matrix p3=fromPrimaries(.68,.32,.265,.690,.15,.06);
const Matrix prophoto=fromPrimaries(.7347,.2653,.1596,.8404,.0366,.0001,true);

float inputCurve(cmsToneCurve *curve, float x)
{
    const int type = cmsGetToneCurveParametricType(curve);
    const auto *segment = cmsGetToneCurveSegment(0, curve);
    if (!segment) return cmsEvalToneCurveFloat(curve, x);
    const auto *p = segment->Params;
    if (type == 1) return std::pow(x, static_cast<float>(p[0]));
    if (type >= 2 && type <= 5) {
        float g=p[0], a=p[1], b=p[2], c=type>=3?p[3]:0, d=type>=4?p[4]:-b/a;
        float e=type==5?p[5]:0, f=type==5?p[6]:0;
        if (type >= 4 && std::abs(g-2.4f)<1.f/2048 && std::abs(a-1.f/1.055f)<1.f/2048 &&
            std::abs(b-.055f/1.055f)<1.f/2048 && std::abs(c-1.f/12.92f)<1.f/2048 && std::abs(d-.04045f)<1.f/2048) {
            g=2.4f; a=1.f/1.055f; b=.055f/1.055f; c=1.f/12.92f; d=.04045f; e=f=0;
        }
        if (type == 2) return x >= d ? std::pow(a*x+b,g) : 0;
        if (type == 3) return x >= d ? std::pow(a*x+b,g)+c : c;
        return x < d ? c*x+f : std::pow(a*x+b,g)+e;
    }
    return cmsEvalToneCurveFloat(curve, x);
}
bool normalizeRgb(PkImage &image, cmsHPROFILE profile)
{
    const auto *r=static_cast<const cmsCIEXYZ*>(cmsReadTag(profile,cmsSigRedColorantTag));
    const auto *g=static_cast<const cmsCIEXYZ*>(cmsReadTag(profile,cmsSigGreenColorantTag));
    const auto *b=static_cast<const cmsCIEXYZ*>(cmsReadTag(profile,cmsSigBlueColorantTag));
    cmsToneCurve *curves[] = {static_cast<cmsToneCurve*>(cmsReadTag(profile,cmsSigRedTRCTag)),
        static_cast<cmsToneCurve*>(cmsReadTag(profile,cmsSigGreenTRCTag)), static_cast<cmsToneCurve*>(cmsReadTag(profile,cmsSigBlueTRCTag))};
    if (!r || !g || !b || !curves[0] || !curves[1] || !curves[2]) return false;
    Matrix source {{{float(r->X),float(r->Y),float(r->Z)}},{{float(g->X),float(g->Y),float(g->Z)}},{{float(b->X),float(b->Y),float(b->Z)}}};
    for (const auto &known : {srgb,adobe,p3,prophoto}) if (source.matches(known)) { source=known; break; }
    const Matrix transform = source.matches(srgb) ? Matrix{{{1,0,0}},{{0,1,0}},{{0,0,1}}} : srgb.inverted()*source;
    bool sameTrc = true;
    for (auto *curve : curves) for (float x : {.25f,.5f,.75f})
        sameTrc &= std::abs(inputCurve(curve,x)-std::pow((1.f/1.055f)*x+.055f/1.055f,2.4f)) < 1.f/65536;
    if (source.matches(srgb) && sameTrc) return true;
    std::array<std::array<float,4081>,3> toLinear;
    for (int c=0;c<3;++c) for (int i=0;i<=4080;++i)
        toLinear[c][i] = std::lround(inputCurve(curves[c], float(i/4080.0))*65280) * (1.f/65280);
    std::array<unsigned,4081> fromLinear;
    const float a=std::pow(1.f/(1.f/1.055f),2.4f), gInv=1.f/2.4f;
    const float e=-(.055f/1.055f)/(1.f/1.055f), c=1.f/(1.f/12.92f), d=(1.f/12.92f)*.04045f;
    for (int i=0;i<=4080;++i) {
        const float x=float(i/4080.0);
        const float value=x<d?c*x:std::pow(a*x,gInv)+e;
        fromLinear[i]=std::lround(value*65280);
    }
    if (image.format()==PkImage::Format_RGBA64 || image.format()==PkImage::Format_RGBX64) {
        PkImage normalized(image.width(),image.height(),PkImage::Format_RGBA64);
        for (int y=0;y<image.height();++y) {
            const auto *input=reinterpret_cast<const std::uint16_t*>(image.constScanLine(y));
            auto *output=reinterpret_cast<std::uint16_t*>(normalized.scanLine(y));
            for (int x=0;x<image.width();++x) {
                const auto index=[](unsigned value) { return (value-(value>>8))>>4; };
                const auto linear=transform.map({{toLinear[0][index(input[4*x])],
                    toLinear[1][index(input[4*x+1])],toLinear[2][index(input[4*x+2])]}});
                for (int c=0;c<3;++c) {
                    // Qt's straight RGBA64 store uses half-up LUT indexing,
                    // then expands its 0..65280 entries without losing low bits.
                    const auto value=fromLinear[int(std::clamp(linear[c],0.f,1.f)*4080.f+.5f)];
                    output[4*x+c]=value+(value>>8);
                }
                output[4*x+3]=image.format()==PkImage::Format_RGBX64?65535:input[4*x+3];
            }
        }
        image=normalized;
        return true;
    }
    const bool premultiplied=image.format()==PkImage::Format_ARGB32_Premultiplied;
    PkImage normalized(image.width(),image.height(),premultiplied?PkImage::Format_ARGB32_Premultiplied:PkImage::Format_ARGB32);
    for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) {
        const unsigned pixel=image.pixel(x,y);
        const unsigned alpha=pixel>>24;
        const auto inputIndex=[&](unsigned channel) {
            if (!premultiplied) return int(channel*16);
            if (!alpha) return 0;
#if defined(__SSE2__)
            const __m128 va = _mm_set1_ps(alpha);
            __m128 inverse = _mm_rcp_ps(va);
            inverse = _mm_sub_ps(_mm_add_ps(inverse,inverse),_mm_mul_ps(inverse,_mm_mul_ps(inverse,va)));
            const float value=float(channel)*_mm_cvtss_f32(inverse)*4080.f;
            return std::clamp(int(std::nearbyint(value)),0,4080);
#else
            return std::clamp(int(channel*(4080.f/alpha)+.5f),0,4080);
#endif
        };
        const auto linear=transform.map({{toLinear[0][inputIndex((pixel>>16)&255)],toLinear[1][inputIndex((pixel>>8)&255)],toLinear[2][inputIndex(pixel&255)]}});
        const auto channel=[&](float value) {
            const float scaled=std::clamp(value,0.f,1.f)*4080.f;
#if defined(__SSE2__)
            const int index=static_cast<int>(std::nearbyint(scaled));
#else
            const int index=static_cast<int>(scaled+.5f);
#endif
            if (!premultiplied) return (fromLinear[index]+128)>>8;
#if defined(__SSE2__)
            return unsigned(std::nearbyint(fromLinear[index]*(float(alpha)*(1.f/65280))));
#else
            return unsigned(fromLinear[index]*(alpha/65280.f)+.5f);
#endif
        };
        normalized.setPixel(x,y,(pixel&0xff000000u)|(channel(linear[0])<<16)|(channel(linear[1])<<8)|channel(linear[2]));
    }
    image=normalized;
    return true;
}
}

PkImage loadPkReferenceImage(const std::string &path)
{
    PkImage image;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto length = file ? file.tellg() : std::streampos(-1);
    if (length > 0 && static_cast<std::uint64_t>(length) <= ImageShapePngData::maxDecodedCompressedBytes()) {
        PkByteArray bytes;
        bytes.resize(static_cast<int>(length));
        file.seekg(0);
        if (file.read(bytes.data(), bytes.size())) image = ImageShapePngData::decodeReferenceImage(bytes);
    }
    if (image.isNull()) image = PkImageFileDecoder::load(path);
    if (image.isNull()) return image;
    try {
        auto metadata = Exiv2::ImageFactory::open(path);
        if (!metadata) return image;
        metadata->readMetadata();
        const auto &profile = metadata->iccProfile();
        if (profile.empty()) return image;
        using Profile = std::unique_ptr<void, decltype(&cmsCloseProfile)>;
        Profile input(cmsOpenProfileFromMem(profile.c_data(), static_cast<cmsUInt32Number>(profile.size())), cmsCloseProfile);
        if (!input || cmsGetColorSpace(input.get()) != cmsSigRgbData) return image;
        normalizeRgb(image, input.get());
        return image;
    } catch (const Exiv2::Error &) {
        // Missing/invalid metadata must not turn an otherwise readable raster
        // into an import failure (the Qt reader also accepts untagged images).
        return image;
    }
}
