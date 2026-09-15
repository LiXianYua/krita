/*
 * SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgTextCursorTest.h"

#include <SvgTextCursor.h>
#include <SvgTextInsertCommand.h>
#include <SvgTextRemoveCommand.h>
#include <SvgTextChangeTransformsOnRange.h>
#include <SvgTextShortCuts.h>
#include <SvgTextTool.h>
#include <SvgTextToolFactory.h>
#include <SvgTextToolResources.h>
#include <SvgTextToolOptionsData.h>
#include <SvgTextInputMethodAdapter.h>
#include <KisDocumentApplicationServices.h>
#include <PkConfigGroup.h>
#include <PkThreadCallQueue.h>
#include <kundo2command.h>
#include <KConfig>
#include <KConfigGroup>
#include <QTemporaryDir>

#include <KoSvgTextShape.h>
#include <KoSvgTextShapeMarkupConverter.h>
#include <KoFontRegistry.h>
#include <KoCanvasController.h>
#include <SvgTextQtPlatformHost.h>
#include <KoToolRegistry.h>
#include <KoViewConverter.h>
#include <kis_coordinates_converter.h>
#include <QCryptographicHash>
#include <QImage>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QTextCharFormat>
#include <QWidget>

#include <chrono>
#include <thread>

#include <tests/MockShapes.h>
#include <simpletest.h>
#include <testui.h>

Q_DECLARE_METATYPE(SvgTextCursor::MoveMode)

SvgTextCursor::NativeKeyEvent
svgTextNativeKeyEvent(const PkToolKeyEvent &event,
                      KoSvgText::WritingMode writingMode,
                      KoSvgText::Direction direction,
                      const KoCanvasKeyBindingHost *bindingHost = nullptr);

namespace {
class CursorCanvas final : public MockCanvas
{
public:
    QWidget window;
    QWidget widget{&window};
    KoViewConverter converter;
    QWidget *canvasWidget() override { return &widget; }
    const QWidget *canvasWidget() const override { return &widget; }
    KoViewConverter *viewConverter() override { return &converter; }
    const KoViewConverter *viewConverter() const override { return &converter; }
};

class CursorController final : public KoCanvasController
{
public:
    explicit CursorController(PkObject *actionCollection = nullptr)
        : KoCanvasController(actionCollection)
    {
    }
    void setCanvas(KoCanvasBase *value) override { currentCanvas = value; }
    KoCanvasBase *canvas() const override { return currentCanvas; }
    void ensureVisibleDoc(const PkRectF &, bool) override {}
    void zoomIn(const KoViewTransformStillPoint &) override {}
    void zoomIn() override {}
    void zoomOut(const KoViewTransformStillPoint &) override {}
    void zoomOut() override {}
    void zoomTo(const PkRect &) override {}
    void setZoom(KoZoomMode::Mode, qreal) override {}
    void setPreferredCenter(const PkPointF &) override {}
    PkPointF preferredCenter() const override { return {}; }
    void pan(const PkPoint &) override {}
    void panUp() override {}
    void panDown() override {}
    void panLeft() override {}
    void panRight() override {}
    PkPoint scrollBarValue() const override { return {}; }
    void setScrollBarValue(const PkPoint &) override {}
    void resetScrollBars() override {}
    PkPointF currentCursorPosition() const override { return {}; }
    KoZoomState zoomState() const override { return {}; }

    // 桶无关宿主动作面（R-70）：controller 既报 identity（读路，SvgTextQtPlatformHost
    // 的 actionShortcut 从这里取），也记录工具写回的启用态（写路，slotTextTypeUpdated
    // 的 setHostActionEnabled）。两条都是可写的测试观测点。
    PkList<KisHostActionIdentity> hostActions() const override { return hostActionIdentities; }
    void setHostActionEnabled(const PkString &objectName, bool enabled) override
    {
        hostActionEnabledWrites.insert(objectName, enabled);
    }

    PkList<KisHostActionIdentity> hostActionIdentities;
    PkMap<PkString, bool> hostActionEnabledWrites;

    KoCanvasBase *currentCanvas = nullptr;
};

/**
 * 宿主的按键绑定面 mock（R-70，impact-map §5 / R-69 交接的 latent）。
 *
 * 本文件此前**没有** `KoCanvasKeyBindingHost` 的任何实现，而
 * `SvgTextTool::pkKeyPressEvent` 用 `dynamic_cast<const KoCanvasKeyBindingHost *>(canvas())`
 * 取它 ⇒ 恒 null ⇒ `svgTextNativeKeyEvent` 的 `command` 恒 `None`、`hostActionDispatch*`
 * 一族的兜底派发分支永不执行。补这份 mock 是让那些断言**真的有判别力地跑起来**，
 * 不是放宽断言。
 *
 * `textCommand` 是 `SvgTextQtPlatformHost::textCommand` 那张 Qt 5.15 表的逐字副本：
 * 生产侧的宿主适配器与测试侧的 mock 回答同一张表，内核拿到同一套绑定标识。
 * `actionShortcuts` 由测试直接写：键是宿主动作的 objectName，值是它今天绑的 chord
 * （形态与生产侧 `KoCanvasActionHost::hostActions()` 的 `shortcutChords` 一致 —— 已解码的 int）。
 */
class MockKeyBindingHost : public KoCanvasKeyBindingHost
{
public:
    KoCanvasKeyBindingHost::TextCommand textCommand(int key, Pk::KeyboardModifiers modifiers) const override
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

    PkKeySequence actionShortcut(const PkString &actionName) const override
    {
        // 没登记 = 该动作今天没绑 = 空序列（PkMap::value 对缺项返回 V()）。
        return actionShortcuts.value(actionName);
    }

    PkMap<PkString, PkKeySequence> actionShortcuts;
};

class ApplyingCanvas final : public MockCanvas, public MockKeyBindingHost
{
public:
    void addCommand(KUndo2Command *command) override
    {
        if (command) {
            command->redo();
            delete command;
        }
    }
};

class HostToolCanvas final : public MockCanvas, public MockKeyBindingHost
{
public:
    explicit HostToolCanvas(KoShapeControllerBase *shapeController)
        : MockCanvas(shapeController)
    {
    }

    QWidget window;
    QWidget widget{&window};
    KisCoordinatesConverter converter;

    void addCommand(KUndo2Command *command) override
    {
        if (command) {
            command->redo();
            delete command;
        }
    }
    QWidget *canvasWidget() override { return &widget; }
    const QWidget *canvasWidget() const override { return &widget; }
    KoViewConverter *viewConverter() override { return &converter; }
    const KoViewConverter *viewConverter() const override { return &converter; }
};

class ClipboardApplicationServices final : public KisDocumentApplicationServices
{
public:
    ClipboardData clipboardData() const override
    {
        ++readCount;
        return useSecondRead && readCount > 1 ? secondReadData : data;
    }
    void setClipboardData(const ClipboardData &value) override
    {
        data = value;
        ++writeCount;
    }

    ClipboardData data;
    ClipboardData secondReadData;
    mutable int readCount = 0;
    int writeCount = 0;
    bool useSecondRead = false;

    void updateInputMethod(Pk::InputMethodQueries queries) override
    {
        lastQueries = queries;
        ++updateCount;
    }
    void setInputMethodVisible(bool visible) override { inputMethodVisible = visible; }
    void invokeInputMethodAction(InputMethodAction action, int cursorPosition) override
    {
        lastAction = action;
        lastActionPosition = cursorPosition;
    }
    void setInputMethodItemTransform(const PkTransform &transform) override { inputItemTransform = transform; }
    void setInputMethodItemRectangle(const PkRectF &rect) override { inputItemRectangle = rect; }
    void commitInputMethod() override { ++commitCount; }

    Pk::InputMethodQueries lastQueries;
    InputMethodAction lastAction = InputMethodAction::Click;
    int lastActionPosition = -1;
    PkTransform inputItemTransform;
    PkRectF inputItemRectangle;
    int updateCount = 0;
    int commitCount = 0;
    bool inputMethodVisible = false;
};

class ApplicationServicesGuard final
{
public:
    explicit ApplicationServicesGuard(KisDocumentApplicationServices *services)
    {
        KisDocumentApplicationServices::setInstance(services);
    }

    ~ApplicationServicesGuard()
    {
        KisDocumentApplicationServices::setInstance(nullptr);
    }
};

class NativeCursorHost final : public SvgTextCursor::HostSurface
{
public:
    bool isAvailable() const override { return available; }
    bool hasFocus() const override { return focused; }
    PkPoint offsetInWindow() const override { return offset; }
    PkRectF geometry() const override { return rect; }

    bool available = true;
    bool focused = true;
    PkPoint offset{17, 23};
    PkRectF rect{0, 0, 640, 480};
};

