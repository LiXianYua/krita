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
#include <SvgTextToolOptionsData.h>
#include <PkConfigGroup.h>
#include <KConfig>
#include <KConfigGroup>
#include <QTemporaryDir>

#include <KoSvgTextShape.h>
#include <KoSvgTextShapeMarkupConverter.h>
#include <KoFontRegistry.h>
#include <KoCanvasController.h>
#include <KoViewConverter.h>
#include <QInputMethod>
#include <QWidget>

#include <tests/MockShapes.h>
#include <simpletest.h>
#include <testui.h>

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
    CursorController() : KoCanvasController(nullptr) {}
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
    KoCanvasBase *currentCanvas = nullptr;
};
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
        QAction action;
        action.setObjectName(QString::fromLatin1(item.name));
        action.setCheckable(true);
        action.setChecked(item.checked);
        QVERIFY(SvgTextShortCuts::configureAction(&action, item.name));
        KoSvgTextProperties properties;
        properties.setProperty(KoSvgTextProperties::PropertyId(item.property), PkVariant(item.before.toDouble()));
        const auto result = SvgTextShortCuts::getModifiedProperties(&action, {properties});
        QCOMPARE(result.property(KoSvgTextProperties::PropertyId(item.property)).toDouble(), item.after.toDouble());
    }
    QVERIFY(!SvgTextShortCuts::actionEnabled(nullptr, {}));
    QVERIFY(!SvgTextShortCuts::configureAction(nullptr, "svg_weight_bold"));
    QAction unknown;
    unknown.setObjectName("unknown");
    QVERIFY(!SvgTextShortCuts::configureAction(&unknown, "unknown"));
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
    CursorCanvas canvas;
    CursorController controller;
    controller.setCanvas(&canvas);
    canvas.setCanvasController(&controller);
    KoSvgTextShape shape;
    KoSvgTextShapeMarkupConverter converter(&shape);
    QVERIFY(converter.convertFromSvg("<text font-size=\"10\">Hello</text>", {}, PkRectF(0, 0, 300, 300), 72.0));
    SvgTextCursor cursor(&canvas);
    cursor.setShape(&shape);
    cursor.focusIn();
    QInputMethod *ime = QGuiApplication::inputMethod();
    canvas.widget.move(17, 23);
    controller.proxyObject->emitSizeChanged(PkSize(640, 480));
    QCOMPARE(ime->inputItemTransform().map(QPointF()), QPointF(17, 23));
    canvas.widget.move(31, 47);
    controller.proxyObject->emitMoveDocumentOffset(PkPointF(), PkPointF(14, 24));
    QCOMPARE(ime->inputItemTransform().map(QPointF()), QPointF(31, 47));
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
