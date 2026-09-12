/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Key-binding contract between retained svg text code and the UI host.
 */
#ifndef KOCANVASKEYBINDINGHOST_H
#define KOCANVASKEYBINDINGHOST_H

#include <PkKeySequence.h>
#include <PkNamespace.h>
#include <PkString.h>

#include "kritaflake_export.h"

class KRITAFLAKE_EXPORT KoCanvasKeyBindingHost
{
public:
    /** 内核侧的按键绑定标识。**不是 Qt 的 StandardKey**：内核只认这套词汇，
        「平台把哪个组合键绑给哪个标准动作」由宿主回答。 */
    enum class TextCommand {
        None,
        MoveNextChar,
        SelectNextChar,
        MovePreviousChar,
        SelectPreviousChar,
        MoveNextLine,
        SelectNextLine,
        MovePreviousLine,
        SelectPreviousLine,
        MoveNextWord,
        SelectNextWord,
        MovePreviousWord,
        SelectPreviousWord,
        MoveStartOfLine,
        SelectStartOfLine,
        MoveEndOfLine,
        SelectEndOfLine,
        MoveStartOfBlock,
        SelectStartOfBlock,
        MoveEndOfBlock,
        SelectEndOfBlock,
        DeleteStartOfWord,
        DeleteEndOfWord,
        DeleteEndOfLine,
        DeleteCompleteLine,
        Backspace,
        Delete,
        InsertLineSeparator
    };

    virtual ~KoCanvasKeyBindingHost() = default;

    /** 把一个 chord（`key | modifiers`，Qt 5.15 编码）解析成绑定标识。
        无宿主时 = None（「宿主没告诉我」）。 */
    virtual TextCommand textCommand(int key, Pk::KeyboardModifiers modifiers) const
    {
        (void)key;
        (void)modifiers;
        return TextCommand::None;
    }

    /** 名为 `actionName` 的宿主动作今天绑定的那个 chord（PkKeySequence，空 = 没绑）。
        内核只做整数比较，不构造任何 Qt 序列对象。 */
    virtual PkKeySequence actionShortcut(const PkString &actionName) const
    {
        (void)actionName;
        return PkKeySequence();
    }
};

#endif // KOCANVASKEYBINDINGHOST_H