KisDocumentApplicationServices::InputMethodTextFormat nativeTextFormat(const QTextCharFormat &format)
{
    using Services = KisDocumentApplicationServices;
    Services::InputMethodTextFormat result;
    if (format.hasProperty(QTextFormat::FontUnderline)) {
        result.underline = format.property(QTextFormat::FontUnderline).toBool();
    }
    if (format.hasProperty(QTextFormat::FontOverline)) {
        result.overline = format.property(QTextFormat::FontOverline).toBool();
    }
    if (format.hasProperty(QTextFormat::FontStrikeOut)) {
        result.strikeOut = format.property(QTextFormat::FontStrikeOut).toBool();
    }
    if (format.hasProperty(QTextFormat::TextUnderlineStyle)) {
        const QTextCharFormat::UnderlineStyle style = format.underlineStyle();
        result.underline = style != QTextCharFormat::NoUnderline;
        if (style == QTextCharFormat::DotLine) {
            result.style = Services::InputMethodLineStyle::Dotted;
        } else if (style == QTextCharFormat::DashUnderline) {
            result.style = Services::InputMethodLineStyle::Dashed;
        } else if (style == QTextCharFormat::WaveUnderline || style == QTextCharFormat::SpellCheckUnderline) {
            result.style = Services::InputMethodLineStyle::Wavy;
#ifdef Q_OS_MACOS
            if (style == QTextCharFormat::SpellCheckUnderline) {
                result.style = Services::InputMethodLineStyle::Dotted;
            }
#endif
        }
    }
    if (format.hasProperty(QTextFormat::BackgroundBrush)) {
        result.thick = format.background().isOpaque();
#ifdef Q_OS_LINUX
        if (result.style == Services::InputMethodLineStyle::Dashed) {
            result.style = Services::InputMethodLineStyle::Solid;
        }
#endif
    }
    if (!result.underline && !result.overline && !result.strikeOut) {
        result.underline = true;
    }
    return result;
}

KisDocumentApplicationServices::InputMethodEvent
svgTextNativeInputMethodEvent(const QInputMethodEvent &event)
{
    using Services = KisDocumentApplicationServices;
    Services::InputMethodEvent result;
    result.commitString = toPkString(event.commitString());
    result.preeditString = toPkString(event.preeditString());
    result.replacementStart = event.replacementStart();
    result.replacementLength = event.replacementLength();
    for (const QInputMethodEvent::Attribute &attribute : event.attributes()) {
        Services::InputMethodAttribute nativeAttribute;
        nativeAttribute.start = attribute.start;
        nativeAttribute.length = attribute.length;
        if (attribute.type == QInputMethodEvent::Selection) {
            nativeAttribute.type = Services::InputMethodAttributeType::Selection;
        } else if (attribute.type == QInputMethodEvent::Cursor) {
            nativeAttribute.type = Services::InputMethodAttributeType::Cursor;
        } else if (attribute.type == QInputMethodEvent::TextFormat) {
            if (attribute.length == 0 || attribute.start < 0 || !attribute.value.isValid()) {
                continue;
            }
            nativeAttribute.type = Services::InputMethodAttributeType::TextFormat;
            nativeAttribute.format = nativeTextFormat(attribute.value.value<QTextFormat>().toCharFormat());
        } else {
            continue;
        }
        result.attributes.append(nativeAttribute);
    }
    return result;
}

}

