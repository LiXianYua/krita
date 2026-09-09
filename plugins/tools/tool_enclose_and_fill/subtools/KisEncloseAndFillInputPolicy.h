/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ENCLOSE_AND_FILL_INPUT_POLICY_H
#define KIS_ENCLOSE_AND_FILL_INPUT_POLICY_H

#include <PkNamespace.h>

namespace KisEncloseAndFillInputPolicy
{

inline bool isPrimaryButton(Pk::MouseButton button)
{
    return button == Pk::LeftButton;
}

inline bool acceptsPathPress(Pk::MouseButton button, Pk::KeyboardModifiers modifiers)
{
    const Pk::KeyboardModifiers acceptedModifiers =
        Pk::ShiftModifier | Pk::ControlModifier | Pk::AltModifier;

    return isPrimaryButton(button) &&
        ((modifiers & acceptedModifiers) || modifiers == Pk::NoModifier);
}

}

#endif
