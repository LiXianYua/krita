/*
 * SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SVGSELECTTEXTSTRATEGY_H
#define SVGSELECTTEXTSTRATEGY_H

#include <KoInteractionStrategy.h>
#include <PkPoint.h>

class SvgTextCursor;

class SvgSelectTextStrategy : public KoInteractionStrategy
{
public:
    SvgSelectTextStrategy(KoToolBase *tool, SvgTextCursor *cursor, const PkPointF &clicked, Qt::KeyboardModifiers modifiers);
    ~SvgSelectTextStrategy() override = default;

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void cancelInteraction() override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override;

private:
    SvgTextCursor *m_cursor;
    PkPointF m_dragStart;
    PkPointF m_dragEnd;
};

#endif // SVGSELECTTEXTSTRATEGY_H