void SvgTextCursorTest::clipboardCopyAndPasteShareInjectedApplicationService()
{
    ClipboardApplicationServices services;
    ApplicationServicesGuard guard(&services);

    KoSvgTextShape source;
    KoSvgTextShapeMarkupConverter sourceConverter(&source);
    QVERIFY(sourceConverter.convertFromSvg("<text font-size=\"10\">Hello</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas sourceCanvas;
    SvgTextCursor sourceCursor(&sourceCanvas);
    sourceCursor.setShape(&source);
    sourceCursor.setPos(source.posForIndex(5), source.posForIndex(0));
    sourceCursor.copy();

    QCOMPARE(services.writeCount, 1);
    const KisDocumentApplicationServices::ClipboardData copied = services.data;
    QVERIFY(copied.hasText);
    QVERIFY(copied.hasHtml);
    QVERIFY(copied.hasSvg);
    QCOMPARE(toQString(copied.text), QStringLiteral("Hello"));
    QVERIFY(!copied.html.isEmpty());
    QVERIFY(!copied.svg.isEmpty());

    // QMimeData is the independent Qt 5.15 oracle for the consumed presence
    // semantics; production cursor code must not depend on it.
    QMimeData oracle;
    oracle.setText(toQString(copied.text));
    oracle.setHtml(toQString(copied.html));
    oracle.setData(QStringLiteral("image/svg+xml"), QByteArray(copied.svg.data(), int(copied.svg.size())));
    QVERIFY(oracle.hasText());
    QVERIFY(oracle.hasHtml());
    QVERIFY(oracle.hasFormat(QStringLiteral("image/svg+xml")));

    services.data.text = "PLAIN-MARKER";
    services.data.html = "<p>HTML-MARKER</p>";
    KoSvgTextShape target;
    KoSvgTextShapeMarkupConverter targetConverter(&target);
    QVERIFY(targetConverter.convertFromSvg("<text font-size=\"10\">Before</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas targetCanvas;
    SvgTextCursor targetCursor(&targetCanvas);
    targetCursor.setShape(&target);
    const int end = target.posForIndex(target.plainText().size());
    targetCursor.setPos(end, end);
    QVERIFY(targetCursor.paste());
    QCOMPARE(toQString(target.plainText()), QStringLiteral("BeforeHello"));
}

void SvgTextCursorTest::clipboardPreservesEmptyTextPresence()
{
    ClipboardApplicationServices services;
    ApplicationServicesGuard guard(&services);
    services.data.hasText = true;
    services.data.text = {};

    KoSvgTextShape target;
    KoSvgTextShapeMarkupConverter converter(&target);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">Unchanged</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(&target);
    cursor.setPasteRichTextByDefault(false);
    QVERIFY(cursor.paste());
    QCOMPARE(toQString(target.plainText()), QStringLiteral("Unchanged"));
}

void SvgTextCursorTest::clipboardRichFallbackUsesSingleSnapshot()
{
    ClipboardApplicationServices services;
    ApplicationServicesGuard guard(&services);
    services.data.hasSvg = true;
    services.data.svg = "not svg";
    services.data.hasText = true;
    services.data.text = "FIRST";
    services.secondReadData.hasText = true;
    services.secondReadData.text = "SECOND";
    services.useSecondRead = true;

    KoSvgTextShape target;
    KoSvgTextShapeMarkupConverter converter(&target);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">Before</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(&target);
    const int end = target.posForIndex(target.plainText().size());
    cursor.setPos(end, end);
    QVERIFY(cursor.paste());
    QCOMPARE(services.readCount, 1);
    QCOMPARE(toQString(target.plainText()), QStringLiteral("BeforeFIRST"));
}

void SvgTextCursorTest::nativeInputMethodEventPreservesEditingLifecycle()
{
    ClipboardApplicationServices services;
    ApplicationServicesGuard guard(&services);
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;

    {
        SvgTextCursor cursor(&canvas);
        cursor.setShape(&shape);
        const int end = shape.posForIndex(shape.plainText().size());
        cursor.setPos(end, end);

        KisDocumentApplicationServices::InputMethodEvent preedit;
        preedit.preeditString = "X";
        KisDocumentApplicationServices::InputMethodAttribute cursorAttribute;
        cursorAttribute.type = KisDocumentApplicationServices::InputMethodAttributeType::Cursor;
        cursorAttribute.start = 1;
        cursorAttribute.length = 1;
        preedit.attributes.append(cursorAttribute);
        QVERIFY(cursor.inputMethodEvent(preedit));
        QCOMPARE(toQString(shape.plainText()), QStringLiteral("abcX"));
        QCOMPARE(toQString(cursor.inputMethodQuery(Pk::ImSurroundingText).toString()), QStringLiteral("abc"));

        KisDocumentApplicationServices::InputMethodEvent commit;
        commit.commitString = "Y";
        QVERIFY(cursor.inputMethodEvent(commit));
        QCOMPARE(toQString(shape.plainText()), QStringLiteral("abcY"));

        KisDocumentApplicationServices::InputMethodEvent replacement;
        replacement.commitString = "Z";
        replacement.replacementStart = -1;
        replacement.replacementLength = 1;
        QVERIFY(cursor.inputMethodEvent(replacement));
        QCOMPARE(toQString(shape.plainText()), QStringLiteral("abcZ"));

        KisDocumentApplicationServices::InputMethodEvent finalPreedit;
        finalPreedit.preeditString = "pending";
        QVERIFY(cursor.inputMethodEvent(finalPreedit));
        QCOMPARE(toQString(shape.plainText()), QStringLiteral("abcZpending"));
    }

    QCOMPARE(services.commitCount, 1);
    QCOMPARE(toQString(shape.plainText()), QStringLiteral("abcZ"));
}

void SvgTextCursorTest::qtInputMethodAdapterMatchesQt515Payload()
{
    QTextCharFormat format;
    format.setFontUnderline(true);
    format.setFontOverline(true);
    format.setUnderlineStyle(QTextCharFormat::DashUnderline);
    format.setBackground(QBrush(Qt::red));
    const QList<QInputMethodEvent::Attribute> attributes = {
        {QInputMethodEvent::Selection, 2, 3, {}},
        {QInputMethodEvent::TextFormat, 0, 4, format},
        {QInputMethodEvent::Cursor, 1, 1, {}},
        {QInputMethodEvent::TextFormat, -1, 0, {}}
    };
    QInputMethodEvent event(QStringLiteral("preedit"), attributes);
    event.setCommitString(QStringLiteral("commit"), -2, 1);

    const auto native = svgTextNativeInputMethodEvent(event);
    QCOMPARE(toQString(native.commitString), QStringLiteral("commit"));
    QCOMPARE(toQString(native.preeditString), QStringLiteral("preedit"));
    QCOMPARE(native.replacementStart, -2);
    QCOMPARE(native.replacementLength, 1);
    QCOMPARE(native.attributes.size(), 3);
    QCOMPARE(native.attributes.at(0).type, KisDocumentApplicationServices::InputMethodAttributeType::Selection);
    QCOMPARE(native.attributes.at(0).start, 2);
    QCOMPARE(native.attributes.at(0).length, 3);
    QCOMPARE(native.attributes.at(1).type, KisDocumentApplicationServices::InputMethodAttributeType::TextFormat);
    QVERIFY(native.attributes.at(1).format.underline);
    QVERIFY(native.attributes.at(1).format.overline);
    QVERIFY(native.attributes.at(1).format.thick);
#ifdef Q_OS_LINUX
    QCOMPARE(native.attributes.at(1).format.style, KisDocumentApplicationServices::InputMethodLineStyle::Solid);
#else
    QCOMPARE(native.attributes.at(1).format.style, KisDocumentApplicationServices::InputMethodLineStyle::Dashed);
#endif
    QCOMPARE(native.attributes.at(2).type, KisDocumentApplicationServices::InputMethodAttributeType::Cursor);
}

void SvgTextCursorTest::nativeKeyDispatchMatchesQt515Adapter()
{
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">one two</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;
    SvgTextCursor cursor(&canvas);
    const int end = shape.posForIndex(shape.plainText().size());
    cursor.setShape(&shape);
    cursor.setPos(end, end);

    // 宿主面在 canvas 上（SvgTextTool 用的就是 dynamic_cast<... *>(canvas())），
    // 所以这里把 mock 宿主交给它——否则 bindingHost 恒 null、command 恒 None，
    // 下面那条 QCOMPARE 会因为「没有宿主可问」而红（R-69 交接的 latent）。
    // 刺激不能写死 chord：生产侧 `SvgTextQtPlatformHost::textCommand` 走
    // `QKeySequence::*` 标准键表，该表**按平台**取值 —— macOS / Qt 5.15.7 实测
    // `MoveToPreviousWord == Alt+Left`（Linux/Windows 才是 Ctrl+Left；
    // macOS 上 Ctrl+Left 不命中任何标准键 ⇒ command 恒 None）。R-69 写死 Ctrl+Left
    // 是 Linux/Windows 口径，在 macOS 上这条断言与 mock 无关地恒红。故按同一张标准
    // 键表取 chord：**断言不变**（command 仍是 MovePreviousWord、游标仍要落到词首），
    // 换的只是刺激 —— 即该平台真实的绑定。
    const QKeySequence wordLeftChord(QKeySequence::MoveToPreviousWord);
    QVERIFY(wordLeftChord.count() >= 1);
    const int wordLeftCode = wordLeftChord[0];
    PkToolKeyEvent qtWordLeft(static_cast<Pk::Key>(wordLeftCode & ~Qt::KeyboardModifierMask),
                              static_cast<Pk::KeyboardModifiers>(wordLeftCode & Qt::KeyboardModifierMask),
                              false);
    const SvgTextCursor::NativeKeyEvent nativeWordLeft =
        svgTextNativeKeyEvent(qtWordLeft, KoSvgText::HorizontalTB, KoSvgText::DirectionLeftToRight, &canvas);
    QCOMPARE(nativeWordLeft.command, SvgTextCursor::NativeKeyCommand::MovePreviousWord);
    QVERIFY(cursor.keyPressEvent(nativeWordLeft));
    QCOMPARE(cursor.getPos(), shape.wordStart(end));

    PkToolKeyEvent qtPrintable(static_cast<Pk::Key>(Qt::Key_Z), Pk::NoModifier, false, false, "Z");
    const SvgTextCursor::NativeKeyEvent nativePrintable =
        svgTextNativeKeyEvent(qtPrintable, KoSvgText::HorizontalTB, KoSvgText::DirectionLeftToRight, &canvas);
    QVERIFY(cursor.keyPressEvent(nativePrintable));
    QCOMPARE(toQString(shape.plainText()), QStringLiteral("one Ztwo"));
}

void SvgTextCursorTest::nativeAcceptedInputMatchesQt515UnicodeOracle()
{
    struct Case {
        const char *name;
        QString text;
        Qt::KeyboardModifiers modifiers;
        bool accepted;
    };
    const Case cases[] = {
        {"ascii digit", QStringLiteral("1"), Qt::NoModifier, true},
        {"space", QStringLiteral(" "), Qt::NoModifier, true},
        {"Cc control", QString(QChar(0x0001)), Qt::NoModifier, false},
        {"Cf format with Ctrl+Shift", QString(QChar(0x200c)),
         Qt::ControlModifier | Qt::ShiftModifier, true},
        {"private use", QString(QChar(0xe000)), Qt::NoModifier, true},
        {"unassigned", QString(QChar(0x0378)), Qt::NoModifier, false},
        {"AltGr printable", QString::fromUtf8("®"),
         Qt::ControlModifier | Qt::AltModifier, true},
    };

    const auto qt515AcceptableInput = [](const QKeyEvent &event) {
        if (event.text().isEmpty()) return false;
        const QChar c = event.text().at(0);
        if (c.category() == QChar::Other_Format) return true;
        if (event.modifiers() == Qt::ControlModifier
            || event.modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
            return false;
        }
        return c.isPrint() || c.category() == QChar::Other_PrivateUse
            || c == QLatin1Char('\t');
    };

    for (const Case &item : cases) {
        QKeyEvent qtEvent(QEvent::KeyPress, Qt::Key_unknown, item.modifiers, item.text);
        QCOMPARE(qt515AcceptableInput(qtEvent), item.accepted);

        KoSvgTextShape shape;
        KoSvgTextShapeMarkupConverter converter(&shape);
        QVERIFY2(converter.convertFromSvg("<text font-size=\"10\">x</text>", {},
                                          PkRectF(0, 0, 300, 300), 72.0),
                 item.name);
        ApplyingCanvas canvas;
        SvgTextCursor cursor(&canvas);
        cursor.setShape(&shape);
        const int end = shape.posForIndex(shape.plainText().size());
        cursor.setPos(end, end);

        PkToolKeyEvent pkEvent(static_cast<Pk::Key>(qtEvent.key()),
                               Pk::KeyboardModifiers(static_cast<int>(qtEvent.modifiers())),
                               qtEvent.isAccepted(), qtEvent.isAutoRepeat(), toPkString(qtEvent.text()));
        const SvgTextCursor::NativeKeyEvent native = svgTextNativeKeyEvent(
            pkEvent, KoSvgText::HorizontalTB, KoSvgText::DirectionLeftToRight);
        QCOMPARE(cursor.keyPressEvent(native), item.accepted);
        const QString expected = item.accepted ? QStringLiteral("x") + item.text
                                               : QStringLiteral("x");
        QCOMPARE(toQString(shape.plainText()), expected);
    }
}

void SvgTextCursorTest::nativeActionDispatchPreservesPropertySemantics()
{
    KoSvgTextProperties properties;
    properties.setProperty(KoSvgTextProperties::FontWeightId, PkVariant(400));
    const KoSvgTextProperties modified =
        SvgTextShortCuts::getModifiedProperties("svg_weight_bold", true, {properties});
    QCOMPARE(modified.property(KoSvgTextProperties::FontWeightId).toInt(), 700);
    QVERIFY(SvgTextShortCuts::actionEnabled("svg_weight_bold", {modified}));
    QVERIFY(!SvgTextShortCuts::isAction("not-an-svg-text-action"));

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\" font-weight=\"400\">abc</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(&shape);
    const int end = shape.posForIndex(shape.plainText().size());
    cursor.setPos(end, 0);
    QVERIFY(cursor.triggerAction("svg_weight_bold", true));
    QCOMPARE(shape.propertiesForPos(1, true).propertyOrDefault(KoSvgTextProperties::FontWeightId).toInt(), 700);
    QVERIFY(!cursor.triggerAction("not-an-svg-text-action", false));
}

void SvgTextCursorTest::hostActionDispatchKeepsPrintableAltGrInput()
{
    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});
    const auto originalAlignment = shape.textProperties().propertyOrDefault(
        KoSvgTextProperties::TextAlignAllId);

    // 输入端：宿主把「对齐右」绑在 Ctrl+Alt+R 上。AltGr 在真 Qt 里就报成 Ctrl+Alt，
    // 于是同一组合键既是这条动作的 chord、又携带可打印字符 —— 内核必须先把它当
    // 输入收下（acceptableInput 短路），不能派发给动作。
    canvas.actionShortcuts.insert(PkString("svg_align_right"),
                                  PkKeySequence{QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R)[0]});

    PkToolKeyEvent altGrEvent(Pk::Key_R,
                              Pk::ControlModifier | Pk::AltModifier,
                              false, false, toPkString(QString::fromUtf8("®")));
    tool.pkKeyPressEvent(&altGrEvent);

    QVERIFY(altGrEvent.isAccepted());
    QVERIFY(toQString(shape.plainText()).contains(QString::fromUtf8("®")));
    QCOMPARE(shape.textProperties().propertyOrDefault(KoSvgTextProperties::TextAlignAllId),
             originalAlignment);
}

void SvgTextCursorTest::hostTextTypeRetriggerKeepsCurrentActionChecked()
{
    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});
    const KoSvgTextShape::TextType originalTextType = shape.textType();

    PkString currentAction;
    switch (shape.textType()) {
    case KoSvgTextShape::PreformattedText:
        currentAction = PkString("text_type_preformatted");
        break;
    case KoSvgTextShape::PrePositionedText:
        currentAction = PkString("text_type_pre_positioned");
        break;
    case KoSvgTextShape::InlineWrap:
        currentAction = PkString("text_type_inline_wrap");
        break;
    case KoSvgTextShape::TextInShape:
        QFAIL("The host text-type actions do not represent TextInShape");
        break;
    }

    // 输入端：宿主把「当前已经是的那条类型动作」绑在 Ctrl+T 上，然后按下去。
    canvas.actionShortcuts.insert(currentAction,
                                  PkKeySequence{QKeySequence(Qt::CTRL | Qt::Key_T)[0]});

    PkToolKeyEvent event(Pk::Key_T, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&event);

    QVERIFY(event.isAccepted());
    // 重触发「当前类型」的动作是幂等的：slotConvertType 在
    // index == shape->textType() 时只走 slotTextTypeUpdated() 并把类型原样留在那里。
    QCOMPARE(shape.textType(), originalTextType);

    // 原用例还断言过「三个 text_type 动作互斥勾选」（按下后恰好一条 isChecked()）。
    // 那是**宿主态**：KisHostActionIdentity 没有承载 checked 的字段、KoCanvasActionHost
    // 也没有写通道 —— 按 impact-map §4 登记 1 移出保留范围，故此处不再断言。
    // 动作面本身仍被上面两条断言覆盖：键盘 chord → 内核处理器确实跑到。
}

