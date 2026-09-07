/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2016 Michael Abrahams <miabraha@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

/**
 * This is a basic template to create selection tools from basic path based drawing tools.
 * The template overrides the ability to execute alternate actions correctly.
 * Modifier keys are overridden with the following behavior:
 *
 * Shift: add to selection
 * Alt: subtract from selection
 * Shift+Alt: intersect current selection
 * Ctrl: replace selection
 *
 * Certain tools also use modifier keys to alter their behavior, e.g. forcing square proportions with the rectangle tool.
 * The template enables the following rules for forwarding keys:
 * 1) Any modifier keys held *when the tool is first activated* will determine the new selection method.
 * 2) If the underlying tool *does not take modifier keys*, pressing modifier keys in the middle of a stroke will change the selection method.  This applies to the lasso tool and polygon tool.
 * 3) If the underlying tool *takes modifier keys,* they will always be forwarded to the underlying tool, and it is not possible to change the selection method in the middle of a stroke.
 */

#include "kis_selection.h"
#include "kis_selection_modifier_mapper.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

void connectSelectionModifierMapperToConfigChanges(KisSelectionModifierMapper *mapper);

// This numerically serializes modifier flags... let's keep it around for later.
#if 0
#include <bitset>
PkString modifierBinary(Pk::KeyboardModifiers m)
{
    return PkString(std::bitset<sizeof(int) * 8>(m).to_string().c_str());
};
#endif

struct KisSelectionModifierMapper::Private
{
    SelectionAction map(Pk::KeyboardModifiers m);
    void slotConfigChanged();
    Pk::KeyboardModifiers replaceModifiers;
    Pk::KeyboardModifiers intersectModifiers;
    Pk::KeyboardModifiers addModifiers;
    Pk::KeyboardModifiers subtractModifiers;
    Pk::KeyboardModifiers symmetricdifferenceModifiers;
};


KisSelectionModifierMapper::KisSelectionModifierMapper()
    : m_d(new Private)
{
    connectSelectionModifierMapperToConfigChanges(this);
    slotConfigChanged();
}


KisSelectionModifierMapper::~KisSelectionModifierMapper()
{
}

KisSelectionModifierMapper *KisSelectionModifierMapper::instance()
{
    static KisSelectionModifierMapper instance;
    return &instance;
}

void KisSelectionModifierMapper::slotConfigChanged()
{
    m_d->slotConfigChanged();
}


void KisSelectionModifierMapper::Private::slotConfigChanged()
{
    const bool switchSelectionCtrlAlt =
        PkSharedConfig::openConfig()->group(PkString()).readEntry("switchSelectionCtrlAlt", false);
    if (!switchSelectionCtrlAlt) {
        replaceModifiers   = Pk::ControlModifier;
        intersectModifiers = Pk::AltModifier | Pk::ShiftModifier;
        subtractModifiers  = Pk::AltModifier;
        symmetricdifferenceModifiers = Pk::ControlModifier | Pk::AltModifier;
    } else {
        replaceModifiers   = Pk::AltModifier;
        intersectModifiers = Pk::ControlModifier | Pk::ShiftModifier;
        subtractModifiers  = Pk::ControlModifier;
        symmetricdifferenceModifiers = Pk::AltModifier | Pk::ControlModifier;
    }

    addModifiers = Pk::ShiftModifier;
}

SelectionAction KisSelectionModifierMapper::map(Pk::KeyboardModifiers m)
{
    return instance()->m_d->map(m);
}

SelectionAction KisSelectionModifierMapper::Private::map(Pk::KeyboardModifiers m)
{
    SelectionAction newAction = SELECTION_DEFAULT;
    if (m == replaceModifiers) {
        newAction = SELECTION_REPLACE;
    } else if (m == intersectModifiers) {
        newAction = SELECTION_INTERSECT;
    } else if (m == addModifiers) {
        newAction = SELECTION_ADD;
    } else if (m == subtractModifiers) {
        newAction = SELECTION_SUBTRACT;
    } else if (m == symmetricdifferenceModifiers) {
        newAction = SELECTION_SYMMETRICDIFFERENCE;
    }
        
    return newAction;
}
