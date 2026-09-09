/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TRANSFORM_MODIFIER_POLICY_H
#define TRANSFORM_MODIFIER_POLICY_H

#include <PkNamespace.h>

struct TransformModifierState
{
    bool shift {false};
    bool alt {false};
};

inline TransformModifierState transformModifierState(Pk::KeyboardModifiers modifiers)
{
    return {
        bool(modifiers & Pk::ShiftModifier),
        bool(modifiers & Pk::AltModifier),
    };
}

#endif