void SvgTextCursorTest::hostMappedTextTypeShortcutDispatches()
{
    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});

    // 目标：当前类型之外的那一条类型动作（原用例同款选择规则）。
    const PkString target = shape.textType() == KoSvgTextShape::InlineWrap
        ? PkString("text_type_preformatted")
        : PkString("text_type_inline_wrap");
    const KoSvgTextShape::TextType expectedType = target == PkString("text_type_inline_wrap")
        ? KoSvgTextShape::InlineWrap : KoSvgTextShape::PreformattedText;

    // 输入端：宿主把目标动作绑在 Ctrl+T 上。
    canvas.actionShortcuts.insert(target, PkKeySequence{QKeySequence(Qt::CTRL | Qt::Key_T)[0]});

    PkToolKeyEvent event(Pk::Key_T, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&event);

    QVERIFY(event.isAccepted());
    QCOMPARE(shape.textType(), expectedType);
}

void SvgTextCursorTest::hostMappedMovementShortcutDispatchesWhenEnabled()
{
    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});

    // 输入端：宿主把「排版模式左移 1px」绑在 Ctrl+M 上。
    canvas.actionShortcuts.insert(PkString("svg_type_setting_move_selection_start_left_1_px"),
                                  PkKeySequence{QKeySequence(Qt::CTRL | Qt::Key_M)[0]});

    const int start = shape.posForIndex(0);
    const int end = shape.posForIndex(shape.plainText().size());
    const PkList<KoSvgTextCharacterInfo> before =
        shape.getPositionsAndRotationsForRange(start, end);
    QVERIFY(!before.isEmpty());

    PkToolKeyEvent event(Pk::Key_M, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&event);

    QVERIFY(event.isAccepted());
    const PkList<KoSvgTextCharacterInfo> after =
        shape.getPositionsAndRotationsForRange(start, end);
    QCOMPARE(after.size(), before.size());
    QVERIFY(after.first().finalPos != before.first().finalPos);

    // 原用例在此前还有一句 `movement->setEnabled(true)` —— 那是给 QAction 的
    // 启用态解闸：旧路径的兜底派发调 `hostAction->trigger()`，而 disabled 的
    // QAction 触发是 no-op（slotTextTypeUpdated() 此前刚把四个移动动作 setEnabled(false)）。
    // 改挂内核物化边界后，启用态由 slotTextTypeUpdated() 经
    // KoCanvasActionHost::setHostActionEnabled 写回**宿主**，内核这条兜底派发不再读它
    // （KisHostActionIdentity 只承载 alwaysEnabled，没有「当前是否 enabled」的回读通道）
    // ⇒ 这一句既无对象也不再影响本用例。**该语义变化是本轮新识别的**，已登记在报告
    // 的「移植丢失」一节，不在 impact-map §4 的既有登记里。
}

void SvgTextCursorTest::hostActionBoundaryReadsIdentitiesAndWritesEnabledState()
{
    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    // ---- 读路：宿主动作快捷键不再从真 Qt QAction 读（K-4 消费方 2）------------
    // 改挂内核物化边界后，唯一的来源是桶无关的 KoCanvasActionHost::hostActions()。
    // 这些 identity 是 native 侧解码好的（丢空 chord、逐和弦取 int），此处逐条验
    // 解码结果与旧路径 `action->shortcut()` 的语义一致。
    const int ctrlT = QKeySequence(Qt::CTRL | Qt::Key_T)[0];

    KisHostActionIdentity single;
    single.objectName = PkString("text_type_inline_wrap");
    single.shortcutChords.append(std::vector<int>{ctrlT});

    KisHostActionIdentity triple;
    triple.objectName = PkString("svg_align_right");
    triple.shortcutChords.append(std::vector<int>{1, 2, 3});

    KisHostActionIdentity unbound;
    unbound.objectName = PkString("svg_clear_formatting");

    controller.hostActionIdentities.append(single);
    controller.hostActionIdentities.append(triple);
    controller.hostActionIdentities.append(unbound);

    SvgTextQtPlatformHost platformHost(&canvas);

    const PkKeySequence singleChord = platformHost.actionShortcut(PkString("text_type_inline_wrap"));
    QCOMPARE(singleChord.size(), 1);
    QCOMPARE(singleChord[0], ctrlT);

    const PkKeySequence tripleChords = platformHost.actionShortcut(PkString("svg_align_right"));
    QCOMPARE(tripleChords.size(), 3);
    QCOMPARE(tripleChords[0], 1);
    QCOMPARE(tripleChords[1], 2);
    QCOMPARE(tripleChords[2], 3);

    // 该动作没有 shortcutChords（今天没绑）= 空序列，与旧路径「QAction 的 shortcut 为空」同义。
    QVERIFY(platformHost.actionShortcut(PkString("svg_clear_formatting")).isEmpty());
    // 宿主根本不报这个名字 = 也返回空序列。
    QVERIFY(platformHost.actionShortcut(PkString("not-a-host-action")).isEmpty());
    // 没有 canvasController 时同样是空序列（不崩）。
    SvgTextQtPlatformHost detachedHost(nullptr);
    QVERIFY(detachedHost.actionShortcut(PkString("text_type_inline_wrap")).isEmpty());

    // ---- 写路：slotTextTypeUpdated() 的启用态写回（取代 setEnabled）-----------
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">abc</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});

    // 三个 text_type 动作：shape != nullptr ⇒ 写 enabled=true。
    const PkStringList enabledOnSelection = {
        "text_type_preformatted", "text_type_pre_positioned", "text_type_inline_wrap"
    };
    for (const PkString &name : enabledOnSelection) {
        QVERIFY(controller.hostActionEnabledWrites.contains(name));
        QVERIFY(controller.hostActionEnabledWrites.value(name));
    }
    // 四个排版模式移动动作：无 UI 可激活 ⇒ 恒写 enabled=false。
    const PkStringList alwaysDisabled = {
        "svg_type_setting_move_selection_start_down_1_px",
        "svg_type_setting_move_selection_start_up_1_px",
        "svg_type_setting_move_selection_start_left_1_px",
        "svg_type_setting_move_selection_start_right_1_px"
    };
    for (const PkString &name : alwaysDisabled) {
        QVERIFY(controller.hostActionEnabledWrites.contains(name));
        QVERIFY(!controller.hostActionEnabledWrites.value(name));
    }
}

