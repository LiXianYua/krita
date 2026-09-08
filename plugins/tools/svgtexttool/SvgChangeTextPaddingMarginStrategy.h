/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SVGCHANGETEXTPADDINGMARGINSTRATEGY_H
#define SVGCHANGETEXTPADDINGMARGINSTRATEGY_H

#include <KoInteractionStrategy.h>

#include <KoSvgTextShape.h>
#include <optional>

class KoPathShape;

class SvgChangeTextPaddingMarginStrategy : public KoInteractionStrategy
{
public:
    SvgChangeTextPaddingMarginStrategy(KoToolBase *tool, KoSvgTextShape *shape, const PkPointF &clicked);
    ~SvgChangeTextPaddingMarginStrategy();


    /**
     * @brief hitTest
     * Tests whether the current mouse position is over a text wrapping area,
     * and if so, will return the angle vector at that point.
     * @param shape -- shape to test for.
     * @param mousePos -- mousePos to test against.
     * @param grabSensitivityInPts -- grabSensitivity in Points
     * @return -- std::optional containing the angle vector.
     */
    static std::optional<PkPointF> hitTest(KoSvgTextShape *shape, const PkPointF &mousePos, const qreal grabSensitivityInPts);
private:
    KoSvgTextShape *m_shape;
    KoPathShape *m_referenceShape;
    bool m_isPadding;
    PkPointF m_lastMousePos;

    // KoInteractionStrategy interface
public:
    void paint(PkPainter &painter, const KoViewConverter &converter) override;
    void handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void finishInteraction(Pk::KeyboardModifiers modifiers) override;
};

#endif // SVGCHANGETEXTPADDINGMARGINSTRATEGY_H
