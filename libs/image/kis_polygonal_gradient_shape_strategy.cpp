/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_polygonal_gradient_shape_strategy.h"

#include "kis_debug.h"

#include "kis_algebra_2d.h"

#include <config-gsl.h>

#ifdef HAVE_GSL
#include <gsl/gsl_multimin.h>
#endif /* HAVE_GSL */

#include <limits>

#include <boost/math/distributions/normal.hpp>

#include "krita_utils.h"



namespace Private {

    PkPointF centerFromPath(const PkPainterPath &selectionPath) {
        PkPointF center;
        int numPoints = 0;

        for (int i = 0; i < selectionPath.elementCount(); i++) {
            PkPainterPath::Element element = selectionPath.elementAt(i);
            if (element.type == PkPainterPath::CurveToDataElement) continue;

            center += element;
            numPoints++;
        }
        if (numPoints == 0) { // can only happen if the path is empty
            return center;
        }
        center /= numPoints;

        return center;
    }

    qreal getDisnormedGradientValue(const PkPointF &pt, const PkPainterPath &selectionPath, qreal exponent)
    {
        // FIXME: exponent = 2.0
        //        We explicitly use pow2() and sqrt() functions here
        //        for efficiency reasons.
        KIS_ASSERT_RECOVER_NOOP(qFuzzyCompare(exponent, 2.0));
        const qreal minHiLevel = std::pow(0.5, 1.0 / exponent);
        qreal ptWeightNode = 0.0;

        for (int i = 0; i < selectionPath.elementCount(); i++) {
            if (selectionPath.elementAt(i).isMoveTo()) continue;

            const int prevI = i > 0 ? i - 1 : selectionPath.elementCount() - 1;
            const PkPointF edgeP1 = selectionPath.elementAt(prevI);
            const PkPointF edgeP2 = selectionPath.elementAt(i);

            const PkPointF edgeVec = edgeP1 - edgeP2;
            const PkPointF q1 = pt - edgeP1;
            const PkPointF q2 = pt - edgeP2;

            const qreal proj1 = KisAlgebra2D::dotProduct(edgeVec, q1);
            const qreal proj2 = KisAlgebra2D::dotProduct(edgeVec, q2);

            qreal hi = 1.0;

            // pt's projection lays outside the edge itself,
            // when the projections has the same sign

            if (proj1 * proj2 >= 0) {
                PkPointF nearestPointVec =
                    qAbs(proj1) < qAbs(proj2) ? q1 : q2;

                hi = KisAlgebra2D::norm(nearestPointVec);
            } else {
                PkLineF line(edgeP1, edgeP2);
                hi = kisDistanceToLine(pt, line);
            }

            hi = qMax(minHiLevel, hi);

            // disabled for efficiency reasons
            // ptWeightNode += 1.0 / std::pow(hi, exponent);

            ptWeightNode += 1.0 / pow2(hi);
        }

        // disabled for efficiency reasons
        // return 1.0 / std::pow(ptWeightNode, 1.0 / exponent);

        return 1.0 / std::sqrt(ptWeightNode);
    }

    qreal initialExtremumValue(bool searchForMax) {
        return searchForMax ?
            std::numeric_limits<qreal>::min() :
            std::numeric_limits<qreal>::max();
    }