// 兜底派发把 **Toggle** 类动作送进内核处理器时，属性必须**真的翻转**。
//
// 这条用例是 B1 的回归网：本文件原有 4 个 hostActionDispatch* 用例只覆盖
// （a）Set 类动作（`svg_align_right`，且断言的是「**没有**被触发」）与
// （b）`addMappedAction` 那一族（`text_type_*` / `svg_type_setting_move_*`，
//     它们的 binding 丢弃实参），**没有任何一条把 Toggle 送进这条派发**
// —— B1 正是从这个缺口溜过去的。
//
// B1 的症状是「属性被设成它**现在**的值、不翻转」；本用例的断言刻意只认
// 「翻转后的值」，所以能区分「翻转」与「设成原值」。
void SvgTextCursorTest::hostToggleActionShortcutFlipsProperty()
{
    using namespace std::chrono_literals;
    PkThreadCallQueue::warmUpCurrentThread();

    MockShapeController shapeController;
    HostToolCanvas canvas(&shapeController);
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\" font-weight=\"400\">abcd</text>", {},
                                     PkRectF(0, 0, 300, 300), 72.0));
    canvas.shapeManager()->selection()->select(&shape);

    SvgTextTool tool(&canvas);
    tool.activate({&shape});

    // 观测值取字内一格（`posForIndex(1)` 落在 'b' 上），避开末端插入点那类边界。
    const auto boldAtChar = [&shape]() {
        return shape.propertiesForPos(shape.posForIndex(1), true)
                    .propertyOrDefault(KoSvgTextProperties::FontWeightId).toInt() >= 500;
    };
    QVERIFY(!boldAtChar());

    // 输入端：宿主把「加粗」这条 Toggle 动作绑在 Ctrl+B 上。
    canvas.actionShortcuts.insert(PkString("svg_weight_bold"),
                                  PkKeySequence{QKeySequence(Qt::CTRL | Qt::Key_B)[0]});

    // 回程（旧路径里由宿主动作自己的 isChecked 承担的那份「当前态」）：内核侧的
    // `binding.active` 只在**游标无选区**时回写 —— `updateCanvasResources()` 以
    // `pos == anchor` 为闸门，经 10ms 的字符选区压缩器送达。这里照
    // nativeTimerRequiresExplicitPumpAndCancelsQueuedDelivery 的泵法把它送达，
    // 让下一次派发拿到的是**当前**态。
    const auto syncCurrentState = [&tool]() {
        tool.deselect();
        std::this_thread::sleep_for(25ms);
        PkThreadCallQueue::processPendingCalls();
    };

    syncCurrentState();
    tool.selectAll();

    PkToolKeyEvent boldOnEvent(Pk::Key_B, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&boldOnEvent);
    QVERIFY(boldOnEvent.isAccepted());
    QVERIFY(boldAtChar());          // 400 → 700：真的翻转了

    syncCurrentState();
    tool.selectAll();

    PkToolKeyEvent boldOffEvent(Pk::Key_B, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&boldOffEvent);
    QVERIFY(boldOffEvent.isAccepted());
    QVERIFY(!boldAtChar());         // 700 → 400：翻回去
}

void SvgTextCursorTest::nativeTimerRequiresExplicitPumpAndCancelsQueuedDelivery()
{
    using namespace std::chrono_literals;
    PkThreadCallQueue::warmUpCurrentThread();

    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">blink</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    ApplyingCanvas canvas;
    int updates = 0;
    {
        SvgTextCursor cursor(&canvas);
        cursor.setDecorationUpdateCallback([&](const PkRectF &) { ++updates; });
        cursor.setShape(&shape);
        cursor.setCaretSetting(1, 10, 100, false);
        cursor.focusIn();
        updates = 0;

        std::this_thread::sleep_for(25ms);
        QCOMPARE(updates, 0);
        QVERIFY(PkThreadCallQueue::pendingCount() >= std::size_t(1));
        QVERIFY(PkThreadCallQueue::processPendingCalls() >= 1);
        QVERIFY(updates > 0);

        updates = 0;
        std::this_thread::sleep_for(25ms);
    }

    PkThreadCallQueue::processPendingCalls();
    QCOMPARE(updates, 0);
}

void SvgTextCursorTest::nativeRegistrationPreservesFactoryAndResources()
{
    registerSvgTextTool();
    KoToolFactoryBase *const firstFactory = KoToolRegistry::instance()->value("SvgTextTool");
    QVERIFY(firstFactory);
    registerSvgTextTool();
    QCOMPARE(KoToolRegistry::instance()->value("SvgTextTool"), firstFactory);

    struct Case {
        SvgTextToolPixmap pixmap;
        int width;
        int height;
        const char *pixelSha256;
    };
    const Case cases[] = {
        {SvgTextToolPixmap::Basic, 32, 32, "9082c8f93543e562107fc244ea27ae858e66c669c371592db1c34a13af771e47"},
        {SvgTextToolPixmap::InlineHorizontal, 32, 32, "dddabca0ae3337a44daa96dfd90a0becc9dbe8a3995f968e14296d39732ff0aa"},
        {SvgTextToolPixmap::InlineVertical, 32, 32, "e2f3c439a8ea087d83a6704fceccdec02d221ab548deba21d635c0b6fa786cb0"},
        {SvgTextToolPixmap::OnPath, 32, 32, "b2426a3c697451b8164f92e63aaa0b24341e808e4cc84eaccb97dbd6e9858df4"},
        {SvgTextToolPixmap::InShape, 32, 32, "a8282cbea6fc6c0e5869c2b703de9bf8b8e4400e74dcf754785caa27421ed1c5"},
        {SvgTextToolPixmap::IBeamHorizontal, 22, 22, "382294bd454bc6d5ad7f17817b37a286e493ba1cfca751dfdfcd01a7e97788ff"},
        {SvgTextToolPixmap::IBeamVertical, 22, 22, "22aca04897604f248419532a4ca2751e53d7847733ebb49024f8cefb9c877120"},
        {SvgTextToolPixmap::IBeamHorizontalDone, 22, 22, "ed0643d3ef3159f8febd667fdc14979fd937db9ace262fe7c7d72c35e2465888"}
    };
    for (const Case &item : cases) {
        QImage image(svgTextToolCursorPixmap(item.pixmap));
        QVERIFY(!image.isNull());
        QCOMPARE(image.width(), item.width);
        QCOMPARE(image.height(), item.height);
        image = image.convertToFormat(QImage::Format_ARGB32);
        const QByteArray pixels(reinterpret_cast<const char *>(image.constBits()), int(image.sizeInBytes()));
        QCOMPARE(QCryptographicHash::hash(pixels, QCryptographicHash::Sha256).toHex(), QByteArray(item.pixelSha256));
    }

    const QByteArray xml(svgTextToolXmlGui());
    QVERIFY(xml.startsWith("<?xml version=\"1.0\"?>"));
    QVERIFY(xml.contains("name=\"svg_SvgTextTool\""));
    QVERIFY(xml.contains("<Action name=\"svg_settings\"/>"));
}

void SvgTextCursorTest::shortcutValuesMatchQt515Oracle()
{
    // Qt QVariant is the independent numeric carrier used by the original
    // shortcut metadata. Literal cases cover toggle, set and both adjustments.
    struct Case { const char *name; bool checked; int property; QVariant before; QVariant after; };
    const Case cases[] = {
        {"svg_weight_bold", true, KoSvgTextProperties::FontWeightId, QVariant(400), QVariant(700)},
        {"svg_weight_bold", false, KoSvgTextProperties::FontWeightId, QVariant(700), QVariant(400)},
        {"svg_weight_normal", false, KoSvgTextProperties::FontWeightId, QVariant(900), QVariant(400)},
        {"svg_increase_font_size", false, KoSvgTextProperties::FontSizeId, QVariant(12.5), QVariant(13.5)},
        {"svg_decrease_font_size", false, KoSvgTextProperties::FontSizeId, QVariant(12.5), QVariant(11.5)}
    };
    for (const Case &item : cases) {
        QVERIFY(SvgTextShortCuts::isAction(item.name));
        KoSvgTextProperties properties;
        properties.setProperty(KoSvgTextProperties::PropertyId(item.property), PkVariant(item.before.toDouble()));
        const auto result = SvgTextShortCuts::getModifiedProperties(item.name, item.checked, {properties});
        QCOMPARE(result.property(KoSvgTextProperties::PropertyId(item.property)).toDouble(), item.after.toDouble());
    }
    QVERIFY(!SvgTextShortCuts::actionEnabled("unknown", {}));
    QVERIFY(!SvgTextShortCuts::isAction("unknown"));
}

