/*
 * SPDX-FileCopyrightText: 2023 Alvin Wong <alvin@alvinhc.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SVG_CREATE_TEXT_STRATEGY_H
#define SVG_CREATE_TEXT_STRATEGY_H

#include <KoInteractionStrategy.h>

#include <PkPoint.h>
#include <PkSize.h>

class SvgTextTool;

class KoSvgTextShape;
class KoShape;

class SvgCreateTextStrategy : public KoInteractionStrategy
{
public:
    SvgCreateTextStrategy(SvgTextTool *tool, const PkPointF &clicked, KoShape *shape = nullptr);
    ~SvgCreateTextStrategy() override = default;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;
    void handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void cancelInteraction() override;
    void finishInteraction(Pk::KeyboardModifiers modifiers) override;

    bool draggingInlineSize();
    bool hasWrappingShape();

private:
    PkPointF m_dragStart;
    PkPointF m_dragEnd;
    PkSizeF m_minSizeInline;
    KoShape *m_flowShape;
    Pk::KeyboardModifiers m_modifiers;
};

#endif /* SVG_CREATE_TEXT_STRATEGY_H */
