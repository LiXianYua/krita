#include <QPainter>
/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_PAINTING_TWEAKS_H
#define __KIS_PAINTING_TWEAKS_H

#include "kritaglobal_export.h"

#include <QPen>
#include <QBrush>

#include <PkVectorND.h>
#include <PkVectorND.h>

class QPainter;
class PkRegion;
class PkRect;
class QPen;

namespace KisPaintingTweaks {

    /**
     * This is a workaround for QPainter::clipRegion() bug. When zoom
     * is about 2000% and rotation is in a range[-5;5] degrees, the
     * generated region will have about 20k+ rectangles inside. Their
     * processing will be really slow. These functions work around
     * the issue.
     */
    KRITAGLOBAL_EXPORT PkRegion safeClipRegion(const QPainter &painter);

    /**
     * \see safeClipRegion()
     */
    KRITAGLOBAL_EXPORT PkRect safeClipBoundingRect(const QPainter &painter);

    KRITAGLOBAL_EXPORT void initAntsPen(QPen *antsPen, QPen *outlinePen,
                                        int antLength = 4, int antSpace = 4);

    /**
     * A special class to save painter->pen() and painter->brush() using RAII
     * principle.
     */
    class KRITAGLOBAL_EXPORT PenBrushSaver
    {
    public:
        struct allow_noop_t { explicit allow_noop_t() = default; };
        static constexpr allow_noop_t	allow_noop { };

        /**
         * Saves pen and brush state of the provided painter object. \p painter cannot be null.
         */
        PenBrushSaver(QPainter *painter);

        /**
         * Overrides pen and brush of \p painter with the provided values. \p painter cannot be null.
         */
        PenBrushSaver(QPainter *painter, const QPen &pen, const QBrush &brush);

        /**
         * Overrides pen and brush of \p painter with the provided values. \p painter cannot be null.
         */
        PenBrushSaver(QPainter *painter, const QPair<QPen, QBrush> &pair);

        /**
         * A special constructor of PenBrushSaver that allows \p painter to be null. Passing null
         * pointer will basically mean that the whole saver existence will be a noop.
         */
        PenBrushSaver(QPainter *painter, const QPair<QPen, QBrush> &pair, allow_noop_t);

        /**
         * Restores the state of the painter that has been saved during the construction of the saver
         */
        ~PenBrushSaver();

    private:
        PenBrushSaver(const PenBrushSaver &rhs) = delete;
        QPainter *m_painter;
        QPen m_pen;
        QBrush m_brush;
    };

    QColor KRITAGLOBAL_EXPORT blendColors(const QColor &c1, const QColor &c2, qreal r1);

    /**
     * @brief luminosityCoarse
     * This calculates the luminosity of the given QColor.
     * It uses a very coarse (10 step) lut to linearize the sRGB trc, and then
     * uses rec709 values to calculate the luminosity. Because of the effect of
     * linearization, this is still more precise than one that just calculates
     * based on coefficients.
     * @param c the color to calculate the luminosity of.
     * @param sRGBtrc whether to linearize the sRGB trc.
     * @return a delinearized luminosity value, quantized to steps of 0.1.
     */
    qreal KRITAGLOBAL_EXPORT luminosityCoarse(const QColor &c, bool sRGBtrc = true);

    /**
     * \return an approximate difference between \p c1 and \p c2
     *         in a (nonlinear) range [0, 3]
     *
     * The colors are compared using the formula:
     *     difference = sqrt(2 * diff_R^2 + 4 * diff_G^2 + 3 * diff_B^2)
     */
    qreal KRITAGLOBAL_EXPORT colorDifference(const QColor &c1, const QColor &c2);

    /**
     * Make the color \p color differ from \p baseColor for at least \p threshold value
     */
    void KRITAGLOBAL_EXPORT dragColor(QColor *color, const QColor &baseColor, qreal threshold);

    inline void rectToVertices(PkVector3D* vertices, const PkRectF &rc)
    {
        vertices[0] = PkVector3D(rc.left(),  rc.bottom(), 0.f);
        vertices[1] = PkVector3D(rc.left(),  rc.top(),    0.f);
        vertices[2] = PkVector3D(rc.right(), rc.bottom(), 0.f);
        vertices[3] = PkVector3D(rc.left(),  rc.top(), 0.f);
        vertices[4] = PkVector3D(rc.right(), rc.top(), 0.f);
        vertices[5] = PkVector3D(rc.right(), rc.bottom(),    0.f);
    }

    inline void rectToTexCoords(PkVector2D* texCoords, const PkRectF &rc)
    {
        texCoords[0] = PkVector2D(rc.left(), rc.bottom());
        texCoords[1] = PkVector2D(rc.left(), rc.top());
        texCoords[2] = PkVector2D(rc.right(), rc.bottom());
        texCoords[3] = PkVector2D(rc.left(), rc.top());
        texCoords[4] = PkVector2D(rc.right(), rc.top());
        texCoords[5] = PkVector2D(rc.right(), rc.bottom());
    }
}

#endif /* __KIS_PAINTING_TWEAKS_H */
