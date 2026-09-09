/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PluginDInputPolicyTest.h"

#include "../TransformModifierPolicy.h"
#include "../../tool_enclose_and_fill/subtools/KisEncloseAndFillInputPolicy.h"

#include "kistest.h"
#include <Qt>

void PluginDInputPolicyTest::enclosePathPolicyMatchesQt515()
{
    const Qt::MouseButton buttons[] = {
        Qt::NoButton,
        Qt::LeftButton,
        Qt::RightButton,
        Qt::MiddleButton,
    };
    const Qt::KeyboardModifiers modifiers[] = {
        Qt::NoModifier,
        Qt::ShiftModifier,
        Qt::ControlModifier,
        Qt::AltModifier,
        Qt::MetaModifier,
        Qt::ShiftModifier | Qt::MetaModifier,
        Qt::ControlModifier | Qt::AltModifier,
    };

    for (Qt::MouseButton button : buttons) {
        for (Qt::KeyboardModifiers modifier : modifiers) {
            const bool qt515Accepts =
                button == Qt::LeftButton &&
                ((modifier & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier)) ||
                 modifier == Qt::NoModifier);
            QCOMPARE(KisEncloseAndFillInputPolicy::acceptsPathPress(
                         static_cast<Pk::MouseButton>(button),
                         Pk::KeyboardModifiers(static_cast<int>(modifier))),
                     qt515Accepts);
        }
    }
}

void PluginDInputPolicyTest::transformModifierPolicyMatchesQt515()
{
    const Qt::KeyboardModifiers modifiers[] = {
        Qt::NoModifier,
        Qt::ShiftModifier,
        Qt::AltModifier,
        Qt::ControlModifier,
        Qt::MetaModifier,
        Qt::ShiftModifier | Qt::AltModifier,
        Qt::ShiftModifier | Qt::MetaModifier,
    };

    for (Qt::KeyboardModifiers modifier : modifiers) {
        const TransformModifierState state = transformModifierState(
            Pk::KeyboardModifiers(static_cast<int>(modifier)));
        QCOMPARE(state.shift, bool(modifier & Qt::ShiftModifier));
        QCOMPARE(state.alt, bool(modifier & Qt::AltModifier));
    }
}

KISTEST_MAIN(PluginDInputPolicyTest)
