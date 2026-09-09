/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkFlakeBridge.h>
#include "SvgTextTool.h"
#include "KoSvgTextProperties.h"
#include "KoSvgTextShape.h"
#include "KoSvgTextShapeMarkupConverter.h"
#include "SvgCreateTextStrategy.h"
#include "SvgInlineSizeChangeCommand.h"
#include "SvgInlineSizeChangeStrategy.h"
#include "SvgSelectTextStrategy.h"
#include "SvgInlineSizeHelper.h"
#include "SvgMoveTextCommand.h"
#include "SvgMoveTextStrategy.h"
#include "SvgTextChangeCommand.h"
#include "SvgTextRemoveCommand.h"
#include "KoSvgConvertTextTypeCommand.h"
#include "SvgTextShortCuts.h"
#include "SvgTextToolResources.h"
#include "SvgTextTypeSettingStrategy.h"
#include "SvgTextInputMethodAdapter.h"
#include "SvgTextChangeTransformsOnRange.h"
#include "SvgChangeTextPathInfoStrategy.h"
#include "SvgChangeTextPaddingMarginStrategy.h"
#include <commands/KoSvgTextAddRemoveShapeCommands.h>

#include <QApplication>
#include <QStyle>
#include <QAction>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QTextCharFormat>
#include <QTextFormat>
#include <QWidget>

#include <cmath>

#include <klocalizedstring.h>

#include "kis_assert.h"
#include <kis_coordinates_converter.h>
#include <KisCanvasToolServices.h>

#include <KoColor.h>
#include <KoCanvasBase.h>
#include <KoSelection.h>
#include <KoShapeManager.h>
#include <KoShapeController.h>
#include <KoShapeRegistry.h>
#include <KoShapeFactoryBase.h>
#include <KoPointerEvent.h>
#include <KoProperties.h>
#include <KoSelectedShapesProxy.h>
#include "KoToolManager.h"
#include <KoShapeFillWrapper.h>
#include <KoSnapGuide.h>
#include "KoCanvasResourceProvider.h"
#include <KoPathShape.h>
#include <KoPathSegment.h>

#include <KisResourceModelProvider.h>
#include <KoCssStylePreset.h>
#include <KoSvgTextPropertyData.h>
#include <KoColorBackground.h>
#include <KisResourceModel.h>

#include "KisHandlePainterHelper.h"
#include "kis_node.h"
#include "kis_debug.h"
#include <commands/KoKeepShapesSelectedCommand.h>

#ifdef Q_OS_ANDROID
#include <QMenuBar>
#endif


using SvgInlineSizeHelper::InlineSizeInfo;

namespace
{
class QtSvgTextCursorHost final : public SvgTextCursor::HostSurface
{
public:
    explicit QtSvgTextCursorHost(KoCanvasBase *canvas)
        : m_canvas(canvas)
    {
    }

