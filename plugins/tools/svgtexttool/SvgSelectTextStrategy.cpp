/*
 * SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgSelectTextStrategy.h"
#include "SvgTextCursor.h"
SvgSelectTextStrategy::SvgSelectTextStrategy(KoToolBase *tool, SvgTextCursor *cursor, const PkPointF &clicked, Pk::KeyboardModifiers modifiers)
    : KoInteractionStrategy(tool)
    , m_cursor(cursor)
    , m_dragStart(clicked)
{
    m_dragEnd = m_dragStart;
    m_cursor->setPosToPoint(m_dragStart, !(modifiers & Pk::ShiftModifier));
}

void SvgSelectTextStrategy::handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    m_dragEnd = mouseLocation;
    if (!(modifiers & Pk::ShiftModifier)) {
        m_cursor->setPosToPoint(m_dragStart, true);
    }
    m_cursor->setPosToPoint(m_dragEnd, false);
}

KUndo2Command *SvgSelectTextStrategy::createCommand()
{
    return nullptr;
}

void SvgSelectTextStrategy::cancelInteraction()
{
    return;
}

void SvgSelectTextStrategy::finishInteraction(Pk::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!(modifiers & Pk::ShiftModifier)) {
        m_cursor->setPosToPoint(m_dragStart, true);
    }
    m_cursor->setPosToPoint(m_dragEnd, false);

}
