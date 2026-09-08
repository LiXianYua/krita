/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_single_action_shortcut.h"

#include <PkVector.h>
#include "kis_abstract_input_action.h"

class KisSingleActionShortcut::Private
{
public:
    PkSet<Pk::Key> modifiers;
    Pk::Key key = Pk::Key_Space;
    bool useWheel = false;
    WheelAction wheelAction = WheelUp;
};


KisSingleActionShortcut::KisSingleActionShortcut(KisAbstractInputAction *action, int index)
    : KisAbstractShortcut(action, index),
      m_d(new Private)
{
}

KisSingleActionShortcut::~KisSingleActionShortcut()
{
    delete m_d;
}

int KisSingleActionShortcut::priority() const
{
    return m_d->modifiers.size() * 2 + 1 + action()->priority();
}

void KisSingleActionShortcut::setKey(const PkSet<Pk::Key> &modifiers, Pk::Key key)
{
    m_d->modifiers = modifiers;
    m_d->key = key;
    m_d->useWheel = false;
}

void KisSingleActionShortcut::setWheel(const PkSet<Pk::Key> &modifiers, WheelAction wheelAction)
{
    m_d->modifiers = modifiers;
    m_d->wheelAction = wheelAction;
    m_d->useWheel = true;
}

bool KisSingleActionShortcut::match(const PkSet<Pk::Key> &modifiers, Pk::Key key)
{
    return !m_d->useWheel && key == m_d->key &&
        compareKeys(modifiers, m_d->modifiers);
}

bool KisSingleActionShortcut::match(const PkSet<Pk::Key> &modifiers, WheelAction wheelAction)
{
    return m_d->useWheel && wheelAction == m_d->wheelAction &&
        compareKeys(modifiers, m_d->modifiers);
}

bool KisSingleActionShortcut::conflictsWith(const PkKeySequence &seq)
{
    if (seq.isEmpty()) return false;

    int seqMainKey = seq[0];
    PkVector<int> sequenceKeys;

    if (seqMainKey & Pk::MetaModifier) {
        sequenceKeys.append(Pk::Key_Meta);
        seqMainKey &= ~Pk::MetaModifier;
    } else if (seqMainKey & Pk::ControlModifier) {
        sequenceKeys.append(Pk::Key_Control);
        seqMainKey &= ~Pk::ControlModifier;
    } else if (seqMainKey & Pk::ShiftModifier) {
        sequenceKeys.append(Pk::Key_Shift);
        seqMainKey &= ~Pk::ShiftModifier;
    } else if (seqMainKey & Pk::AltModifier) {
        sequenceKeys.append(Pk::Key_Alt);
        seqMainKey &= ~Pk::AltModifier;
    }

    if (seqMainKey == 0) return false;
    sequenceKeys.append(seqMainKey);
    std::sort(sequenceKeys.begin(), sequenceKeys.end());

    PkVector<int> shortcutKeys;
    std::copy(m_d->modifiers.begin(), m_d->modifiers.end(), std::back_inserter(shortcutKeys));
    shortcutKeys.append(m_d->key);
    std::sort(shortcutKeys.begin(), shortcutKeys.end());

    return
        std::includes(sequenceKeys.begin(), sequenceKeys.end(),
                      shortcutKeys.begin(), shortcutKeys.end()) ||
        std::includes(shortcutKeys.begin(), shortcutKeys.end(),
                      sequenceKeys.begin(), sequenceKeys.end());
}