    bool isAvailable() const override { return m_canvas && m_canvas->canvasWidget(); }
    bool hasFocus() const override { return isAvailable() && m_canvas->canvasWidget()->hasFocus(); }
    PkPoint offsetInWindow() const override
    {
        if (!isAvailable()) return {};
        const QPoint point = m_canvas->canvasWidget()->mapTo(m_canvas->canvasWidget()->window(), QPoint());
        return PkPoint(point.x(), point.y());
    }
    PkRectF geometry() const override
    {
        if (!isAvailable()) return {};
        const QRect rect = m_canvas->canvasWidget()->geometry();
        return PkRectF(rect.x(), rect.y(), rect.width(), rect.height());
    }

private:
    KoCanvasBase *m_canvas;
};

int adjustedKeyForTextDirection(int key,
                                KoSvgText::WritingMode writingMode,
                                KoSvgText::Direction direction)
{
    if (direction == KoSvgText::DirectionRightToLeft) {
        if (key == Qt::Key_Left) key = Qt::Key_Right;
        else if (key == Qt::Key_Right) key = Qt::Key_Left;
    }

    if (writingMode == KoSvgText::VerticalRL) {
        if (key == Qt::Key_Left) key = Qt::Key_Down;
        else if (key == Qt::Key_Right) key = Qt::Key_Up;
        else if (key == Qt::Key_Up) key = Qt::Key_Left;
        else if (key == Qt::Key_Down) key = Qt::Key_Right;
    }
    return key;
}

SvgTextCursor::NativeKeyCommand nativeCommandForSequence(const QKeySequence &sequence)
{
    using Command = SvgTextCursor::NativeKeyCommand;
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

}

SvgTextCursor::NativeKeyEvent
svgTextNativeKeyEvent(const PkToolKeyEvent &event,
                      KoSvgText::WritingMode writingMode,
                      KoSvgText::Direction direction)
{
    SvgTextCursor::NativeKeyEvent result;
    result.key = static_cast<int>(event.key());
    result.modifiers = event.modifiers();
    result.text = event.text();
    const int adjustedKey = adjustedKeyForTextDirection(
        static_cast<int>(event.key()), writingMode, direction);
    result.command = nativeCommandForSequence(
        QKeySequence(static_cast<int>(event.modifiers()) | adjustedKey));
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

constexpr double INLINE_SIZE_DASHES_PATTERN_A = 4.0; /// Size of the visible part of the inline-size handle dashes.
constexpr double INLINE_SIZE_DASHES_PATTERN_B = 8.0; /// Size of the hidden part of the inline-size handle dashes.
constexpr int INLINE_SIZE_DASHES_PATTERN_LENGTH = 3; /// Total amount of trailing dashes on inline-size handles.
constexpr double INLINE_SIZE_HANDLE_THICKNESS = 1.0; /// Linethickness.


static bool debugEnabled()
{
    static const bool debugEnabled = !qEnvironmentVariableIsEmpty("KRITA_DEBUG_TEXTTOOL");
    return debugEnabled;
}

SvgTextTool::SvgTextTool(KoCanvasBase *canvas)
    : KoToolBase(canvas)
    , m_cursorHost(std::make_unique<QtSvgTextCursorHost>(canvas))
    , m_textCursor(canvas, m_cursorHost.get())
    , m_textOutlineHelper(new KoSvgTextShapeOutlineHelper(canvas))
{
     // TODO: figure out whether we should use system config for this, Windows and GTK have values for it, but Qt and MacOS don't(?).
    const int cursorFlashLimit = 5000;
    const bool enableCursorWithSelection = QApplication::style()->styleHint(QStyle::SH_BlinkCursorWhenTextSelected);
    m_textCursor.setCaretSetting(QApplication::style()->pixelMetric(QStyle::PM_TextCursorWidth)
                                 , qApp->cursorFlashTime()
                                 , cursorFlashLimit
                                 , enableCursorWithSelection);
    m_textCursor.setDecorationUpdateCallback([this](const PkRectF &rect) { slotUpdateCursorDecoration(rect); });
    m_textCursor.setSelectionChangedCallback([this] { updateTextPathHelper(); });
    m_textCursor.setActionStateChangedCallback([this](const PkString &name, bool checked) {
        QAction *action = m_cursorActions.value(name);
        if (action && action->isCheckable() && action->isChecked() != checked) {
            const QSignalBlocker blocker(action);
            action->setChecked(checked);
        }
    });
    if (canvas->canvasController()) {
        QObject::connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
                         this, [this](int key, const PkVariant &value) {
            m_textCursor.notifyCanvasResourceChanged(key, value);
        });
    }

    for (const PkString &name : SvgTextShortCuts::possibleActions()) {
        QAction *a = action(name);
        if (a) connectCursorAction(name);
    }

    const PkStringList extraActions = {
        "svg_paste_rich_text",
        "svg_paste_plain_text",
        "svg_remove_transforms_from_range",
        "svg_clear_formatting"
    };
    for (const PkString &name : extraActions) {
        QAction *a = action(name);
        if (a) {
            connectCursorAction(name);
        }
    }

    addMappedAction("text_type_preformatted", KoSvgTextShape::PreformattedText, false);
    addMappedAction("text_type_inline_wrap", KoSvgTextShape::InlineWrap, false);
    addMappedAction("text_type_pre_positioned", KoSvgTextShape::PrePositionedText, false);

    addMappedAction("svg_type_setting_move_selection_start_down_1_px", Pk::Key_Down, true);
    addMappedAction("svg_type_setting_move_selection_start_up_1_px", Pk::Key_Up, true);
    addMappedAction("svg_type_setting_move_selection_start_left_1_px", Pk::Key_Left, true);
    addMappedAction("svg_type_setting_move_selection_start_right_1_px", Pk::Key_Right, true);

    m_textOutlineHelper->setDrawBoundingRect(false);
    m_textOutlineHelper->setDrawTextWrappingArea(true);

    m_base_cursor = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::Basic)), 7, 7);
    m_text_inline_horizontal = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::InlineHorizontal)), 7, 7);
    m_text_inline_vertical = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::InlineVertical)), 7, 7);
    m_text_on_path = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::OnPath)), 7, 7);
    m_text_in_shape = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::InShape)), 7, 7);
    m_ibeam_horizontal = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::IBeamHorizontal)), 11, 11);
    m_ibeam_vertical = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::IBeamVertical)), 11, 11);
    m_ibeam_horizontal_done = QCursor(QPixmap(svgTextToolCursorPixmap(SvgTextToolPixmap::IBeamHorizontalDone)), 5, 11);
}

SvgTextTool::~SvgTextTool()
{
}

void SvgTextTool::activate(const PkSet<KoShape *> &shapes)
{
    KoToolBase::activate(shapes);
    QObject::connect(canvas()->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                     this, &SvgTextTool::slotShapeSelectionChanged, Qt::UniqueConnection);

    // toolId() only becomes valid once KoToolManager has set the tool's
    // factory, which happens after construction (KoToolBase::toolId() reads
    // d->factory, set by KoToolManager::Private::createTool() post-ctor; see
    // KoToolBase.cpp / KoToolManager.cpp). So this cannot move to the
    // constructor. Guard it to run only once per tool instance, matching the
    // original "load once, lazily" semantics of the deleted options panel
    // (whose createOptionWidget() was itself only ever called once per tool
    // instance and cached by KoToolBase::optionWidgets()).
    if (!m_optionsDataLoaded) {
        m_optionsData.loadConfig(this->toolId());
        slotUpdateVisualCursor();
        slotUpdateTextPasteBehaviour();
        m_optionsDataLoaded = true;
    }

    canvas()->setCurrentShapeManagerOwnerShape(nullptr);

    useCursor(m_base_cursor);
    slotShapeSelectionChanged();

    repaintDecorations();
}

void SvgTextTool::deactivate()
{
    KoToolBase::deactivate();
    QObject::disconnect(canvas()->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                        this, &SvgTextTool::slotShapeSelectionChanged);
    m_textCursor.setShape(nullptr);
    // Exiting text editing mode is handled by requestStrokeEnd
    m_hoveredShapeHighlightRect = PkPainterPath();

    repaintDecorations();
}

KisPopupWidgetInterface *SvgTextTool::popupWidget()
{
    return nullptr;
}

PkVariant SvgTextTool::inputMethodQuery(Pk::InputMethodQuery query) const
{
    if (canvas()) {
        return m_textCursor.inputMethodQuery(query);
    } else {
        return KoToolBase::inputMethodQuery(query);
    }
}

void SvgTextTool::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_textCursor.inputMethodEvent(svgTextNativeInputMethodEvent(*event))) {
        event->accept();
    } else {
        event->ignore();
    }
}

KoSelection *SvgTextTool::koSelection() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(canvas(), 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(canvas()->selectedShapesProxy(), 0);

    return canvas()->selectedShapesProxy()->selection();
}