    bool findBestStartingPoint(int numSamples, const PkPainterPath &path,
                               qreal exponent, bool searchForMax,
                               qreal initialExtremumValue,
                               PkPointF *result) {

        const qreal minStepThreshold = 0.3;
        const int numCandidatesThreshold = 4;

        KIS_ASSERT_RECOVER(numSamples >= 4) {
            *result = centerFromPath(path);
            return true;
        }

        int totalSamples = numSamples;
        int effectiveSamples = numSamples & 0x1 ?
            (numSamples - 1) / 2 + 1 : numSamples;

        PkRectF rect = path.boundingRect();

        qreal xOffset = rect.width() / (totalSamples + 1);
        qreal xStep = effectiveSamples == totalSamples ? xOffset : 2 * xOffset;

        qreal yOffset = rect.height() / (totalSamples + 1);
        qreal yStep = effectiveSamples == totalSamples ? yOffset : 2 * yOffset;

        if (xStep < minStepThreshold || yStep < minStepThreshold) {
            return false;
        }

        int numFound = 0;
        int numCandidates = 0;
        PkPointF extremumPoint;
        qreal extremumValue = initialExtremumValue;

        const qreal eps = 1e-3;
        int sanityNumRows = 0;
        for (qreal y = rect.y() + yOffset; y < rect.bottom() - eps; y += yStep) {
            int sanityNumColumns = 0;

            sanityNumRows++;
            for (qreal x = rect.x() + xOffset; x < rect.right() - eps; x += xStep) {
                sanityNumColumns++;

                const PkPointF pt(x, y);
                if (!path.contains(pt)) continue;

                qreal value = getDisnormedGradientValue(pt, path, exponent);
                bool isExtremum = searchForMax ? value > extremumValue : value < extremumValue;

                numCandidates++;

                if (isExtremum) {
                    numFound++;
                    extremumPoint = pt;
                    extremumValue = value;
                }
            }

            KIS_ASSERT_RECOVER_NOOP(sanityNumColumns == effectiveSamples);
        }
        KIS_ASSERT_RECOVER_NOOP(sanityNumRows == effectiveSamples);

        bool success = numFound && numCandidates >= numCandidatesThreshold;

        if (success) {
            *result = extremumPoint;
        } else {
            int newNumSamples = 2 * numSamples + 1;
            success = findBestStartingPoint(newNumSamples, path,
                                            exponent, searchForMax,
                                            extremumValue,
                                            result);

            if (!success && numFound > 0) {
                *result = extremumPoint;
                success = true;
            }
        }

        return success;
    }

#ifdef HAVE_GSL

    struct GradientDescentParams {
        PkPainterPath selectionPath;
        qreal exponent;
        bool searchForMax;
    };

    double errorFunc (const gsl_vector * x, void *paramsPtr)
    {
        double vX = gsl_vector_get(x, 0);
        double vY = gsl_vector_get(x, 1);

        const GradientDescentParams *params =
            static_cast<const GradientDescentParams*>(paramsPtr);

        qreal weight = getDisnormedGradientValue(PkPointF(vX, vY),
                                                 params->selectionPath,
                                                 params->exponent);

        return params->searchForMax ? 1.0 / weight : weight;
    }

    qreal calculateMaxWeight(const PkPainterPath &selectionPath,
                             qreal exponent,
                             bool searchForMax)
    {
        const gsl_multimin_fminimizer_type *T =
            gsl_multimin_fminimizer_nmsimplex2;
        gsl_multimin_fminimizer *s = 0;
        gsl_vector *ss, *x;
        gsl_multimin_function minex_func;

        size_t iter = 0;
        int status;
        double size;

        PkPointF center;
        bool centerExists =
            findBestStartingPoint(4, selectionPath,
                                  exponent, searchForMax,
                                  Private::initialExtremumValue(searchForMax),
                                  &center);

        if (!centerExists || !selectionPath.contains(center)) {

            // if the path is too small just return default values
            if (selectionPath.boundingRect().width() >= 2.0 &&
                selectionPath.boundingRect().height() >= 2.0) {

                KIS_ASSERT_RECOVER_NOOP(selectionPath.contains(center));
            }

            return searchForMax ? 1.0 : 0.0;
        }

        /* Starting point */
        x = gsl_vector_alloc (2);
        gsl_vector_set (x, 0, center.x());
        gsl_vector_set (x, 1, center.y());

        /* Set initial step sizes to 10 px */
        ss = gsl_vector_alloc (2);
        gsl_vector_set (ss, 0, 10);
        gsl_vector_set (ss, 1, 10);

        GradientDescentParams p;

        p.selectionPath = selectionPath;
        p.exponent = exponent;
        p.searchForMax = searchForMax;

        /* Initialize method and iterate */
        minex_func.n = 2;
        minex_func.f = errorFunc;
        minex_func.params = (void*)&p;

        s = gsl_multimin_fminimizer_alloc (T, 2);
        gsl_multimin_fminimizer_set (s, &minex_func, x, ss);

        qreal result = searchForMax ?
            getDisnormedGradientValue(center, selectionPath, exponent) : 0.0;

        do
        {
            iter++;
            status = gsl_multimin_fminimizer_iterate(s);

            if (status)
                break;

            size = gsl_multimin_fminimizer_size (s);
            status = gsl_multimin_test_size (size, 1e-6);

            if (status == GSL_SUCCESS)
            {
                // dbgKrita << "*******Converged to minimum";
                // dbgKrita << gsl_vector_get (s->x, 0)
                //          << gsl_vector_get (s->x, 1)
                //          << "|" << s->fval << size;

                result = searchForMax ? 1.0 / s->fval : s->fval;
            }
        }
        while (status == GSL_CONTINUE && iter < 10000);

        gsl_vector_free(x);
        gsl_vector_free(ss);
        gsl_multimin_fminimizer_free (s);

        return result;
    }

#else /* HAVE_GSL */

