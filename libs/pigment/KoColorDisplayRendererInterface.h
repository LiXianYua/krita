/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __KO_COLOR_DISPLAY_RENDERER_INTERFACE_H
#define __KO_COLOR_DISPLAY_RENDERER_INTERFACE_H

#include <PkObject.h>
#include <PkColor.h>
#include <PkImage.h>
#include <PkSize.h>
#include <PkSignalCompat.h>

#include "KoColor.h"

class KoChannelInfo;
class KoColorSpace;

/**
 * A special interface class provided by pigment to let widgets render
 * a KoColor on screen using custom profiling provided by the user.
 *
 * If you want to provide your own rendering of the KoColor on screen,
 * reimplement this class and provide its instance to a supporting
 * widget.
 */
class KRITAPIGMENT_EXPORT KoColorDisplayRendererInterface : public PkObject
{
    // 信号声明采用 PkSignal 形态（pk/signal）：displayConfigurationChanged 用
    // signals 标记声明，定义由 pk/signal/pk_signal_moc.py 生成（体内调用
    // activateSignal）。继承 PkObject 以满足信号连接的生命周期绑定；
    // 拷贝构造/赋值由 PkObject 基类 delete。

public:
    KoColorDisplayRendererInterface();
    ~KoColorDisplayRendererInterface() override;

    /**
     * @brief Convert a consecutive block of pixel data to an ARGB32 PkImage
     * @param srcColorSpace the colorspace the pixel data is in
     * @param data a pointer to a byte array with color data; must cover the requested image size
     * @param size defines the dimensions of the resulting image
     * @param proofPaintColors optionally adjust the color data to painting gamut first
     * @return a PkImage that can be displayed
     */
    virtual PkImage toQImage(const KoColorSpace *srcColorSpace, const quint8 *data, PkSize size, bool proofPaintColors = false) const = 0;

    /**
     * Convert the color \p c to a custom PkColor that will be
     * displayed by the widget on screen. Please note, that the
     * reverse conversion may simply not exist.
     * @param proofPaintColors optionally adjust the color data to painting gamut first
     */
    virtual PkColor toQColor(const KoColor &c, bool proofToPaintColors = false) const = 0;

    /**
     * This tries to approximate a rendered PkColor into the KoColor
     * of the painting color space. Please note, that in most of the
     * cases the exact reverse transformation does not exist, so the
     * resulting color will be only a rough approximation. Never try
     * to do a round trip like that:
     *
     * // r will never be equal to c!
     * r = approximateFromRenderedQColor(toQColor(c));
     */
    virtual KoColor approximateFromRenderedQColor(const PkColor &c) const = 0;

    virtual KoColor fromHsv(int h, int s, int v, int a = 255) const = 0;
    virtual void getHsv(const KoColor &srcColor, int *h, int *s, int *v, int *a = 0) const = 0;


    /**
     * \return the minimum value of a floating point channel that can
     *         be seen on screen
     */
    virtual qreal minVisibleFloatValue(const KoChannelInfo *chaninfo) const = 0;

    /**
     * \return the maximum value of a floating point channel that can
     *         be seen on screen. In normal situation it is 1.0. When
     *         the user changes exposure the value varies.
     */
    virtual qreal maxVisibleFloatValue(const KoChannelInfo *chaninfo) const = 0;

    /**
     * @brief getColorSpace
     * @return the painting color space, this is useful for determining the transform.
     */
    virtual const KoColorSpace* getPaintingColorSpace() const = 0;

signals:
    void displayConfigurationChanged();

private:
    KoColorDisplayRendererInterface(const KoColorDisplayRendererInterface&) = delete;
    KoColorDisplayRendererInterface& operator=(const KoColorDisplayRendererInterface&) = delete;
};

/**
 * The default conversion class that just calls KoColor::toQColor()
 * conversion implementation which effectively renders the color into
 * sRGB color space.
 */
class KRITAPIGMENT_EXPORT KoDumbColorDisplayRenderer : public KoColorDisplayRendererInterface
{
public:
    PkImage toQImage(const KoColorSpace *srcColorSpace, const quint8 *data, PkSize size, bool proofPaintColors = false) const override;
    PkColor toQColor(const KoColor &c, bool proofToPaintColors = false) const override;
    KoColor approximateFromRenderedQColor(const PkColor &c) const override;
    KoColor fromHsv(int h, int s, int v, int a = 255) const override;
    void getHsv(const KoColor &srcColor, int *h, int *s, int *v, int *a = 0) const override;

    qreal minVisibleFloatValue(const KoChannelInfo *chaninfo) const override;
    qreal maxVisibleFloatValue(const KoChannelInfo *chaninfo) const override;

    const KoColorSpace* getPaintingColorSpace() const override;

    static KoColorDisplayRendererInterface* instance();
};

#endif /* __KO_COLOR_DISPLAY_RENDERER_INTERFACE_H */