KoSvgTextShape *SvgTextTool::selectedShape() const
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(canvas(), 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(canvas()->selectedShapesProxy(), 0);

    PkList<KoShape*> shapes = koSelection()->selectedEditableShapes();
    if (shapes.isEmpty()) return 0;

    KoSvgTextShape *textShape = dynamic_cast<KoSvgTextShape*>(shapes.first());

    return textShape;
}

void SvgTextTool::updateTextPathHelper()
{
    m_textOnPathHelper.setPos(m_textCursor.getPos());
}

PkString SvgTextTool::generateDefs(const KoSvgTextProperties &properties)
{
    PkStringList propStrings;
    PkMap<PkString, PkString> paraProps = properties.convertParagraphProperties();
    for (auto it = paraProps.constBegin(); it != paraProps.constEnd(); it++) {
        propStrings.append(PkString("%1: %2;").arg(it.key()).arg(it.value()));
    }
    paraProps = properties.convertToSvgTextAttributes();
    for (auto it = paraProps.constBegin(); it != paraProps.constEnd(); it++) {
        propStrings.append(PkString("%1: %2;").arg(it.key()).arg(it.value()));
    }

    return PkString("<defs>\n <style>\n  text {\n   %1\n  }\n </style>\n</defs>").arg(propStrings.join("\n   "));
}

KoSvgTextProperties SvgTextTool::propertiesForNewText() const
{
    const bool useCurrent = m_optionsData.useCurrentTextProperties;
    const PkString presetName = m_optionsData.cssStylePresetName;

    KoSvgTextProperties props;
    if (useCurrent || presetName.isEmpty()) {
        KoSvgTextPropertyData textData = canvas()->resourceManager()->resource(KoCanvasResource::SvgTextPropertyData).value<KoSvgTextPropertyData>();
        props = textData.commonProperties;
    } else {
        KisAllResourcesModel *model = KisResourceModelProvider::resourceModel(ResourceType::CssStyles);
        PkVector<KoResourceSP> res = model->resourcesForName(presetName);
        if (res.first()) {
            KoCssStylePresetSP style = res.first().staticCast<KoCssStylePreset>();
            const qreal dpi = canvas()->shapeController()->pixelsPerInch();
            if (style) {
                props = style->properties(dpi, true);
            }
        }
    }

    PkColor fontColor = (canvas()->resourceManager()->isUsingOtherColor()
                ? canvas()->resourceManager()->backgroundColor()
                : canvas()->resourceManager()->foregroundColor()).toQColor();
    PkSharedPointer<KoColorBackground> bg(new KoColorBackground());
    bg->setColor(fontColor);
    KoSvgText::BackgroundProperty bgProp(bg);
    props.setProperty(KoSvgTextProperties::FillId, PkVariant::fromValue(bgProp));
    return props;
}

void SvgTextTool::slotShapeSelectionChanged()
{
    PkList<KoShape *> shapes = koSelection()->selectedEditableShapes();
    if (shapes.size() == 1) {
        KoSvgTextShape *textShape = selectedShape();
        if (!textShape) {
            koSelection()->deselectAll();
            return;
        }
    } else if (shapes.size() > 1) {
        KoSvgTextShape *foundTextShape = nullptr;

        for (KoShape *shape : shapes) {
            KoSvgTextShape *textShape = dynamic_cast<KoSvgTextShape*>(shape);
            if (textShape) {
                foundTextShape = textShape;
                break;
            }
        }

        koSelection()->deselectAll();
        if (foundTextShape) {
            koSelection()->select(foundTextShape);
        }
        return;
    }
    KoSvgTextShape *const shape = selectedShape();
    if (shape != m_textCursor.shape() || shape == nullptr) {
        m_textCursor.setShape(shape);
        m_textOnPathHelper.setShape(shape);
        if (shape) {
            setTextMode(true);
        } else {
            setTextMode(false);
        }
    }
    slotTextTypeUpdated();
}

void SvgTextTool::copy() const
{
    m_textCursor.copy();
}

void SvgTextTool::deleteSelection()
{
    m_textCursor.removeSelection();
}

bool SvgTextTool::paste()
{
    return m_textCursor.paste();
}

bool SvgTextTool::hasSelection()
{
    return m_textCursor.hasSelection();
}

bool SvgTextTool::selectAll()
{
    m_textCursor.moveCursor(SvgTextCursor::ParagraphStart, true);
    m_textCursor.moveCursor(SvgTextCursor::ParagraphEnd, false);
    return true;
}

void SvgTextTool::deselect()
{
    m_textCursor.deselectText();
}

KoToolSelection *SvgTextTool::selection()
{
    return &m_textCursor;
}

void SvgTextTool::requestStrokeEnd()
{
    if (!isActivated()) return;
    if (!m_textCursor.isAddingCommand() && !m_strategyAddingCommand) {
        if (m_interactionStrategy) {
            m_dragging = DragMode::None;
            m_interactionStrategy->cancelInteraction();
            m_interactionStrategy = nullptr;
            useCursor(Qt::ArrowCursor);
            m_textOnPathHelper.setStrategyActive(false);
        }
    }
}

void SvgTextTool::requestStrokeCancellation()
{
    /**
     * Doing nothing, since these signals come on undo/redo actions
     * in the mainland undo stack, which we manipulate while editing
     * text
     */
}

void SvgTextTool::slotUpdateCursorDecoration(PkRectF updateRect)
{
    if (canvas()) {
        canvas()->updateCanvas(updateRect);
    }
}

void SvgTextTool::slotConvertType(int index) {
    KoSvgTextShape *shape = selectedShape();
    if (shape) {
        if (index == shape->textType()) {
            slotTextTypeUpdated();
            return;
        }
        KoSvgTextShape::TextType type = KoSvgTextShape::TextType(index);
        KUndo2Command *parentCommand = new KUndo2Command();
        new KoKeepShapesSelectedCommand({shape}, {}, canvas()->selectedShapesProxy(),
                                        KisCommandUtils::FlipFlopCommand::State::INITIALIZING, parentCommand);
        KoSvgConvertTextTypeCommand *cmd = new KoSvgConvertTextTypeCommand(shape, type, m_textCursor.getPos(), parentCommand);
        parentCommand->setText(cmd->text());
        KoSvgTextRemoveShapeCommand::removeContourShapesFromFlow(shape, parentCommand, shape->textType() == KoSvgTextShape::TextInShape, type == KoSvgTextShape::InlineWrap);
        new KoKeepShapesSelectedCommand({}, {shape}, canvas()->selectedShapesProxy(),
                                        KisCommandUtils::FlipFlopCommand::State::FINALIZING, parentCommand);
        canvas()->addCommand(parentCommand);
        slotTextTypeUpdated();
    }
}