void SvgTextCursorTest::configHandlesMatchKConfigOracle()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("tools.ini");
    const QString preset = QString::fromUtf8("预设 café العربية");
    const QString boundaryXml = QString::fromUtf8("<color channeldepth=\"U8\"><sRGB r=\"0.25\" g=\"0.5\" b=\"1\"/></color>");
    KConfig reference(fileName, KConfig::SimpleConfig);
    KConfigGroup expected(&reference, "SvgTextToolOracle");
    expected.writeEntry("useCurrentTextProperties", false);
    expected.writeEntry("cssStylePresetName", preset);
    expected.writeEntry("useVisualBidiCursor", true);
    expected.writeEntry("pasteRichtTextByDefault", true);
    expected.writeEntry("fuzziness", 37);
    expected.writeEntry("boundaryColor", boundaryXml);
    QVERIFY(expected.sync());

    const PkString groupName("SvgTextToolOracle");
    PkConfigGroup group(groupName);
    group.deleteGroup();
    SvgTextToolOptionsData written;
    written.useCurrentTextProperties = false;
    written.cssStylePresetName = toPkString(preset);
    written.useVisualBidiCursor = true;
    written.pasteRichtTextByDefault = true;
    written.writeConfig(groupName);
    group.writeEntry("fuzziness", 37);
    group.writeEntry("boundaryColor", toPkString(boundaryXml));
    group.sync();

    // This intentionally verifies fresh handles in one process. A disk reload
    // is a separate requirement; PkConfigStore currently has no disk backend.
    KConfig reopened(fileName, KConfig::SimpleConfig);
    KConfigGroup oracle(&reopened, "SvgTextToolOracle");
    SvgTextToolOptionsData loaded;
    loaded.loadConfig(groupName);
    QCOMPARE(loaded.useCurrentTextProperties, oracle.readEntry("useCurrentTextProperties", true));
    QCOMPARE(toQString(loaded.cssStylePresetName), oracle.readEntry("cssStylePresetName", QString()));
    QCOMPARE(loaded.useVisualBidiCursor, oracle.readEntry("useVisualBidiCursor", false));
    QCOMPARE(loaded.pasteRichtTextByDefault, oracle.readEntry("pasteRichtTextByDefault", false));
    PkConfigGroup fresh(groupName);
    QCOMPARE(fresh.hasKey("threshold"), oracle.hasKey("threshold"));
    QCOMPARE(fresh.readEntry("fuzziness", 8), oracle.readEntry("fuzziness", 8));
    QCOMPARE(toQString(fresh.readEntry("boundaryColor", PkString())), oracle.readEntry("boundaryColor", QString()));
    group.deleteGroup();
}

void SvgTextCursorTest::controllerChangesUpdateImeTransform()
{
    ClipboardApplicationServices services;
    ApplicationServicesGuard guard(&services);
    CursorCanvas canvas;
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">Hello</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    NativeCursorHost host;
    SvgTextCursor cursor(&canvas, &host);
    cursor.setShape(&shape);
    cursor.focusIn();
    controller.proxyObject->emitSizeChanged(PkSize(640, 480));
    QCOMPARE(services.inputItemTransform.map(PkPointF()), PkPointF(17, 23));
    QVERIFY(services.inputMethodVisible);
    QVERIFY(services.updateCount > 0);
    host.offset = PkPoint(31, 47);
    controller.proxyObject->emitMoveDocumentOffset(PkPointF(), PkPointF(14, 24));
    QCOMPARE(services.inputItemTransform.map(PkPointF()), PkPointF(31, 47));
    cursor.setShape(nullptr);
    canvas.setCanvasController(nullptr);
}

void SvgTextCursorTest::initTestCase()
{
    QString fileName = QString(FILES_DATA_DIR) + '/' + "DejaVuSans.ttf";
    bool res = KoFontRegistry::instance()->addFontFilePathToRegistry(toPkString(fileName));

    QVERIFY2(res, QString("KoFontRegistry could not add the test font %1").arg(fileName).toLatin1());
}

void SvgTextCursorTest::test_ltr_data()
{
    QTest::addColumn<SvgTextCursor::MoveMode>("mode");
    QTest::addColumn<bool>("visual");
    QTest::addColumn<int>("result");

    QTest::addRow("down")  << SvgTextCursor::MoveDown << false << 16;
    QTest::addRow("up  ")  << SvgTextCursor::MoveUp << false  << 0;
    QTest::addRow("left")  << SvgTextCursor::MoveLeft << false  << 4;
    QTest::addRow("right") << SvgTextCursor::MoveRight << false  << 6;
    QTest::addRow("word left")  << SvgTextCursor::MoveWordLeft << false  << 4;
    QTest::addRow("word right") << SvgTextCursor::MoveWordRight << false  << 9;
    QTest::addRow("line end")   << SvgTextCursor::MoveLineEnd << false  << 10;
    QTest::addRow("line start") << SvgTextCursor::MoveLineStart << false  << 0;
    QTest::addRow("paragraph end")   << SvgTextCursor::ParagraphEnd << false  << 48;
    QTest::addRow("paragraph start") << SvgTextCursor::ParagraphStart << false  << 0;
    QTest::addRow("visual - down")  << SvgTextCursor::MoveDown << true << 16;
    QTest::addRow("visual - up  ")  << SvgTextCursor::MoveUp << true  << 0;
    QTest::addRow("visual - left")  << SvgTextCursor::MoveLeft << true  << 4;
    QTest::addRow("visual - right") << SvgTextCursor::MoveRight << true  << 6;
    QTest::addRow("visual - word left")  << SvgTextCursor::MoveWordLeft << true  << 4;
    QTest::addRow("visual - word right") << SvgTextCursor::MoveWordRight << true  << 9;
    QTest::addRow("visual - line end")   << SvgTextCursor::MoveLineEnd << true  << 10;
    QTest::addRow("visual - line start") << SvgTextCursor::MoveLineStart << true  << 0;
    QTest::addRow("visual - paragraph end")   << SvgTextCursor::ParagraphEnd << true  << 48;
    QTest::addRow("visual - paragraph start") << SvgTextCursor::ParagraphStart << true  << 0;
}

void SvgTextCursorTest::test_ltr()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0;font-family:Deja Vu Sans\">The quick brown fox jumps over the lazy dog.</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    MockCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(textShape);

    QFETCH(SvgTextCursor::MoveMode, mode);
    QFETCH(bool, visual);
    QFETCH(int, result);
    cursor.setVisualMode(visual);

    //The current textcursor sets the pos to the text end...
    //QCOMPARE(cursor.getPos(), 0);
    cursor.setPos(5, 5);

    cursor.moveCursor(mode);
    QCOMPARE(cursor.getPos(), result);
}

void SvgTextCursorTest::test_rtl_data()
{
    QTest::addColumn<SvgTextCursor::MoveMode>("mode");
    QTest::addColumn<bool>("visual");
    QTest::addColumn<int>("result");

    QTest::addRow("down")  << SvgTextCursor::MoveDown << false << 22;
    QTest::addRow("up  ")  << SvgTextCursor::MoveUp << false  << 6;
    QTest::addRow("left")  << SvgTextCursor::MoveLeft << false  << 11;
    QTest::addRow("right") << SvgTextCursor::MoveRight << false  << 9;
    QTest::addRow("word left")  << SvgTextCursor::MoveWordLeft << false  << 11;
    QTest::addRow("word right") << SvgTextCursor::MoveWordRight << false  << 8;
    QTest::addRow("line end")   << SvgTextCursor::MoveLineEnd << false  << 16;
    QTest::addRow("line start") << SvgTextCursor::MoveLineStart << false  << 8;
    QTest::addRow("paragraph end")   << SvgTextCursor::ParagraphEnd << false  << 33;
    QTest::addRow("paragraph start") << SvgTextCursor::ParagraphStart << false  << 0;
    QTest::addRow("visual -down")  << SvgTextCursor::MoveDown << true << 22;
    QTest::addRow("visual - up  ")  << SvgTextCursor::MoveUp << true  << 6;
    QTest::addRow("visual - left")  << SvgTextCursor::MoveLeft << true  << 9;
    QTest::addRow("visual - right") << SvgTextCursor::MoveRight << true  << 11;
    QTest::addRow("visual - word left")  << SvgTextCursor::MoveWordLeft << true  << 11;
    QTest::addRow("visual - word right") << SvgTextCursor::MoveWordRight << true  << 8;
    QTest::addRow("visual - line end")   << SvgTextCursor::MoveLineEnd << true  << 16;
    QTest::addRow("visual - line start") << SvgTextCursor::MoveLineStart << true  << 8;
    QTest::addRow("visual - paragraph end")   << SvgTextCursor::ParagraphEnd << true  << 33;
    QTest::addRow("visual - paragraph start") << SvgTextCursor::ParagraphStart << true  << 0;
}

