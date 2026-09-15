/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SvgTextQtPlatformHost.h"

#include <QApplication>
#include <QKeySequence>
#include <QPalette>
#include <QStyle>

#include <PkFlakeBridge.h>

#include <KoCanvasBase.h>
#include <KoCanvasController.h>

SvgTextQtPlatformHost::SvgTextQtPlatformHost(KoCanvasBase *canvas)
    : m_canvas(canvas)
{
}

int SvgTextQtPlatformHost::textCursorWidth() const
{
    return QApplication::style()->pixelMetric(QStyle::PM_TextCursorWidth);
}

int SvgTextQtPlatformHost::cursorFlashTime() const
{
    return qApp->cursorFlashTime();
}

bool SvgTextQtPlatformHost::blinkCursorWhenTextSelected() const
{
    return QApplication::style()->styleHint(QStyle::SH_BlinkCursorWhenTextSelected);
}

PkColor SvgTextQtPlatformHost::highlightColor() const
{
    return toPkColor(qApp->palette().color(QPalette::Highlight));
}

KoCanvasKeyBindingHost::TextCommand
SvgTextQtPlatformHost::textCommand(int key, Pk::KeyboardModifiers modifiers) const
{
    using Command = TextCommand;
    const QKeySequence sequence(static_cast<int>(modifiers) | key);
    if (sequence == QKeySequence::MoveToNextChar) return Command::MoveNextChar;
    if (sequence == QKeySequence::SelectNextChar) return Command::SelectNextChar;
    if (sequence == QKeySequence::MoveToPreviousChar) return Command::MovePreviousChar;
    if (sequence == QKeySequence::SelectPreviousChar) return Command::SelectPreviousChar;
    if (sequence == QKeySequence::MoveToNextLine) return Command::MoveNextLine;
    if (sequence == QKeySequence::SelectNextLine) return Command::SelectNextLine;
    if (sequence == QKeySequence::MoveToPreviousLine) return Command::MovePreviousLine;
    if (sequence == QKeySequence::SelectPreviousLine) return Command::SelectPreviousLine;
    if (sequence == QKeySequence::MoveToNextWord) return Command::MoveNextWord;
    if (sequence == QKeySequence::SelectNextWord) return Command::SelectNextWord;
    if (sequence == QKeySequence::MoveToPreviousWord) return Command::MovePreviousWord;
    if (sequence == QKeySequence::SelectPreviousWord) return Command::SelectPreviousWord;
    if (sequence == QKeySequence::MoveToStartOfLine) return Command::MoveStartOfLine;
    if (sequence == QKeySequence::SelectStartOfLine) return Command::SelectStartOfLine;
    if (sequence == QKeySequence::MoveToEndOfLine) return Command::MoveEndOfLine;
    if (sequence == QKeySequence::SelectEndOfLine) return Command::SelectEndOfLine;
    if (sequence == QKeySequence::MoveToStartOfBlock || sequence == QKeySequence::MoveToStartOfDocument) return Command::MoveStartOfBlock;
    if (sequence == QKeySequence::SelectStartOfBlock || sequence == QKeySequence::SelectStartOfDocument) return Command::SelectStartOfBlock;
    if (sequence == QKeySequence::MoveToEndOfBlock || sequence == QKeySequence::MoveToEndOfDocument) return Command::MoveEndOfBlock;
    if (sequence == QKeySequence::SelectEndOfBlock || sequence == QKeySequence::SelectEndOfDocument) return Command::SelectEndOfBlock;
    if (sequence == QKeySequence::DeleteStartOfWord) return Command::DeleteStartOfWord;
    if (sequence == QKeySequence::DeleteEndOfWord) return Command::DeleteEndOfWord;
    if (sequence == QKeySequence::DeleteEndOfLine) return Command::DeleteEndOfLine;
    if (sequence == QKeySequence::DeleteCompleteLine) return Command::DeleteCompleteLine;
    if (sequence == QKeySequence::Backspace) return Command::Backspace;
    if (sequence == QKeySequence::Delete) return Command::Delete;
    if (sequence == QKeySequence::InsertLineSeparator || sequence == QKeySequence::InsertParagraphSeparator) return Command::InsertLineSeparator;
    return Command::None;
}

PkKeySequence SvgTextQtPlatformHost::actionShortcut(const PkString &actionName) const
{
    KoCanvasController *controller = m_canvas ? m_canvas->canvasController() : nullptr;
    if (!controller) {
        return PkKeySequence();
    }

    // 宿主动作面已改挂内核物化边界（R-70，K-4）：这里不再持有任何动作对象，只经
    // 桶无关的 `KoCanvasActionHost::hostActions()` 拿 identity。`shortcutChords` 是
    // **已解码**的 encoded chord（丢空 chord、逐和弦取 int 都在 native 侧完成，见
    // KoCanvasController::encodeHostActionShortcuts），故此处只按 objectName 找条目、
    // 再按 chord 数还原。语义与旧的 `action->shortcut()` 路径逐条相同：找不到该
    // objectName、或该动作没有非空 shortcut 时，都返回空序列；`PkKeySequence` 的
    // `std::initializer_list` 构造函数是它今天唯一的成串入口，故仍按数分支。
    const PkList<KisHostActionIdentity> identities = controller->hostActions();
    for (int i = 0; i < identities.size(); ++i) {
        const KisHostActionIdentity &identity = identities.at(i);
        if (identity.objectName != actionName) {
            continue;
        }
        if (identity.shortcutChords.isEmpty()) {
            return PkKeySequence();
        }
        // 宿主动作的 chord 数上限是 4（Qt 5.15 的 `int key[4]`），encoded chord
        // 因此最长 4 段；与旧路径一样按数分支，多出来的段不会出现。
        const std::vector<int> &chords = identity.shortcutChords.first();
        switch (chords.size()) {
        case 0:  return PkKeySequence();
        case 1:  return PkKeySequence{chords[0]};
        case 2:  return PkKeySequence{chords[0], chords[1]};
        case 3:  return PkKeySequence{chords[0], chords[1], chords[2]};
        default: return PkKeySequence{chords[0], chords[1], chords[2], chords[3]};
        }
    }

    return PkKeySequence();
}