void SvgTextTool::slotUpdateVisualCursor()
{
    m_textCursor.setVisualMode(m_optionsData.useVisualBidiCursor);
}

void SvgTextTool::slotUpdateTextPasteBehaviour()
{
    m_textCursor.setPasteRichTextByDefault(m_optionsData.pasteRichtTextByDefault);
}

void SvgTextTool::slotTextTypeUpdated()
{
    KoSvgTextShape *shape = selectedShape();
    const PkStringList textTypeActions = {
        "text_type_preformatted", "text_type_pre_positioned", "text_type_inline_wrap"
    };
    for (const PkString &name : textTypeActions) {
        if (QAction *hostAction = action(name)) {
            hostAction->setCheckable(true);
            hostAction->setEnabled(shape != nullptr);
        }
    }
    if (shape) {
        action("text_type_preformatted")->setChecked(shape->textType() == KoSvgTextShape::PreformattedText);
        action("text_type_pre_positioned")->setChecked(shape->textType() == KoSvgTextShape::PrePositionedText);
        action("text_type_inline_wrap")->setChecked(shape->textType() == KoSvgTextShape::InlineWrap);
    }
    // Typesetting mode has no UI to activate it, so it is always disabled.
    const PkStringList movementActions = {
        "svg_type_setting_move_selection_start_down_1_px",
        "svg_type_setting_move_selection_start_up_1_px",
        "svg_type_setting_move_selection_start_left_1_px",
        "svg_type_setting_move_selection_start_right_1_px"
    };
    for (const PkString &name : movementActions) {
        if (QAction *hostAction = action(name)) hostAction->setEnabled(false);
    }
    m_textCursor.updateTypeSettingDecorFromShape();
}

void SvgTextTool::slotMoveTextSelection(int index)
{
    KoSvgTextShape *shape = selectedShape();
    if (!shape) return;
    PkPointF offset;
    // test type setting mode.
    if (index == Pk::Key_Down) {
        offset = PkPointF(0, 1);
    } else if (index == Pk::Key_Up) {
        offset = PkPointF(0, -1);
    } else if (index == Pk::Key_Right) {
        offset = PkPointF(-1, 0);
    } else if (index == Pk::Key_Left) {
        offset = PkPointF(1, 0);
    } else {
        return;
    }
    const auto *converter = dynamic_cast<const KisCoordinatesConverter *>(
        canvas()->viewConverter());
    KIS_SAFE_ASSERT_RECOVER_RETURN(converter);
    offset = converter->imageToDocumentTransform().map(offset);
    KUndo2Command *parentCommand = new KUndo2Command();
    new KoKeepShapesSelectedCommand({selectedShape()}, {}, canvas()->selectedShapesProxy(), KisCommandUtils::FlipFlopCommand::State::INITIALIZING, parentCommand);
    KUndo2Command *cmd = new SvgTextChangeTransformsOnRange(shape, m_textCursor.getPos(), m_textCursor.getAnchor(), offset, SvgTextChangeTransformsOnRange::OffsetAll, true, parentCommand);
    new KoKeepShapesSelectedCommand({}, {selectedShape()}, canvas()->selectedShapesProxy(), KisCommandUtils::FlipFlopCommand::State::FINALIZING, parentCommand);
    parentCommand->setText(cmd->text());
    canvas()->addCommand(parentCommand);
}

bool SvgTextTool::nodeEditable()
{
    KisNodeSP node = canvas()->resourceManager()->resource(KoCanvasResource::CurrentKritaNode).value<KisNodeWSP>();
    if (!node->isEditable(true)) {
        if (KisCanvasToolServices *services =
                dynamic_cast<KisCanvasToolServices *>(canvas())) {
            services->toolShowFloatingMessage(services->toolNodeEditableMessage(node));
        }
        return false;
    }
    return true;
}

PkRectF SvgTextTool::decorationsRect() const
{
    PkRectF rect;
    KoSvgTextShape *const shape = selectedShape();
    if (shape) {
        rect |= shape->boundingRect();

        const PkPointF anchor = shape->absoluteTransformation().map(PkPointF());
        rect |= kisGrowRect(PkRectF(anchor, anchor), handleRadius());

        qreal pxlToPt = canvas()->viewConverter()->viewToDocumentX(1.0);
        qreal length = (INLINE_SIZE_DASHES_PATTERN_A + INLINE_SIZE_DASHES_PATTERN_B) * INLINE_SIZE_DASHES_PATTERN_LENGTH;

        if (std::optional<InlineSizeInfo> info = InlineSizeInfo::fromShape(shape, length * pxlToPt)) {
            rect |= kisGrowRect(info->boundingRect(), handleRadius() * 2);
        }

        if (canvas()->snapGuide()->isSnapping()) {
            rect |= canvas()->snapGuide()->boundingRect();
        }
    }

    rect |= m_hoveredShapeHighlightRect.boundingRect();

    rect |= m_textOutlineHelper->decorationRect();
    rect |= m_textOnPathHelper.decorationRect(canvas()->viewConverter()->documentToView());

    return rect;
}