void SvgTextCursorTest::test_rtl()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0; direction:rtl; font-family:Deja Vu Sans\">داستان SVG 1.1 SE طولا ني است.</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    MockCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(textShape);

    QFETCH(SvgTextCursor::MoveMode, mode);
    QFETCH(bool, visual);
    QFETCH(int, result);
    cursor.setVisualMode(visual);

    //The current textcursor sets the pos to the text end...
    //QCOMPARE(cursor.getPos(), 0);
    cursor.setPos(10, 10);

    cursor.moveCursor(mode);
    QCOMPARE(cursor.getPos(), result);
}
void SvgTextCursorTest::test_ttb_rl_data()
{
    QTest::addColumn<SvgTextCursor::MoveMode>("mode");
    QTest::addColumn<bool>("visual");
    QTest::addColumn<int>("result");

    QTest::addRow("down")  << SvgTextCursor::MoveDown << false << 6;
    QTest::addRow("up  ")  << SvgTextCursor::MoveUp << false  << 4;
    QTest::addRow("left")  << SvgTextCursor::MoveLeft << false  << 12;
    QTest::addRow("right") << SvgTextCursor::MoveRight << false  << 0;
    QTest::addRow("word left")  << SvgTextCursor::MoveWordLeft << false  << 12;
    QTest::addRow("word right") << SvgTextCursor::MoveWordRight << false  << 0;
    QTest::addRow("line end")   << SvgTextCursor::MoveLineEnd << false  << 6;
    QTest::addRow("line start") << SvgTextCursor::MoveLineStart << false  << 0;
    QTest::addRow("paragraph end")   << SvgTextCursor::ParagraphEnd << false  << 36;
    QTest::addRow("paragraph start") << SvgTextCursor::ParagraphStart << false  << 0;
    QTest::addRow("visual - down")  << SvgTextCursor::MoveDown << true << 6;
    QTest::addRow("visual - up  ")  << SvgTextCursor::MoveUp << true  << 4;
    QTest::addRow("visual - left")  << SvgTextCursor::MoveLeft << true  << 12;
    QTest::addRow("visual - right") << SvgTextCursor::MoveRight << true  << 0;
    QTest::addRow("visual - word left")  << SvgTextCursor::MoveWordLeft << true  << 12;
    QTest::addRow("visual - word right") << SvgTextCursor::MoveWordRight << true  << 0;
    QTest::addRow("visual - line end")   << SvgTextCursor::MoveLineEnd << true  << 6;
    QTest::addRow("visual - line start") << SvgTextCursor::MoveLineStart << true  << 0;
    QTest::addRow("visual - paragraph end")   << SvgTextCursor::ParagraphEnd << true  << 36;
    QTest::addRow("visual - paragraph start") << SvgTextCursor::ParagraphStart << true  << 0;
}

void SvgTextCursorTest::test_ttb_rl()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0; writing-mode:vertical-rl; font-family:Deja Vu Sans\">A B C D E F G H I J K L M N O P</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    MockCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(textShape);

    QFETCH(SvgTextCursor::MoveMode, mode);
    QFETCH(bool, visual);
    QFETCH(int, result);
    cursor.setVisualMode(visual);

    //The current textcursor sets the pos to the text end...
    //QCOMPARE(cursor.getPos(), 0);
    cursor.setPos(5, 5);

    cursor.moveCursor(mode);
    QCOMPARE(cursor.getPos(), result);
}
void SvgTextCursorTest::test_ttb_lr_data()
{
    QTest::addColumn<SvgTextCursor::MoveMode>("mode");
    QTest::addColumn<bool>("visual");
    QTest::addColumn<int>("result");

    QTest::addRow("down")  << SvgTextCursor::MoveDown << false << 6;
    QTest::addRow("up  ")  << SvgTextCursor::MoveUp << false  << 4;
    QTest::addRow("left")  << SvgTextCursor::MoveLeft << false  << 0;
    QTest::addRow("right") << SvgTextCursor::MoveRight << false  << 12;
    QTest::addRow("word left")  << SvgTextCursor::MoveWordLeft << false  << 0;
    QTest::addRow("word right") << SvgTextCursor::MoveWordRight << false  << 12;
    QTest::addRow("line end")   << SvgTextCursor::MoveLineEnd << false  << 6;
    QTest::addRow("line start") << SvgTextCursor::MoveLineStart << false  << 0;
    QTest::addRow("paragraph end")   << SvgTextCursor::ParagraphEnd << false  << 36;
    QTest::addRow("paragraph start") << SvgTextCursor::ParagraphStart << false  << 0;
    QTest::addRow("visual - down")  << SvgTextCursor::MoveDown << true << 6;
    QTest::addRow("visual - up  ")  << SvgTextCursor::MoveUp << true  << 4;
    QTest::addRow("visual - left")  << SvgTextCursor::MoveLeft << true  << 0;
    QTest::addRow("visual - right") << SvgTextCursor::MoveRight << true  << 12;
    QTest::addRow("visual - word left")  << SvgTextCursor::MoveWordLeft << true  << 0;
    QTest::addRow("visual - word right") << SvgTextCursor::MoveWordRight << true  << 12;
    QTest::addRow("visual - line end")   << SvgTextCursor::MoveLineEnd << true  << 6;
    QTest::addRow("visual - line start") << SvgTextCursor::MoveLineStart << true  << 0;
    QTest::addRow("visual - paragraph end")   << SvgTextCursor::ParagraphEnd << true  << 36;
    QTest::addRow("visual - paragraph start") << SvgTextCursor::ParagraphStart << true  << 0;
}

void SvgTextCursorTest::test_ttb_lr()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0; writing-mode:vertical-lr; font-family:Deja Vu Sans\">A B C D E F G H I J K L M N O P</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    MockCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(textShape);

    QFETCH(SvgTextCursor::MoveMode, mode);
    QFETCH(bool, visual);
    QFETCH(int, result);
    cursor.setVisualMode(visual);

    //The current textcursor sets the pos to the text end...
    //QCOMPARE(cursor.getPos(), 0);
    cursor.setPos(5, 5);

    cursor.moveCursor(mode);
    QCOMPARE(cursor.getPos(), result);
}

void SvgTextCursorTest::test_filter_control_chars_in_command_data()
{
    QTest::addColumn<QString>("srcText");
    QTest::addColumn<QString>("expectedFilteredText");

    QTest::addRow("cr") << "test\rtext" << "test\ntext";
    QTest::addRow("crlf") << "test\r\ntext" << "test\ntext";
    QTest::addRow("Unicode Paragraph Separator (U+2029)") << QString::fromUtf16(u"test\x2029text") << "test\ntext";
    QTest::addRow("Unicode Line Separator (U+2028)") << QString::fromUtf16(u"test\x2028text") << "test\ntext";
    QTest::addRow("Unicode Vertical Tab (U+000B)") << QString::fromUtf16(u"test\x000Btext") << "test\ntext";
    QTest::addRow("Whitespace is preserved") << "test text" << "test text";
    QTest::addRow("Multiwhitespace is preserved") << "test  text" << "test  text";
}

void SvgTextCursorTest::test_filter_control_chars_in_command()
{
    QFETCH(QString, srcText);
    QFETCH(QString, expectedFilteredText);

    const QString result = toQString(SvgTextInsertCommand::filterInputUnicodeString(toPkString(srcText)));
    QCOMPARE(result, expectedFilteredText);
}

// Test basic text insertion in a horizontal ltr wrapped text;
void SvgTextCursorTest::test_text_insert_command()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0;font-family:Deja Vu Sans\">The quick brown fox jumps over the lazy dog.</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    int pos = textShape->posForIndex(25, false, true);
    SvgTextInsertCommand *cmd = new SvgTextInsertCommand(textShape, pos, pos, " badly");
    QString test = "The quick brown fox jumps over the lazy dog.";
    QString test2 = test;
    test.insert(25, " badly");
    cmd->redo();
    QCOMPARE(test, toQString(textShape->plainText()));

    cmd->undo();
    QCOMPARE(test2, toQString(textShape->plainText()));
}

// Test basic text removal in a horizontal ltr wrapped text;
void SvgTextCursorTest::test_text_remove_command()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0;font-family:Deja Vu Sans\">The quick brown fox jumps over the lazy dog.</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);
    QString test = "The quick brown fox jumps over the lazy dog.";
    QString test2 = test;

    SvgTextRemoveCommand *cmd = new SvgTextRemoveCommand(textShape, 15, 10, 10, 5);
    test.remove(10, 5);

    cmd->redo();
    QCOMPARE(test, toQString(textShape->plainText()));

    cmd->undo();
    QCOMPARE(test2, toQString(textShape->plainText()));
}

