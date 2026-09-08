/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SVGTEXTTYPESETTINGSTRATEGY_H
#define SVGTEXTTYPESETTINGSTRATEGY_H

#include <KoInteractionStrategy.h>
#include <KoSvgTextShape.h>
#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkRect.h>

class SvgTextCursor;
class KoSvgTextShape;

/**
 * @brief The SvgTextTypeSettingStrategy class
 * This class encompasses the typesetting mode.
 */
class SvgTextTypeSettingStrategy: public KoInteractionStrategy
{
public:
    SvgTextTypeSettingStrategy(KoToolBase *tool, KoSvgTextShape *textShape, SvgTextCursor *textCursor, const PkRectF &regionOfInterest, Pk::KeyboardModifiers modifiers = Pk::NoModifier);

    // KoInteractionStrategy interface
public:
    void handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void cancelInteraction() override;
    void finishInteraction(Pk::KeyboardModifiers modifiers) override;

private:
    KoSvgTextShape *m_shape;
    PkPointF m_dragStart;
    PkPointF m_dragCurrent;
    PkPointF m_currentDelta;

    int m_cursorPos;
    int m_cursorAnchor;
    int m_editingType;
    int m_referenceCursorPos;

    bool m_deltaCalc;
    Pk::KeyboardModifiers m_modifiers;

    QScopedPointer<KUndo2Command> m_previousCmd;
    KoSvgTextShapeMementoSP m_textData;
};

#endif // SVGTEXTTYPESETTINGSTRATEGY_H