    qreal calculateMaxWeight(const PkPainterPath &selectionPath,
                             qreal exponent,
                             bool searchForMax)
    {
        PkPointF center = centerFromPath(selectionPath);
        return searchForMax ?
            getDisnormedGradientValue(center, selectionPath, exponent) : 0.0;
    }

#endif /* HAVE_GSL */

}

PkPainterPath simplifyPath(const PkPainterPath &path,
                          qreal sizePortion,
                          qreal minLinearSize,
                          int minNumSamples)
{
    PkPainterPath finalPath;

    const auto polygons = path.toSubpathPolygons(PkTransform());
    Q_FOREACH (const PkPolygonF poly, polygons) {
        PkPainterPath p;
        p.addPolygon(poly);

        const qreal length = p.length();
        const qreal lengthStep =
            KritaUtils::maxDimensionPortion(poly.boundingRect(),
                                            sizePortion, minLinearSize);

        int numSamples = qMax(qCeil(length / lengthStep), minNumSamples);

        if (numSamples > poly.size()) {
            finalPath.addPolygon(poly);
            finalPath.closeSubpath();
            continue;
        }

        const qreal portionStep = 1.0 / numSamples;

        PkPolygonF newPoly;
        for (qreal t = 0.0; t < 1.0; t += portionStep) {
            newPoly << p.pointAtPercent(t);
        }

        finalPath.addPolygon(newPoly);
        finalPath.closeSubpath();
    }

    return finalPath;
}

KisPolygonalGradientShapeStrategy::KisPolygonalGradientShapeStrategy(const PkPainterPath &selectionPath,
                                                                     qreal exponent)
    : m_exponent(exponent)
{
    m_selectionPath = simplifyPath(selectionPath, 0.01, 3.0, 100);

    m_maxWeight = Private::calculateMaxWeight(m_selectionPath, m_exponent, true);
    m_minWeight = Private::calculateMaxWeight(m_selectionPath, m_exponent, false);

    m_scaleCoeff = 1.0 / (m_maxWeight - m_minWeight);
}

KisPolygonalGradientShapeStrategy::~KisPolygonalGradientShapeStrategy()
{
}

double KisPolygonalGradientShapeStrategy::valueAt(double x, double y) const
{
    PkPointF pt(x, y);
    qreal value = 0.0;

    if (m_selectionPath.contains(pt)) {
        value = Private::getDisnormedGradientValue(pt, m_selectionPath, m_exponent);
        value =  (value - m_minWeight) * m_scaleCoeff;
    }

    return value;
}

PkPointF KisPolygonalGradientShapeStrategy::testingCalculatePathCenter(int numSamples, const PkPainterPath &path, qreal exponent, bool searchForMax)
{
    PkPointF result;

    qreal extremumValue = Private::initialExtremumValue(searchForMax);
    bool success = Private::findBestStartingPoint(numSamples, path,
                                                  exponent, searchForMax,
                                                  extremumValue,
                                                  &result);

    if (!success) {
        dbgKrita << "WARNING: Couldn't calculate findBestStartingPoint for:";
        dbgKrita << ppVar(numSamples);
        dbgKrita << ppVar(exponent);
        dbgKrita << ppVar(searchForMax);
        dbgKrita << ppVar(path);

    }

    return result;
}
