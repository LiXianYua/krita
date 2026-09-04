/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOMESHPATCHESRENDERER_H
#define KOMESHPATCHESRENDERER_H

#include <PkImage.h>
#include <QImage>
#include <QPainter>
#include <PkPainterPath.h>
#include <PkVector.h>
#include <PkRect.h>

#include <PkFlakeBridge.h>
#include <KoColorSpaceRegistry.h>
#include <KoMixColorsOp.h>
#include <SvgMeshArray.h>
#include <SvgMeshGradient.h>

struct KoMeshPatchesRenderer {
public:

    KoMeshPatchesRenderer()
    {}

    void configure(PkRectF gradientRect, const PkTransform& painterTransform) {

        // NOTE: This is a necessary step to prevent loss of quality, because painterTransform is scaled.

        // we wish to scale the patch, but not translate, because patch should stay inside
        // the boundingRect
        PkTransform painterTransformShifted = painterTransform *
            PkTransform::fromTranslate(painterTransform.dx(), painterTransform.dy()).inverted();

        // we are applying transformation on a Unit rect, so we can extract scaling info only
        PkRectF unitRectScaled = painterTransformShifted.mapRect(PkRectF(gradientRect.topLeft(), PkSize(1, 1)));
        PkTransform scaledTransform = PkTransform::fromScale(unitRectScaled.width(), unitRectScaled.height());

        // boundingRect of the scaled version
        PkRectF scaledGradientRect = scaledTransform.mapRect(gradientRect);

        m_patch = QImage(toQSizeF(scaledGradientRect.size()).toSize(), QImage::Format_ARGB32);
        m_patch.fill(Qt::transparent);

        m_patchPainter.begin(&m_patch);

        // this ensures that the patch renders inside the boundingRect
        m_patchPainter.translate(-scaledGradientRect.left(), -scaledGradientRect.top());

        // upscale the patch to the same scaling factor as the painterTransform
        m_patchPainter.setTransform(toQTransform(scaledTransform), true);
        m_patchPainter.setCompositionMode(QPainter::CompositionMode_Source);
    }

    void fillPatch(const SvgMeshPatch *patch,
                   SvgMeshGradient::Shading type,
                   const SvgMeshArray *mesharray = nullptr,
                   const int row = -1,
                   const int col = -1) {

        PkColor color0 = patch->getStop(SvgMeshPatch::Top).color;
        PkColor color1 = patch->getStop(SvgMeshPatch::Right).color;
        PkColor color2 = patch->getStop(SvgMeshPatch::Bottom).color;
        PkColor color3 = patch->getStop(SvgMeshPatch::Left).color;

        const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();

        quint8 c[4][4];
        cs->fromQColor(color0, c[0]);
        cs->fromQColor(color1, c[1]);
        cs->fromQColor(color2, c[2]);
        cs->fromQColor(color3, c[3]);

        bool verticalDiv = patch->isDivisibleVertically();
        bool horizontalDiv = patch->isDivisibleHorizontally();
        bool colorVariationExists = checkColorVariance(c);

        if (colorVariationExists && (verticalDiv || horizontalDiv)) {
            PkVector<PkColor> colors;
            if (type == SvgMeshGradient::BICUBIC) {

                // it is a parent patch, so calculate coefficients aka alpha
                if (mesharray) {
                    calculateAlpha(mesharray, row, col, patch);
                }

                colors = getColorsBicubic(patch);
            } else {
                colors = getColorsBilinear(patch);
            }

            // SvgMeshPatch::subdivide* 收 PkVector<PkColor>，此处把 PkVector<PkColor> 转过去。
            PkVector<PkColor> colorsPk;
            for (const PkColor &c : colors) colorsPk.append(c);

            if (verticalDiv && horizontalDiv) {
                PkVector<SvgMeshPatch*> patches;
                patches.reserve(4);

                patch->subdivide(patches, colorsPk);
                for (const auto& p: patches) {
                    fillPatch(p, type);
                    delete p;
                }
            } else if (verticalDiv) {
                PkVector<SvgMeshPatch*> patches;
                patches.reserve(2);

                patch->subdivideVertically(patches, colorsPk);
                for (const auto& p: patches) {
                    fillPatch(p, type);
                    delete p;
                }
            } else if (horizontalDiv) {
                PkVector<SvgMeshPatch*> patches;
                patches.reserve(2);

                patch->subdivideHorizontally(patches, colorsPk);
                for (const auto& p: patches) {
                    fillPatch(p, type);
                    delete p;
                }
            }

        } else {
            const PkPainterPath outline = patch->getPath();
            const PkRectF patchRect = outline.boundingRect();

            quint8 mixed[4];
            cs->mixColorsOp()->mixColors(c[0], 4, mixed);

            // KoColorSpace::toQColor 剥离后收 PkColor*，先转 Pk 再经桥接回真 Qt 颜色。
            PkColor averagePk;
            cs->toQColor(mixed, &averagePk);
            PkColor average = averagePk;

            PkPen pen(average);
            pen.setWidth(0);
            m_patchPainter.setPen(toQPen(pen));

            if (patchRect.width() <= 1 && patchRect.height() <= 1) {
                m_patchPainter.drawPoint(toQPointF(patchRect.topLeft()));
                m_patchPainter.fillPath(toQPainterPath(outline), toQColor(average));

            } else {
                m_patchPainter.setBrush(toQColor(average));
                m_patchPainter.drawPath(toQPainterPath(outline));
            }
        }
    }

