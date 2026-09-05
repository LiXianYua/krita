/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_stroke_shortcut.h"

#include "kis_abstract_input_action.h"

#include <QMouseEvent>

#include <cmath>

class Q_DECL_HIDDEN KisStrokeShortcut::Private
{
public:
    QSet<Pk::Key> modifiers;
    QSet<Pk::MouseButton> buttons;
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
    Q_FOREACH (Pk::MouseButton button, m_d->buttons) {
        buttonScore += maxScore - std::log2((int) button);
    }

    return m_d->modifiers.size() * 0xFFFF + buttonScore * 0xFF + action()->priority();
}

void KisStrokeShortcut::setButtons(const QSet<Pk::Key> &modifiers,
                                   const QSet<Pk::MouseButton> &buttons)
{
    if (buttons.empty()) return;

    m_d->modifiers = modifiers;
    m_d->buttons = buttons;
}

bool KisStrokeShortcut::matchReady(const QSet<Pk::Key> &modifiers,
                                   const QSet<Pk::MouseButton> &buttons)
{
    bool modifiersOk =
        (m_d->modifiers.isEmpty() && action()->canIgnoreModifiers()) ||
        compareKeys(m_d->modifiers, modifiers);

    if (!modifiersOk || buttons.size() < m_d->buttons.size() - 1) {
        return false;
    }

    Q_FOREACH (Pk::MouseButton button, buttons) {
        if (!m_d->buttons.contains(button)) return false;
    }
    return true;
}

bool KisStrokeShortcut::matchBegin(Pk::MouseButton button)
{
    return m_d->buttons.contains(button);
}

QMouseEvent KisStrokeShortcut::fakeEndEvent(const QPointF &localPos) const
{
    Pk::MouseButton button = !m_d->buttons.isEmpty() ? *m_d->buttons.begin() : Pk::NoButton;
    return QMouseEvent(QEvent::MouseButtonRelease, localPos,
                       static_cast<Qt::MouseButton>(button),
                       static_cast<Qt::MouseButton>(Pk::NoButton),
                       static_cast<Qt::KeyboardModifiers>(Pk::NoModifier));
}