void SvgTextTool::paint(PkPainter &gc, const KoViewConverter &converter)
{
    if (!isActivated()) return;

    if (m_dragging == DragMode::Create || m_dragging == DragMode::InShapeOffset) {
        m_interactionStrategy->paint(gc, converter);
    }

    KoSvgTextShape *shape = selectedShape();
    if (shape) {
        KisHandlePainterHelper handlePainter =
            KoShape::createHandlePainterHelperView(&gc, shape, converter, handleRadius(), decorationThickness());

        if (m_dragging != DragMode::InlineSizeHandle && m_dragging != DragMode::Move && m_dragging != DragMode::TypeSetting) {
            handlePainter.setHandleStyle(KisHandleStyle::primarySelection());
            PkPainterPath path;
            path.addRect(shape->outlineRect());
            handlePainter.drawPath(path);
        }

        qreal pxlToPt = canvas()->viewConverter()->viewToDocumentX(1.0);
        qreal length = (INLINE_SIZE_DASHES_PATTERN_A + INLINE_SIZE_DASHES_PATTERN_B) * INLINE_SIZE_DASHES_PATTERN_LENGTH;
        if (std::optional<InlineSizeInfo> info = InlineSizeInfo::fromShape(shape, length * pxlToPt)) {
            handlePainter.setHandleStyle(KisHandleStyle::secondarySelection());
            handlePainter.drawConnectionLine(info->baselineLineLocal());

            if (m_highlightItem == HighlightItem::InlineSizeStartHandle) {
                handlePainter.setHandleStyle(m_dragging == DragMode::InlineSizeHandle? KisHandleStyle::partiallyHighlightedPrimaryHandles()
                                                                                     : KisHandleStyle::highlightedPrimaryHandles());
            }
            PkVector<qreal> dashPattern = {INLINE_SIZE_DASHES_PATTERN_A, INLINE_SIZE_DASHES_PATTERN_B};
            handlePainter.drawHandleLine(info->startLineLocal());
            handlePainter.drawHandleLine(info->startLineDashes(), INLINE_SIZE_HANDLE_THICKNESS, dashPattern, INLINE_SIZE_DASHES_PATTERN_A);

            handlePainter.setHandleStyle(KisHandleStyle::secondarySelection());
            if (m_highlightItem == HighlightItem::InlineSizeEndHandle) {
                handlePainter.setHandleStyle(m_dragging == DragMode::InlineSizeHandle? KisHandleStyle::partiallyHighlightedPrimaryHandles()
                                                                                     : KisHandleStyle::highlightedPrimaryHandles());
            }
            handlePainter.drawHandleLine(info->endLineLocal());
            handlePainter.drawHandleLine(info->endLineDashes(), INLINE_SIZE_HANDLE_THICKNESS, dashPattern, INLINE_SIZE_DASHES_PATTERN_A);
        }

        if (m_highlightItem == HighlightItem::MoveBorder) {
            handlePainter.setHandleStyle(KisHandleStyle::highlightedPrimaryHandles());
        } else {
            handlePainter.setHandleStyle(KisHandleStyle::primarySelection());
        }
        handlePainter.drawHandleCircle(PkPointF(), KoToolBase::handleRadius() * 0.75);
    }

    m_textOutlineHelper->setDecorationThickness(decorationThickness());
    m_textOutlineHelper->setHandleRadius(handleRadius());
    m_textOutlineHelper->paint(&gc, converter);
    m_textOnPathHelper.setDecorationThickness(decorationThickness());
    m_textOnPathHelper.setHandleRadius(handleRadius());
    m_textOnPathHelper.paint(&gc, converter);
    gc.setTransform(converter.documentToView(), true);
    {
        KisHandlePainterHelper handlePainter(&gc, handleRadius(), decorationThickness());
        if (!m_hoveredShapeHighlightRect.isEmpty()) {
            handlePainter.setHandleStyle(KisHandleStyle::highlightedPrimaryHandlesWithSolidOutline());
            PkPainterPath path;
            path.addPath(m_hoveredShapeHighlightRect);
            handlePainter.drawPath(path);
        }
    }
    if (shape) {
        m_textCursor.paintDecorations(gc, toPkColor(qApp->palette().color(QPalette::Highlight)), decorationThickness(), handleRadius());
    }
    if (m_interactionStrategy) {
        gc.save();
        canvas()->snapGuide()->paint(gc, converter);
        gc.restore();
    }

    // Paint debug outline. Character-bbox debug was on by default whenever
    // KRITA_DEBUG_TEXTTOOL was set (line-box debug was off by default); there
    // is no UI to change either anymore, so these defaults are now fixed.
    if (debugEnabled() && shape) {
        gc.save();
        const KoSvgTextShape::DebugElements el{KoSvgTextShape::DebugElement::CharBbox};

        gc.setTransform(shape->absoluteTransformation(), true);
        shape->paintDebug(gc, el);
        gc.restore();
    }
}