    /*
     * returns false if the variation is below tolerance.
     */
    bool checkColorVariance(quint8 c[4][4])
    {
        const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
        const quint8 tolerance = 0;

        for (int i = 0; i < 3; ++i) {
            if (cs->difference(c[i], c[i + 1]) > tolerance) {
                return true;
            }

            if (c[i][3] != c[i + 1][3] && cs->differenceA(c[i], c[i + 1]) > tolerance) {
                return true;
            }
        }

        return false;
    }

    PkVector<qreal> difference(const PkVector<qreal>& v1, const PkVector<qreal>& v2)
    {
        PkVector<qreal> v3(4, 0);
        for (int i = 0; i < 4; ++i) {
            v3[i] = v1[i] - v2[i];
        }
        return v3;
    }


    PkVector<qreal> multiply(const PkVector<qreal>& v1, qreal n)
    {
        PkVector<qreal> v3(4, 0);
        for (int i = 0; i < 4; ++i) {
            v3[i] = v1[i] * n;
        }
        return v3;
    }

    PkVector<qreal> split(PkColor c)
    {
        return {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
    }

    qreal getValue(const PkVector<qreal>& alpha, const PkPointF p)
    {
        KIS_ASSERT(alpha.size() == 16);
        qreal x = p.x(), y = p.y();
        qreal result = 0;

        qreal xx  = x * x;
        qreal xxx = xx * x;
        qreal yy  = y * y;
        qreal yyy = yy * y;

        result += alpha[0];
        result += alpha[1] * x;
        result += alpha[2] * xx;
        result += alpha[3] * xxx;

        result += alpha[4] * y;
        result += alpha[5] * y * x;
        result += alpha[6] * y * xx;
        result += alpha[7] * y * xxx;

        result += alpha[ 8] * yy;
        result += alpha[ 9] * yy * x;
        result += alpha[10] * yy * xx;
        result += alpha[11] * yy * xxx;

        result += alpha[12] * yyy;
        result += alpha[13] * yyy * x;
        result += alpha[14] * yyy * xx;
        result += alpha[15] * yyy * xxx;

        return result;
    }

    PkColor getColorUsingAlpha(const PkVector<PkVector<qreal>>& alpha, PkPointF p)
    {
        qreal r = getValue(alpha[0], p);
        qreal g = getValue(alpha[1], p);
        qreal b = getValue(alpha[2], p);
        qreal a = getValue(alpha[3], p);

        // Clamp
        r = r > 1.0 ? 1.0 : r;
        g = g > 1.0 ? 1.0 : g;
        b = b > 1.0 ? 1.0 : b;
        a = a > 1.0 ? 1.0 : a;

        r = r < 0.0 ? 0.0 : r;
        g = g < 0.0 ? 0.0 : g;
        b = b < 0.0 ? 0.0 : b;
        a = a < 0.0 ? 0.0 : a;

        PkColor result;
        result.setRgbF(r, g, b);
        result.setAlphaF(a);
        return result;
    }

    /// Naming convention adopted from: https://en.wikipedia.org/wiki/Bicubic_interpolation#Computation
    PkVector<qreal> getAlpha(const PkVector<qreal>& X)
    {
        const static qreal A[16][16] = {
            { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0},
            { 0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0},
            {-3, 3, 0, 0, -2,-1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0},
            { 2,-2, 0, 0,  1, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0},
            { 0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0},
            { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0},
            { 0, 0, 0, 0,  0, 0, 0, 0, -3, 3, 0, 0, -2,-1, 0, 0},
            { 0, 0, 0, 0,  0, 0, 0, 0,  2,-2, 0, 0,  1, 1, 0, 0},
            {-3, 0, 3, 0,  0, 0, 0, 0, -2, 0,-1, 0,  0, 0, 0, 0},
            { 0, 0, 0, 0, -3, 0, 3, 0,  0, 0, 0, 0, -2, 0,-1, 0},
            { 9,-9,-9, 9,  6, 3,-6,-3,  6,-6, 3,-3,  4, 2, 2, 1},
            {-6, 6, 6,-6, -3,-3, 3, 3, -4, 4,-2, 2, -2,-2,-1,-1},
            { 2, 0,-2, 0,  0, 0, 0, 0,  1, 0, 1, 0,  0, 0, 0, 0},
            { 0, 0, 0, 0,  2, 0,-2, 0,  0, 0, 0, 0,  1, 0, 1, 0},
            {-6, 6, 6,-6, -4,-2, 4, 2, -3, 3,-3, 3, -2,-1,-2,-1},
            { 4,-4,-4, 4,  2, 2,-2,-2,  2,-2, 2,-2,  1, 1, 1, 1}};


        PkVector<qreal> alpha(16, 0);
        for (int i = 0; i < 16; ++i) {
            alpha[i] = 0;
            for (int j = 0; j < 16; ++j) {
                alpha[i] += A[i][j] * X[j];
            }
        }

        return alpha;
    }

    PkVector<qreal> secant(const SvgMeshStop& stop1, const SvgMeshStop& stop2)
    {
        qreal distance = PkLineF(stop1.point, stop2.point).length();

        if (distance == 0.0) {  // NaN
            return {0.0, 0.0, 0.0, 0.0};
        }

        qreal c1[4] = {stop1.color.redF(), stop1.color.greenF(), stop1.color.blueF(), stop1.color.alphaF()};
        qreal c2[4] = {stop2.color.redF(), stop2.color.greenF(), stop2.color.blueF(), stop2.color.alphaF()};

        PkVector<qreal> result(4, 0.0);

        for (int i = 0; i < 4; ++i) {
            result[i] = (c2[i] - c1[i]) / distance;
        }

        return result;
    }

    PkVector<qreal> derivative(const SvgMeshStop& stop0,
                              const SvgMeshStop& stop1,
                              const SvgMeshStop& stop2)
    {
        PkVector<qreal> delta0 = secant(stop0, stop1);
        PkVector<qreal> delta1 = secant(stop1, stop2);

        PkVector<qreal> result(4);

        for (int i = 0; i < 4; ++i) {
            qreal slope = (delta0[i] + delta1[i]) / 2;

            // if sign changes
            if ((delta0[i] * delta1[i]) < 0 && delta0[i] > 0) {
                slope = 0;
            } else if (abs(slope) > 3 * abs(delta0[i])) {
                slope = 3 * delta0[i];
            } else if (abs(slope) > 3 * abs(delta1[i])) {
                slope = 3 * delta1[i];
            }

            result.push_back(slope);
        }
        return result;
    }

    /// Derivative in the X direction, but the patch should not be on an edge
    PkVector<PkVector<qreal>> derivativeX(const SvgMeshPatch* patch0,
                                        const SvgMeshPatch* patch1,
                                        const SvgMeshPatch* patch2)
    {
        SvgMeshStop f10 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f20 = patch0->getStop(SvgMeshPatch::Left);

        SvgMeshStop f11 = patch1->getStop(SvgMeshPatch::Top);
        SvgMeshStop f12 = patch1->getStop(SvgMeshPatch::Right);
        SvgMeshStop f21 = patch1->getStop(SvgMeshPatch::Left);
        SvgMeshStop f22 = patch1->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f13 = patch2->getStop(SvgMeshPatch::Right);
        SvgMeshStop f23 = patch2->getStop(SvgMeshPatch::Bottom);

        PkVector<qreal> d11 = derivative(f10, f11, f12);
        PkVector<qreal> d12 = derivative(f11, f12, f13);
        PkVector<qreal> d21 = derivative(f20, f21, f22);
        PkVector<qreal> d22 = derivative(f21, f22, f23);

        return {d11, d12, d21, d22};
    }

    /// Derivative in the Y direction, but the patch should not be on an edge
    PkVector<PkVector<qreal>> derivativeY(const SvgMeshPatch* patch0,
                                        const SvgMeshPatch* patch1,
                                        const SvgMeshPatch* patch2)
    {
        SvgMeshStop f01 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f02 = patch0->getStop(SvgMeshPatch::Right);

        SvgMeshStop f11 = patch1->getStop(SvgMeshPatch::Top);
        SvgMeshStop f12 = patch1->getStop(SvgMeshPatch::Right);
        SvgMeshStop f21 = patch1->getStop(SvgMeshPatch::Left);
        SvgMeshStop f22 = patch1->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f31 = patch2->getStop(SvgMeshPatch::Left);
        SvgMeshStop f32 = patch2->getStop(SvgMeshPatch::Bottom);

        PkVector<qreal> d11 = derivative(f01, f11, f21);
        PkVector<qreal> d12 = derivative(f02, f12, f22);
        PkVector<qreal> d21 = derivative(f11, f21, f31);
        PkVector<qreal> d22 = derivative(f12, f22, f32);

        return {d11, d12, d21, d22};
    }

    /// Derivative in the X direction. The edge has to be in left-most column.
    PkVector<PkVector<qreal>> derivativeEdgeBeginX(const SvgMeshPatch* patch0,
                                                 const SvgMeshPatch* patch1)
    {
        SvgMeshStop f00 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f01 = patch0->getStop(SvgMeshPatch::Right);
        SvgMeshStop f10 = patch0->getStop(SvgMeshPatch::Left);
        SvgMeshStop f11 = patch0->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f02 = patch1->getStop(SvgMeshPatch::Right);
        SvgMeshStop f12 = patch1->getStop(SvgMeshPatch::Bottom);

        PkVector<qreal> d01 = derivative(f00, f01, f02);
        PkVector<qreal> d11 = derivative(f10, f11, f12);
        PkVector<qreal> d00 = difference(multiply(secant(f00, f01), 2), d01);
        PkVector<qreal> d10 = difference(multiply(secant(f10, f11), 2), d11);

        return {d00, d01, d10, d11};
    }

    /// Derivative in the Y direction. The edge has to be in top row
    PkVector<PkVector<qreal>> derivativeEdgeBeginY(const SvgMeshPatch* patch0,
                                                 const SvgMeshPatch* patch1)
    {
        SvgMeshStop f00 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f01 = patch0->getStop(SvgMeshPatch::Right);
        SvgMeshStop f10 = patch0->getStop(SvgMeshPatch::Left);
        SvgMeshStop f11 = patch0->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f20 = patch1->getStop(SvgMeshPatch::Left);
        SvgMeshStop f21 = patch1->getStop(SvgMeshPatch::Bottom);

        PkVector<qreal> d10 = derivative(f00, f10, f20);
        PkVector<qreal> d11 = derivative(f01, f11, f21);
        PkVector<qreal> d00 = difference(multiply(secant(f00, f10), 2), d10);
        PkVector<qreal> d01 = difference(multiply(secant(f01, f11), 2), d11);

        return {d00, d01, d10, d11};
    }

    // Derivative in the X direction. The edge has to be in right-most column.
    PkVector<PkVector<qreal>> derivativeEdgeEndX(const SvgMeshPatch* patch1,
                                               const SvgMeshPatch* patch0)
    {
        SvgMeshStop f02 = patch1->getStop(SvgMeshPatch::Top);
        SvgMeshStop f03 = patch1->getStop(SvgMeshPatch::Right);
        SvgMeshStop f12 = patch1->getStop(SvgMeshPatch::Left);
        SvgMeshStop f13 = patch1->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f01 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f11 = patch0->getStop(SvgMeshPatch::Left);

        PkVector<qreal> d02 = derivative(f01, f02, f03);
        PkVector<qreal> d12 = derivative(f11, f12, f13);
        PkVector<qreal> d03 = difference(multiply(secant(f02, f03), 2), d02);
        PkVector<qreal> d13 = difference(multiply(secant(f12, f13), 2), d12);

        return {d02, d03, d12, d13};
    }

    // Derivative in the Y direction. The edge has to be the bottom row
    PkVector<PkVector<qreal>> derivativeEdgeEndY(const SvgMeshPatch* patch1,
                                                const SvgMeshPatch* patch0)
    {
        SvgMeshStop f22 = patch1->getStop(SvgMeshPatch::Top);
        SvgMeshStop f23 = patch1->getStop(SvgMeshPatch::Right);
        SvgMeshStop f32 = patch1->getStop(SvgMeshPatch::Left);
        SvgMeshStop f33 = patch1->getStop(SvgMeshPatch::Bottom);

        SvgMeshStop f12 = patch0->getStop(SvgMeshPatch::Top);
        SvgMeshStop f13 = patch0->getStop(SvgMeshPatch::Right);

        PkVector<qreal> d22 = derivative(f12, f22, f32);
        PkVector<qreal> d23 = derivative(f13, f23, f33);
        PkVector<qreal> d32 = difference(multiply(secant(f22, f32), 2), d22);
        PkVector<qreal> d33 = difference(multiply(secant(f23, f33), 2), d23);

        return {d22, d23, d32, d33};
    }

    // TODO: Make this private
    void calculateAlpha(const SvgMeshArray* mesharray, const int row, const int col, const SvgMeshPatch* patch)
    {
        // This is the convention which is being followed:
        //
        // f00, f03, f30, f33 are corners and their respective derivatives are represented as
        // d00, d03, d30, d33
        //   
        //   +-----> U/x (row)
        // |
        // |                 f00-------f01-------f02-------f03
        // V                  |         |         |         |
        //  V/y (col)         |         |         |         |
        //                    |         |         |         |
        //                   f10-------f11-------f12-------f13
        //                    |         |         |         |
        //                    |         |         |         |
        //                    |         |         |         |
        //                   f20-------f21-------f22-------f23
        //                    |         |         |         |
        //                    |         |         |         |
        //                    |         |         |         |
        //                   f30-------f31-------f32-------f33
        //

        SvgMeshStop f11 = patch->getStop(SvgMeshPatch::Top);
        SvgMeshStop f12 = patch->getStop(SvgMeshPatch::Right);
        SvgMeshStop f21 = patch->getStop(SvgMeshPatch::Left);
        SvgMeshStop f22 = patch->getStop(SvgMeshPatch::Bottom);

        PkVector<PkVector<qreal>> dx(4, PkVector<qreal>(4, 0));
        PkVector<PkVector<qreal>> dy(4, PkVector<qreal>(4, 0));

        // dx
        if (!mesharray || mesharray->numColumns() < 2) {

            // NOTE: they're zero here: from trial and error
            dx[0] = multiply(secant(f11, f12), 2);
            dx[2] = multiply(secant(f21, f22), 2);
            dx[1] = dx[3] = {0, 0, 0, 0};

        } else if (col == 0) {
            dx = derivativeEdgeBeginX(patch, mesharray->getPatch(row, col + 1));

        } else if (col == mesharray->numColumns() - 1) {
            dx = derivativeEdgeEndX(mesharray->getPatch(row, col),
                                     mesharray->getPatch(row, col - 1));
        } else {
            dx = derivativeX(mesharray->getPatch(row, col - 1),
                             mesharray->getPatch(row, col),
                             mesharray->getPatch(row, col + 1));
        }

        // dy
        if (!mesharray || mesharray->numRows() < 2) {

            // NOTE: they're zero here: from trial and error
            dy[0] = multiply(secant(f11, f21), 2);
            dy[1] = multiply(secant(f12, f22), 2);
            dy[2] = dy[3] = {0, 0, 0, 0};

        } else if (row == 0) {
            dy = derivativeEdgeBeginY(patch, mesharray->getPatch(row + 1, col));

        } else if (row == mesharray->numRows() - 1) {
            dy = derivativeEdgeEndY(mesharray->getPatch(row, col),
                                     mesharray->getPatch(row - 1, col));
        } else {
            dy = derivativeY(mesharray->getPatch(row - 1, col),
                             mesharray->getPatch(row, col),
                             mesharray->getPatch(row + 1, col));
        }

        PkVector<PkVector<qreal>> c = {split(f11.color), split(f12.color), split(f21.color), split(f22.color)};
        PkVector<PkVector<qreal>> alpha(4, PkVector<qreal>(16, 0));

        qreal width01 = PkLineF(f11.point, f12.point).length();
        qreal width23 = PkLineF(f21.point, f22.point).length();

        qreal height01 = PkLineF(f11.point, f21.point).length();
        qreal height23 = PkLineF(f12.point, f22.point).length();

        for (int i = 0; i < 4; ++i) {
            PkVector<qreal> X {
                c[0][i],
                c[1][i],
                c[2][i],
                c[3][i],

                dx[0][i] * width01,
                dx[1][i] * width01,
                dx[2][i] * width23,
                dx[3][i] * width23,

                dy[0][i] * height01,
                dy[1][i] * height23,
                dy[2][i] * height01,
                dy[3][i] * height23,

                // Specs says not to care about cross derivatives
                0,
                0,
                0,
                0};

            alpha[i] = getAlpha(X);
        }

        m_alpha = alpha;
    }

    PkVector<PkColor> getColorsBicubic(const SvgMeshPatch* patch)
    {
        PkPointF midTop    = patch->getMidpointParametric(SvgMeshPatch::Top);
        PkPointF midRight  = patch->getMidpointParametric(SvgMeshPatch::Right);
        PkPointF midBottom = patch->getMidpointParametric(SvgMeshPatch::Bottom);
        PkPointF midLeft   = patch->getMidpointParametric(SvgMeshPatch::Left);
        PkPointF center    = (midTop + midBottom) / 2;

        PkVector<PkColor> result(5);
        result[0] = getColorUsingAlpha(m_alpha, midTop);
        result[1] = getColorUsingAlpha(m_alpha, midRight);
        result[2] = getColorUsingAlpha(m_alpha, midBottom);
        result[3] = getColorUsingAlpha(m_alpha, midLeft);
        result[4] = getColorUsingAlpha(m_alpha, center);

        return result;
    }

    PkColor midPointColor(PkColor first, PkColor second)
    {
        qreal a = (first.alphaF() + second.alphaF()) / 2;
        qreal r = (first.redF()   + second.redF()) / 2;
        qreal g = (first.greenF() + second.greenF()) / 2;
        qreal b = (first.blueF()  + second.blueF()) / 2;

        PkColor c;
        c.setRgbF(r, g, b, a);
        return c;
    }

    PkVector<PkColor> getColorsBilinear(const SvgMeshPatch* patch)
    {
        PkVector<PkColor> result(5);

        PkColor c1 = patch->getStop(SvgMeshPatch::Top).color;
        PkColor c2 = patch->getStop(SvgMeshPatch::Right).color;
        PkColor c3 = patch->getStop(SvgMeshPatch::Bottom).color;
        PkColor c4 = patch->getStop(SvgMeshPatch::Left).color;

        result[0] = midPointColor(c1, c2);
        result[1] = midPointColor(c2, c3);
        result[2] = midPointColor(c3, c4);
        result[3] = midPointColor(c4, c1);
        result[4] = midPointColor(midPointColor(result[0], result[2]), midPointColor(result[1], result[3]));

        return result;
    }

    QImage* patchImage() {
        return &m_patch;
    }

private:
    QImage m_patch;   // S-09-g：QPainter 需 QPaintDevice（过渡期真 Qt）
    QPainter m_patchPainter;
    // TODO: make them local
    PkVector<PkVector<qreal>> m_alpha;
};

#endif // KOMESHPATCHESRENDERER_H
