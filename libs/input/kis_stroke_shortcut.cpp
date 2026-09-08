/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_stroke_shortcut.h"

#include "kis_abstract_input_action.h"

#include <cmath>

class KisStrokeShortcut::Private
{
public:
    PkSet<Pk::Key> modifiers;
    PkSet<Pk::MouseButton> buttons;
};


KisStrokeShortcut::KisStrokeShortcut(KisAbstractInputAction *action, int index)
    : KisAbstractShortcut(action, index),
      m_d(new Private)
{
}

KisStrokeShortcut::~KisStrokeShortcut()
{
    delete m_d;
}

int KisStrokeShortcut::priority() const
{
    const int maxScore = std::log2((int) Pk::MaxMouseButton);
    int buttonScore = 0;
    for (Pk::MouseButton button : m_d->buttons) {
        buttonScore += maxScore - std::log2((int) button);
    }

    return m_d->modifiers.size() * 0xFFFF + buttonScore * 0xFF + action()->priority();
}

void KisStrokeShortcut::setButtons(const PkSet<Pk::Key> &modifiers,
                                   const PkSet<Pk::MouseButton> &buttons)
{
    if (buttons.isEmpty()) return;

    m_d->modifiers = modifiers;
    m_d->buttons = buttons;
}

bool KisStrokeShortcut::matchReady(const PkSet<Pk::Key> &modifiers,
                                   const PkSet<Pk::MouseButton> &buttons)
{
    bool modifiersOk =
        (m_d->modifiers.isEmpty() && action()->canIgnoreModifiers()) ||
        compareKeys(m_d->modifiers, modifiers);

    if (!modifiersOk || buttons.size() < m_d->buttons.size() - 1) {
        return false;
    }

    for (Pk::MouseButton button : buttons) {
        if (!m_d->buttons.contains(button)) return false;
    }
    return true;
}

bool KisStrokeShortcut::matchBegin(Pk::MouseButton button)
{
    return m_d->buttons.contains(button);
}

PkInputEvent KisStrokeShortcut::fakeEndEvent(const PkPointF &localPos) const
{
    Pk::MouseButton button = !m_d->buttons.isEmpty() ? *m_d->buttons.begin() : Pk::NoButton;
    return PkInputEvent(PkInputEvent::MouseButtonRelease,
                        localPos,
                        localPos,
                        localPos,
                        button,
                        Pk::NoButton,
                        Pk::NoModifier);
}
