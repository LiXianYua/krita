/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SVGTEXTONPATHDECORATIONHELPER_H
#define SVGTEXTONPATHDECORATIONHELPER_H

#include <PkPoint.h>
#include <PkRect.h>
#include <PkTransform.h>

#include <memory>


class KoSvgTextShape;
class KoViewConverter;
class PkPainter;
/**
 * @brief The SvgTextOnPathDecorationHelper class
 */
class SvgTextOnPathDecorationHelper
{
public:
    SvgTextOnPathDecorationHelper();
    ~SvgTextOnPathDecorationHelper();

    /**
     * @brief setPos
     * the the position of the cursor, where
     * the text path is sought. A single text shape
     * can have multiple paths, and this is how they
     * are distinguished.
     * @param pos -- a text cursor position.
     */
    void setPos(int pos);

    /**
     * @brief setShape
     * Set the shape for which to draw the text path.
     * @param shape -- text shape.
     */
    void setShape(KoSvgTextShape *shape);

    void setHandleRadius(qreal radius);
    void setDecorationThickness(qreal thickness);

    /**
     * @brief hitTest
     * @return whether the current position is over a handle.
     */
    bool hitTest(PkPointF mouseInPts, const PkTransform &viewToDocument);

    /**
     * @brief paint
     * Paint the handles for the text path.
     */
    void paint(PkPainter *painter, const KoViewConverter &converter);

    /**
     * @brief decorationRect
     * @param documentToView -- document to view transform.
     * @return the decoration rect.
     */
    PkRectF decorationRect(const PkTransform &documentToView) const;

    void setStrategyActive(bool isActive);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

#endif // SVGTEXTONPATHDECORATIONHELPER_H