void SvgTextCursorTest::test_text_remove_dedicated_data()
{
    QTest::addColumn<int>("pos");
    QTest::addColumn<SvgTextCursor::MoveMode>("mode1");
    QTest::addColumn<SvgTextCursor::MoveMode>("mode2");
    QTest::addColumn<int>("length");

    QTest::addRow("backspace") << 11 << SvgTextCursor::MovePreviousChar << SvgTextCursor::MoveNone << 1;
    QTest::addRow("delete") << 10 << SvgTextCursor::MoveNone << SvgTextCursor::MoveNextChar << 1;
    QTest::addRow("start-of-word") << 5 << SvgTextCursor::MoveWordStart << SvgTextCursor::MoveNone << 1;
    QTest::addRow("end-of-word") << 5 << SvgTextCursor::MoveNone << SvgTextCursor::MoveWordEnd << 4;
    QTest::addRow("end-of-line") << 5 << SvgTextCursor::MoveNone << SvgTextCursor::MoveLineEnd << 5;
    QTest::addRow("delete-line") << 5 << SvgTextCursor::MoveLineStart << SvgTextCursor::MoveLineEnd << 10;
}

void SvgTextCursorTest::test_text_remove_dedicated()
{
    KoSvgTextShape *textShape = new KoSvgTextShape();
    QString ref ("<text style=\"inline-size:50.0; font-size:10.0;font-family:Deja Vu Sans\">The quick brown fox jumps over the lazy dog.</text>");
    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(ref), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    QFETCH(int, pos);
    QFETCH(SvgTextCursor::MoveMode, mode1);
    QFETCH(SvgTextCursor::MoveMode, mode2);
    QFETCH(int, length);

    MockCanvas canvas;
    SvgTextCursor cursor(&canvas);
    cursor.setShape(textShape);
    cursor.setPos(pos, pos);
    cursor.moveCursor(mode1);
    int posA = textShape->indexForPos(cursor.getPos());
    cursor.setPos(pos, pos);
    cursor.moveCursor(mode2);
    int posB = textShape->indexForPos(cursor.getPos());
    int posStart = pkMin(posA, posB);
    int posEnd = pkMax(posA, posB);

    QCOMPARE(posEnd - posStart, length);

}

void SvgTextCursorTest::test_set_transforms_on_text_command_data()
{
    QTest::addColumn<QString>("svg");
    QTest::addColumn<int>("pos");
    QTest::addColumn<int>("length");
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("delta");

    QMap<int, QString> mapType = {
        {int(SvgTextChangeTransformsOnRange::OffsetAll), "-offset-all"},
        {int(SvgTextChangeTransformsOnRange::ScaleAndRotate), "-scale-and-rotate"},
        {int(SvgTextChangeTransformsOnRange::ScaleOnly), "-scale-only"},
        {int(SvgTextChangeTransformsOnRange::RotateOnly), "-rotate-only"}
    };
    QMap<int, QString> mapDelta = {
        {true, "-delta-pos"},
        {false, "-absolute-pos"},
    };

    Q_FOREACH(int type, mapType.keys()) {
        Q_FOREACH(int delta, mapDelta.keys()) {
            QTest::addRow("ltr%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data()) << "<text style=\"font-size:10.0;font-family:Deja Vu Sans\">Digital Painting, Creative Freedom</text>" << 3 << 3 << type << bool(delta);
            QTest::addRow("ttb%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data())  << "<text style=\"font-size:10.0;writing-mode: vertical-rl;font-family:Deja Vu Sans\">KRITA</text>" << 1 << 3 << type << bool(delta);
            QTest::addRow("ltr-absolute-pos%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data()) << "<text style=\"font-size:10.0;font-family:Deja Vu Sans\">Digital Painting, <tspan x=\"0\" y=\"0\">Creative</tspan> Freedom</text>" << 15 << 4 << type << bool(delta);
            QTest::addRow("ltr-mixed-pos%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data()) << "<text style=\"font-size:10.0;font-family:Deja Vu Sans\">Digital Painting, <tspan x=\"0\" dy=\"10\">Creative</tspan> Freedom</text>" << 15 << 4 << type << bool(delta);

            // These tests are very sensitive to the location of the anchor, so the following tests have been massaged a little so the text isn't moved in such a manner that the anchor location changes.

            QTest::addRow("ltr-relative-pos%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data()) << "<text style=\"font-size:10.0;text-anchor:start;font-family:Deja Vu Sans\">Digital Painting, <tspan dx=\"-50\" dy=\"10\">Creative</tspan> Freedom</text>" << 15 << 4 << type << bool(delta);
            QTest::addRow("rtl%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data())  << "<text style=\"font-size:10.0;direction: rtl;text-anchor:end;font-family:Deja Vu Sans\">رسم رقميّ، حريّة إبداعيّة</text>" << 2 << 5 << type << bool(delta);
            QTest::addRow("rtl-unicode-bidi%s%s",
                          mapType.value(type).toLatin1().data(),
                          mapDelta.value(delta).toLatin1().data())  << "<text style=\"font-size:10.0;direction: rtl;text-anchor:end;font-family:Deja Vu Sans\"><tspan>رسم رقميّ،</tspan><tspan direction=\"ltr\" unicode-bidi=\"isolate\">- Krita - </tspan><tspan> حريّة إبداعيّة</tspan></text></text>" << 14 << 7 << type << bool(delta);
        }
    }
}

void SvgTextCursorTest::test_set_transforms_on_text_command()
{
    QFETCH(QString, svg);
    QFETCH(int, pos);
    QFETCH(int, length);
    QFETCH(int, type);
    QFETCH(bool, delta);
    const PkPointF offset(50, 20);
    const SvgTextChangeTransformsOnRange::OffsetType offsetType = SvgTextChangeTransformsOnRange::OffsetType(type);

    KoSvgTextShape *textShape = new KoSvgTextShape();

    KoSvgTextShapeMarkupConverter converter(textShape);
    converter.convertFromSvg(toPkString(svg), PkString(), PkRectF(0, 0, 300, 300), 72.0);

    // Normalize the pos and anchor, so we're sure the index corresponds to the given pos.
    const int indexPos = textShape->indexForPos(pos);
    const int indexAnchor = textShape->indexForPos(pos+length);
    const int posNormalized = textShape->posForIndex(indexPos);
    const int anchor = textShape->posForIndex(indexAnchor);

    PkString currentString = textShape->plainText().mid(indexPos, indexAnchor - indexPos);

    PkList<KoSvgTextCharacterInfo> infos = textShape->getPositionsAndRotationsForRange(posNormalized, anchor);
    PkTransform deltaTf = SvgTextChangeTransformsOnRange::getTransformForOffset(textShape, posNormalized, anchor, offset, offsetType);

    QVector<PkPointF> positions;
    QVector<qreal> rotations;
    if (offsetType == SvgTextChangeTransformsOnRange::OffsetAll) {
        while (!infos.isEmpty()) {
            KoSvgTextCharacterInfo tf = infos.takeFirst();
            positions.append(deltaTf.map(tf.finalPos));
            rotations.append(tf.rotateDeg);
        }
    } else {
        PkLineF l(0, 0, 10, 0);
        l.setAngle(0);
        l = deltaTf.map(l);
        while (!infos.isEmpty()) {
            KoSvgTextCharacterInfo tf = infos.takeFirst();
            if (offsetType != SvgTextChangeTransformsOnRange::RotateOnly) {
                positions.append(deltaTf.map(tf.finalPos));
            } else {
                positions.append(tf.finalPos);
            }
            if (offsetType != SvgTextChangeTransformsOnRange::ScaleOnly) {
                rotations.append(tf.rotateDeg - (l.angle()));
            } else {
                rotations.append(tf.rotateDeg);
            }
        }
    }

    KUndo2Command *cmd = new SvgTextChangeTransformsOnRange(textShape, posNormalized, anchor, offset, offsetType, delta);

    cmd->redo();

    int newPos = textShape->posForIndex(indexPos);
    int newAnchor = textShape->posForIndex(indexAnchor);
    PkList<KoSvgTextCharacterInfo> newInfos = textShape->getPositionsAndRotationsForRange(newPos, newAnchor);

    for (int i = 0; i < positions.size(); i++) {
        KoSvgTextCharacterInfo info = newInfos.value(i);
        if (i > 0 && !delta && info.rtl) {
            /// Absolute positioning is supossed to affect the shaping, which in turn affects advance and positioning
            /// This means we cannot be expected to test rtl beyond the first offset.
            break;
        }
        const PkPointF position = positions.value(i);
        const qreal rotate = rotations.value(i);

        if (offsetType != SvgTextChangeTransformsOnRange::RotateOnly) {
            QCOMPARE(position, info.finalPos);
        }
        if (offsetType != SvgTextChangeTransformsOnRange::ScaleOnly) {
            QCOMPARE(rotate, info.rotateDeg);
        }
    }
}

SIMPLE_TEST_MAIN(SvgTextCursorTest)