void SvgTextTool::mousePressEvent(KoPointerEvent *event)
{
    // When using touch drawing, we only ever receive move events after the
    // finger has pressed down. We have to issue an artificial move here so that
    // the tool's state is updated properly to handle the press.
    if (event->isTouchEvent()) {
        mouseMoveEvent(event);
    }

    if (!nodeEditable()) {
        event->accept();
        return;
    }

    KoSvgTextShape *selectedShape = this->selectedShape();

    if (selectedShape) {
        if (m_textOutlineHelper->contourModeButtonHovered(event->point)) {
            m_textOutlineHelper->toggleTextContourMode(selectedShape);
            event->accept();
            KoToolManager::instance()->switchToolRequested("InteractionTool");
            return;
        }

        if (m_highlightItem == HighlightItem::TypeSettingHandle) {
            SvgTextCursor::TypeSettingModeHandle handle = m_textCursor.typeSettingHandleAtPos(handleGrabRect(event->point));
            if (handle != SvgTextCursor::NoHandle) {
                if (!m_textCursor.setDominantBaselineFromHandle(handle)) {
                    m_interactionStrategy.reset(new SvgTextTypeSettingStrategy(
                        this,
                        selectedShape,
                        &m_textCursor,
                        handleGrabRect(event->point),
                        Pk::KeyboardModifiers(static_cast<int>(event->modifiers()))));
                    m_dragging = DragMode::TypeSetting;
                    m_textCursor.setDrawTypeSettingHandle(false);
                }
                event->accept();
                return;
            }
        } else if (SvgChangeTextPaddingMarginStrategy::hitTest(selectedShape, event->point, grabSensitivityInPt())) {
            m_interactionStrategy.reset(new SvgChangeTextPaddingMarginStrategy(this, selectedShape, event->point));
            m_dragging = DragMode::InShapeOffset;
            m_textOutlineHelper->setDrawTextWrappingArea(false);
            event->accept();
            return;
        } else if (m_textOnPathHelper.hitTest(event->point, canvas()->viewConverter()->viewToDocument())) {
            m_interactionStrategy.reset(new SvgChangeTextPathInfoStrategy(this, selectedShape, event->point, m_textCursor.getPos()));
            m_dragging = DragMode::TextPathHandle;
            m_textOnPathHelper.setStrategyActive(true);
            event->accept();
            return;
        } else if (m_highlightItem == HighlightItem::MoveBorder) {
            m_interactionStrategy.reset(new SvgMoveTextStrategy(this, selectedShape, event->point));
            m_dragging = DragMode::Move;
            event->accept();
            return;
        } else if (m_highlightItem == HighlightItem::InlineSizeEndHandle) {
            m_interactionStrategy.reset(new SvgInlineSizeChangeStrategy(this, selectedShape, event->point, false));
            m_dragging = DragMode::InlineSizeHandle;
            event->accept();
            return;
        }  else if (m_highlightItem == HighlightItem::InlineSizeStartHandle) {
            m_interactionStrategy.reset(new SvgInlineSizeChangeStrategy(this, selectedShape, event->point, true));
            m_dragging = DragMode::InlineSizeHandle;
            event->accept();
            return;
        }
    }

    KoSvgTextShape *hoveredShape = dynamic_cast<KoSvgTextShape *>(canvas()->shapeManager()->shapeAt(event->point));
    KoPathShape *hoveredFlowShape = dynamic_cast<KoPathShape *>(canvas()->shapeManager()->shapeAt(event->point));
    PkString shapeType;
    PkPainterPath hoverPath = dynamic_cast<KisCanvasToolServices *>(canvas())
                                  ->toolShapeHoverInfoCrossLayer(event->point, shapeType);
    bool crossLayerPossible = !hoverPath.isEmpty() && shapeType == KoSvgTextShape_SHAPEID;

    if (!selectedShape && !hoveredShape && !hoveredFlowShape && !crossLayerPossible) {
        PkPointF point = canvas()->snapGuide()->snap(
            event->point, Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
        m_interactionStrategy.reset(new SvgCreateTextStrategy(this, point));
        m_dragging = DragMode::Create;
        event->accept();
    } else if (hoveredShape) {
        if (hoveredShape != selectedShape) {
            canvas()->shapeManager()->selection()->deselectAll();
            canvas()->shapeManager()->selection()->select(hoveredShape);
            m_hoveredShapeHighlightRect = PkPainterPath();
        }
        m_interactionStrategy.reset(new SvgSelectTextStrategy(
            this,
            &m_textCursor,
            event->point,
            Pk::KeyboardModifiers(static_cast<int>(event->modifiers()))));
        m_dragging = DragMode::Select;
        event->accept();
    } else if (hoveredFlowShape) {
        PkPointF point = canvas()->snapGuide()->snap(
            event->point, Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
        m_interactionStrategy.reset(new SvgCreateTextStrategy(this, point, hoveredFlowShape));
        m_dragging = DragMode::Create;
        event->accept();
    } else if (crossLayerPossible) {
        if (dynamic_cast<KisCanvasToolServices *>(canvas())
                ->toolSelectShapeCrossLayer(event->point, KoSvgTextShape_SHAPEID)) {
            m_interactionStrategy.reset(new SvgSelectTextStrategy(
                this,
                &m_textCursor,
                event->point,
                Pk::KeyboardModifiers(static_cast<int>(event->modifiers()))));
            m_dragging = DragMode::Select;
            m_hoveredShapeHighlightRect = PkPainterPath();
        } else {
            canvas()->shapeManager()->selection()->deselectAll();
        }
        event->accept();
    } else { // if there's a selected shape but no hovered shape...
        canvas()->shapeManager()->selection()->deselectAll();
        event->accept();
    }

    repaintDecorations();
}

static inline Qt::CursorShape angleToCursor(const PkPointF &unit)
{
    constexpr float SIN_PI_8 = 0.382683432;
    if (unit.y() < SIN_PI_8 && unit.y() > -SIN_PI_8) {
        return Qt::SizeHorCursor;
    } else if (unit.x() < SIN_PI_8 && unit.x() > -SIN_PI_8) {
        return Qt::SizeVerCursor;
    } else if ((unit.x() > 0 && unit.y() > 0) || (unit.x() < 0 && unit.y() < 0)) {
        return Qt::SizeFDiagCursor;
    } else {
        return Qt::SizeBDiagCursor;
    }
}

static inline Qt::CursorShape lineToCursor(const PkLineF &line, const KoCanvasBase *const canvas)
{
    const auto *converter = dynamic_cast<const KisCoordinatesConverter *>(
        canvas->viewConverter());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(converter, Qt::ArrowCursor);
    const PkLineF wdgLine = converter->flakeToWidget(line);
    const PkPointF vector = wdgLine.p2() - wdgLine.p1();
    const qreal length = std::hypot(vector.x(), vector.y());
    return angleToCursor(length > 0.0 ? vector / length : PkPointF());
}

void SvgTextTool::mouseMoveEvent(KoPointerEvent *event)
{
    m_lastMousePos = event->point;
    m_hoveredShapeHighlightRect = PkPainterPath();
    m_textCursor.updateModifiers(Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));

    if (m_interactionStrategy) {
        m_interactionStrategy->handleMouseMove(
            event->point, Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
        if (m_dragging == DragMode::Create) {
            SvgCreateTextStrategy *c = dynamic_cast<SvgCreateTextStrategy*>(m_interactionStrategy.get());
            if (c && c->draggingInlineSize() && !c->hasWrappingShape()) {
                if (this->writingMode() == KoSvgText::HorizontalTB) {
                    useCursor(m_text_inline_horizontal);
                } else {
                    useCursor(m_text_inline_vertical);
                }
            } else {
                useCursor(m_base_cursor);
            }
        } else if (m_dragging == DragMode::Select && this->selectedShape()) {
            KoSvgTextShape *const selectedShape = this->selectedShape();
            // Todo: replace with something a little less hacky.
            if (selectedShape->writingMode() == KoSvgText::HorizontalTB) {
                useCursor(m_ibeam_horizontal);
            } else {
                useCursor(m_ibeam_vertical);
            }
        } else if (m_dragging != DragMode::InShapeOffset) {
            useCursor(Qt::ArrowCursor);
        }
        event->accept();
    } else {
        m_highlightItem = HighlightItem::None;

        KoSvgTextShape *const selectedShape = this->selectedShape();
        QCursor cursor = m_base_cursor;
        if (selectedShape) {
            cursor = m_ibeam_horizontal_done;
            const qreal sensitivity = grabSensitivityInPt();

            SvgTextCursor::TypeSettingModeHandle handle = m_textCursor.typeSettingHandleAtPos(handleGrabRect(event->point));
            m_textCursor.setTypeSettingHandleHovered(handle);
            if (handle != SvgTextCursor::NoHandle) {
                cursor = QCursor(static_cast<Qt::CursorShape>(m_textCursor.cursorTypeForTypeSetting()));
                m_highlightItem = HighlightItem::TypeSettingHandle;
            }

            if (m_highlightItem == HighlightItem::None) {
                if (std::optional<InlineSizeInfo> info = InlineSizeInfo::fromShape(selectedShape)) {
                    const PkPolygonF zone = info->endLineGrabRect(sensitivity);
                    const PkPolygonF startZone = info->startLineGrabRect(sensitivity);
                    if (zone.containsPoint(event->point, Pk::OddEvenFill)) {
                        m_highlightItem = HighlightItem::InlineSizeEndHandle;
                        cursor = lineToCursor(info->baselineLine(), canvas());
                    } else if (startZone.containsPoint(event->point, Pk::OddEvenFill)){
                        m_highlightItem = HighlightItem::InlineSizeStartHandle;
                        cursor = lineToCursor(info->baselineLine(), canvas());
                    }
                }
            }

            if (m_highlightItem == HighlightItem::None) {
                const PkPolygonF textOutline = selectedShape->absoluteTransformation().map(selectedShape->outlineRect());
                const PkPolygonF moveBorderRegion = selectedShape->absoluteTransformation().map(kisGrowRect(selectedShape->outlineRect(),
                                                                                                           sensitivity * 2));
                if (moveBorderRegion.containsPoint(event->point, Pk::OddEvenFill) && !textOutline.containsPoint(event->point, Pk::OddEvenFill)) {
                    m_highlightItem = HighlightItem::MoveBorder;
                    cursor = Qt::SizeAllCursor;
                }
            }
        }

        PkString shapeType;
        bool isHorizontal = true;
        const KoSvgTextShape *hoveredShape = dynamic_cast<KoSvgTextShape *>(canvas()->shapeManager()->shapeAt(event->point));
        const KoPathShape *hoveredFlowShape = dynamic_cast<KoPathShape *>(canvas()->shapeManager()->shapeAt(event->point));
        PkPainterPath hoverPath = dynamic_cast<KisCanvasToolServices *>(canvas())
                                      ->toolShapeHoverInfoCrossLayer(event->point,
                                                                    shapeType,
                                                                    &isHorizontal);

        bool textAreasHovered = false;
        if (m_textOnPathHelper.hitTest(event->point, canvas()->viewConverter()->viewToDocument()) ) {
            cursor = Qt::ArrowCursor;
        } else if(std::optional<PkPointF> offsetVector = SvgChangeTextPaddingMarginStrategy::hitTest(selectedShape, event->point, grabSensitivityInPt())) {
            cursor = lineToCursor(PkLineF(PkPointF(), offsetVector.value()).normalVector(), canvas());
            textAreasHovered = true;
        } else if (selectedShape && selectedShape == hoveredShape && m_highlightItem == HighlightItem::None) {
            if (selectedShape->writingMode() == KoSvgText::HorizontalTB) {
                cursor = m_ibeam_horizontal;
            } else {
                cursor = m_ibeam_vertical;
            }
        } else if (hoveredShape && m_highlightItem == HighlightItem::None) {
            if (!hoveredShape->textWrappingAreas().isEmpty()) {
                PK_FOREACH(const PkPainterPath &path, hoveredShape->textWrappingAreas()) {
                    m_hoveredShapeHighlightRect.addPath(hoveredShape->absoluteTransformation().map(path));
                }
            } else {
                m_hoveredShapeHighlightRect.addRect(hoveredShape->boundingRect());
            }
            if (hoveredShape->writingMode() == KoSvgText::HorizontalTB) {
                cursor = m_ibeam_horizontal;
            } else {
                cursor = m_ibeam_vertical;
            }
        } else if (hoveredFlowShape) {
            m_hoveredShapeHighlightRect.addPath(hoveredFlowShape->absoluteTransformation().map(hoveredFlowShape->outline()));
            if (hoveredFlowShape->segmentAtPoint(event->point, handleGrabRect(event->point)).isValid()) {
                cursor = m_text_on_path;
            } else {
                cursor = m_text_in_shape;
            }
        } else if (!hoverPath.isEmpty() && shapeType == KoSvgTextShape_SHAPEID && m_highlightItem == HighlightItem::None) {
            m_hoveredShapeHighlightRect = hoverPath;
            if (isHorizontal) {
                cursor = m_ibeam_horizontal;
            } else {
                cursor = m_ibeam_vertical;
            }
        }
        m_textOutlineHelper->setTextAreasHovered(textAreasHovered);
        useCursor(cursor);
        event->ignore();
    }

    repaintDecorations();
}

void SvgTextTool::mouseReleaseEvent(KoPointerEvent *event)
{
    if (m_interactionStrategy) {
        m_interactionStrategy->finishInteraction(
            Pk::KeyboardModifiers(static_cast<int>(event->modifiers())));
        KUndo2Command *const command = m_interactionStrategy->createCommand();
        if (command) {
            m_strategyAddingCommand = true;
            canvas()->addCommand(command);
            m_strategyAddingCommand = false;
        }
        m_interactionStrategy = nullptr;
        if (m_dragging != DragMode::Select) {
            useCursor(m_base_cursor);
        }
        m_textOnPathHelper.setStrategyActive(false);
        m_textOutlineHelper->setDrawTextWrappingArea(true);
        m_dragging = DragMode::None;
        m_textCursor.setDrawTypeSettingHandle(true);
        event->accept();
    } else {
        useCursor(m_base_cursor);
    }
    event->accept();
}

void SvgTextTool::pkKeyPressEvent(PkToolKeyEvent *event)
{
    if (m_interactionStrategy
            && (event->key() == Pk::Key_Control || event->key() == Pk::Key_Alt || event->key() == Pk::Key_Shift
                || event->key() == Pk::Key_Meta)) {
        m_interactionStrategy->handleMouseMove(m_lastMousePos, event->modifiers());
        event->accept();
        return;
    } else if (event->key() == Pk::Key_Escape) {
        requestStrokeEnd();
    } else if (selectedShape()) {
        const KoSvgTextProperties properties = selectedShape()->textProperties();
        const auto writingMode = KoSvgText::WritingMode(
            properties.propertyOrDefault(KoSvgTextProperties::WritingModeId).toInt());
        const auto direction = KoSvgText::Direction(
            properties.propertyOrDefault(KoSvgTextProperties::DirectionId).toInt());
        const SvgTextCursor::NativeKeyEvent nativeEvent =
            svgTextNativeKeyEvent(*event, writingMode, direction);
        const bool consumed = m_textCursor.keyPressEvent(nativeEvent, [this, event, writingMode, direction] {
            const int adjustedKey = adjustedKeyForTextDirection(
                static_cast<int>(event->key()), writingMode, direction);
            const QKeySequence sequence(static_cast<int>(event->modifiers()) | adjustedKey);
            for (auto it = m_cursorActions.constBegin(); it != m_cursorActions.constEnd(); ++it) {
                QAction *hostAction = it.value();
                if (hostAction && hostAction->shortcut() == sequence) {
                    hostAction->trigger();
                    return true;
                }
            }
            return false;
        });
        if (consumed) {
            event->accept();
            return;
        }
    }

    event->ignore();
}

void SvgTextTool::pkKeyReleaseEvent(PkToolKeyEvent *event)
{
    m_textCursor.updateModifiers(event->modifiers());
    if (m_interactionStrategy
            && (event->key() == Pk::Key_Control || event->key() == Pk::Key_Alt || event->key() == Pk::Key_Shift
                || event->key() == Pk::Key_Meta)) {
        m_interactionStrategy->handleMouseMove(m_lastMousePos, event->modifiers());
        event->accept();
    } else {
        event->ignore();
    }
}

void SvgTextTool::focusInEvent(QFocusEvent *event)
{
    m_textCursor.focusIn();
    event->accept();
}

void SvgTextTool::focusOutEvent(QFocusEvent *event)
{
    m_textCursor.focusOut();
    event->accept();
}

void SvgTextTool::mouseDoubleClickEvent(KoPointerEvent *event)
{
    if (canvas()->shapeManager()->shapeAt(event->point) != selectedShape()) {
        event->ignore(); // allow the event to be used by another
        return;
    } else {
        m_textCursor.setPosToPoint(event->point, true);
        m_textCursor.moveCursor(SvgTextCursor::MoveWordLeft, true);
        m_textCursor.moveCursor(SvgTextCursor::MoveWordRight, false);
    }
    const PkRectF updateRect = std::exchange(m_hoveredShapeHighlightRect, PkPainterPath()).boundingRect();
    canvas()->updateCanvas(kisGrowRect(updateRect, 100));
    event->accept();
}

void SvgTextTool::mouseTripleClickEvent(KoPointerEvent *event)
{
    if (canvas()->shapeManager()->shapeAt(event->point) == selectedShape()) {
        // TODO: Consider whether we want to use sentence based selection instead:
        // QTextBoundaryFinder allows us to find sentences if necessary.
        m_textCursor.moveCursor(SvgTextCursor::ParagraphStart, true);
        m_textCursor.moveCursor(SvgTextCursor::ParagraphEnd, false);
        event->accept();
    }
}

qreal SvgTextTool::grabSensitivityInPt() const
{
    const int sensitivity = grabSensitivity();
    return canvas()->viewConverter()->viewToDocumentX(sensitivity);
}

KoSvgText::WritingMode SvgTextTool::writingMode() const
{
    KoSvgTextProperties props = propertiesForNewText();
    return KoSvgText::WritingMode(props.propertyOrDefault(KoSvgTextProperties::WritingModeId).toInt());
}

void SvgTextTool::connectCursorAction(const PkString &actionName)
{
    QAction *hostAction = action(actionName);
    if (!hostAction) return;
    m_cursorActions.insert(actionName, hostAction);
    QObject::connect(hostAction, &QAction::triggered, this,
                     [this, actionName](bool checked) {
        m_textCursor.triggerAction(actionName, checked);
    });
}

void SvgTextTool::addMappedAction(const PkString &actionName, int value, bool movementAction)
{
    QAction *hostAction = action(actionName);
    if (!hostAction) return;
    m_cursorActions.insert(actionName, hostAction);
    QObject::connect(hostAction, &QAction::triggered, this,
                     [this, value, movementAction] {
        if (movementAction) slotMoveTextSelection(value);
        else slotConvertType(value);
    });
}
