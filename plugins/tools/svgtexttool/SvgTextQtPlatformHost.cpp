/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SvgTextQtPlatformHost.h"

#include <QAction>
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
    QObject *collection = controller ? controller->actionCollection() : nullptr;
    QAction *action = collection ? collection->findChild<QAction *>(toQString(actionName)) : nullptr;
    if (!action) {
        return PkKeySequence();
    }

    const QKeySequence shortcut = action->shortcut();
    // `QKeySequence` 的 chord 数上限是 4（Qt 5.15 的 `int key[4]`），`operator[]` 到
    // `count()` 之外返回 0 且不越界。按现场 chord 数收进既有载体——`PkKeySequence` 的
    // `std::initializer_list` 构造函数是它今天唯一的成串入口，故此处按数分支；
    // 内核拿到的是与 Qt 侧等长的序列，之后只做 `size()` / `operator[]` 整数比较。
    switch (shortcut.count()) {
    case 0:  return PkKeySequence();
    case 1:  return PkKeySequence{shortcut[0]};
    case 2:  return PkKeySequence{shortcut[0], shortcut[1]};
    case 3:  return PkKeySequence{shortcut[0], shortcut[1], shortcut[2]};
    default: return PkKeySequence{shortcut[0], shortcut[1], shortcut[2], shortcut[3]};
    }
}
