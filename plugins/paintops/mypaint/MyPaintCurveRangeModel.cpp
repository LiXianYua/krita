/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintCurveRangeModel.h"
#include "kis_algebra_2d.h"
#include "kis_cubic_curve.h"
#include <lager/lenses.hpp>

namespace {

    auto curveToNormalizedCurve = lager::lenses::getset(
        [] (const std::tuple<PkString, PkRectF> &curveData)
        {
            MyPaintCurveRangeModel::NormalizedCurve normalized;
            PkList<KisCubicCurvePoint> points = KisCubicCurve(std::get<0>(curveData)).curvePoints();
            const PkRectF bounds = std::get<1>(curveData);

            normalized.yLimit = qMax(qAbs(bounds.top()), qAbs(bounds.bottom()));
            normalized.xMax = bounds.right();
            normalized.xMin = bounds.left();

            if (qFuzzyIsNull(normalized.yLimit)) {
                points = {{0.0, 0.5, false}, {1.0, 0.5, false}};
            } else {
                for (auto it = points.begin(); it != points.end(); ++it) {
                    it->setX((it->x() - bounds.left()) / bounds.width());
                    it->setY(it->y() / (2.0 * normalized.yLimit) + 0.5);
                }
            }

            normalized.curve = KisCubicCurve(points).toString();

            //qDebug() << "get" << std::get<0>(curveData) << "->" << normalized.curve << bounds;
            return normalized;
        },
        [] (std::tuple<PkString, PkRectF> curveData, const MyPaintCurveRangeModel::NormalizedCurve &normalizedCurve) {
            PkList<KisCubicCurvePoint> points = KisCubicCurve(normalizedCurve.curve).curvePoints();

            for (auto it = points.begin(); it != points.end(); ++it) {
                it->setX(it->x() * (normalizedCurve.xMax - normalizedCurve.xMin) + normalizedCurve.xMin);
                it->setY((it->y() - 0.5) * normalizedCurve.yLimit * 2.0);
            }

            std::get<0>(curveData) = KisCubicCurve(points).toString();

            std::get<1>(curveData) = PkRectF(normalizedCurve.xMin,
                                            -normalizedCurve.yLimit,
                                            normalizedCurve.xMax - normalizedCurve.xMin,
                                            2.0 * normalizedCurve.yLimit);

            //qDebug() << "set" << std::get<0>(curveData) << "<-" << normalizedCurve.curve << std::get<1>(curveData);
            return curveData;
        }
    );

} // namespace


std::tuple<PkString, PkRectF> MyPaintCurveRangeModel::reshapeCurve(std::tuple<PkString, PkRectF> curve)
{
    /**
     * Krita's GUI doesn't support x-range more narrow than 0...1, so
     * we should extend it if necessary
     */
    std::get<1>(curve) |= PkRect(0, -1, 1, 2);

    NormalizedCurve normalized = lager::view(curveToNormalizedCurve, curve);
    curve = lager::set(curveToNormalizedCurve, curve, normalized);
    return curve;
}
