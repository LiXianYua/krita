/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_PAINTING_TWEAKS_H
#define __KIS_PAINTING_TWEAKS_H

#include "kritaflake_export.h"

#include <PkPen.h>
#include <pk/render/PkPainter.h>
// [migrate] missing include for Pk/Qt type
#include <PkRect.h>

class PkRect;
class PkPen;

namespace KisPaintingTweaks {

    KRITAFLAKE_EXPORT void initAntsPen(PkPen *antsPen, PkPen *outlinePen,
                                        int antLength = 4, int antSpace = 4);


    /**
     * A special class to save painter->pen() and painter->brush() using RAII
     * principle.
     */
    class KRITAFLAKE_EXPORT PenBrushSaver
    {
    public:
        struct allow_noop_t { explicit allow_noop_t() = default; };
        static constexpr allow_noop_t	allow_noop { };

        PenBrushSaver(PkPainter *painter);
        PenBrushSaver(PkPainter *painter, const PkPen &pen, const PkBrush &brush);
        PenBrushSaver(PkPainter *painter, const std::pair<PkPen, PkBrush> &pair);
        PenBrushSaver(PkPainter *painter, const std::pair<PkPen, PkBrush> &pair, allow_noop_t);

        /**
         * Restores the state of the painter that has been saved during the construction of the saver
         */
        ~PenBrushSaver();

    private:
        PenBrushSaver(const PenBrushSaver &rhs) = delete;
        PkPainter *m_pkPainter = nullptr;
        PkPen m_pen;
        PkBrush m_pkBrush;
    };

    PkColor KRITAFLAKE_EXPORT blendColors(const PkColor &c1, const PkColor &c2, qreal r1);


    /**
     * @brief luminosityCoarse
     * This calculates the luminosity of the given PkColor.
     * It uses a very coarse (10 step) lut to linearize the sRGB trc, and then
     * uses rec709 values to calculate the luminosity. Because of the effect of
     * linearization, this is still more precise than one that just calculates
     * based on coefficients.
     * @param c the color to calculate the luminosity of.
     * @param sRGBtrc whether to linearize the sRGB trc.
     * @return a delinearized luminosity value, quantized to steps of 0.1.
     */
    qreal KRITAFLAKE_EXPORT luminosityCoarse(const PkColor &c, bool sRGBtrc = true);

    /**
     * \return an approximate difference between \p c1 and \p c2
     *         in a (nonlinear) range [0, 3]
     *
     * The colors are compared using the formula:
     *     difference = sqrt(2 * diff_R^2 + 4 * diff_G^2 + 3 * diff_B^2)
     */
    qreal KRITAFLAKE_EXPORT colorDifference(const PkColor &c1, const PkColor &c2);

    /**
     * Make the color \p color differ from \p baseColor for at least \p threshold value
     */
    void KRITAFLAKE_EXPORT dragColor(PkColor *color, const PkColor &baseColor, qreal threshold);

    template <typename Vector3D>
    inline void rectToVertices(Vector3D* vertices, const PkRectF &rc)
    {
        vertices[0] = Vector3D(rc.left(),  rc.bottom(), 0.f);
        vertices[1] = Vector3D(rc.left(),  rc.top(),    0.f);
        vertices[2] = Vector3D(rc.right(), rc.bottom(), 0.f);
        vertices[3] = Vector3D(rc.left(),  rc.top(), 0.f);
        vertices[4] = Vector3D(rc.right(), rc.top(), 0.f);
        vertices[5] = Vector3D(rc.right(), rc.bottom(),    0.f);
    }

    template <typename Vector2D>
    inline void rectToTexCoords(Vector2D* texCoords, const PkRectF &rc)
    {
        texCoords[0] = Vector2D(rc.left(), rc.bottom());
        texCoords[1] = Vector2D(rc.left(), rc.top());
        texCoords[2] = Vector2D(rc.right(), rc.bottom());
        texCoords[3] = Vector2D(rc.left(), rc.top());
        texCoords[4] = Vector2D(rc.right(), rc.top());
        texCoords[5] = Vector2D(rc.right(), rc.bottom());
    }
}

#endif /* __KIS_PAINTING_TWEAKS_H */
