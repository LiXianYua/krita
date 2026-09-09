/* This file is part of the KDE project
 *
   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SVG_TEXT_TOOL
#define SVG_TEXT_TOOL


#include <KoToolBase.h>
#include <PkMap.h>
#include <QCursor>

#include <KoSvgTextShapeOutlineHelper.h>


#include "SvgTextCursor.h"
#include "SvgTextToolOptionsData.h"
#include "SvgTextOnPathDecorationHelper.h"

#include <memory>

class KoSelection;
class KoSvgTextShape;
class KoInteractionStrategy;
class KUndo2Command;
class QAction;

class SvgTextTool : public KoToolBase
{
    friend class SvgCreateTextStrategy;
    friend class SvgChangeTextPathInfoStrategy;

public:
    explicit SvgTextTool(KoCanvasBase *canvas);
    ~SvgTextTool() override;
    /// reimplemented from KoToolBase
    PkRectF decorationsRect() const override;
    /// reimplemented from KoToolBase
    void paint(PkPainter &gc, const KoViewConverter &converter) override;
    /// reimplemented from KoToolBase
    void mousePressEvent(KoPointerEvent *event) override;
    /// reimplemented from superclass
    void mouseDoubleClickEvent(KoPointerEvent *event) override;
    /// reimplemented from KoToolBase
    void mouseTripleClickEvent(KoPointerEvent *event) override;
    /// reimplemented from KoToolBase
    void mouseMoveEvent(KoPointerEvent *event) override;
    /// reimplemented from KoToolBase
    void mouseReleaseEvent(KoPointerEvent *event) override;

    void pkKeyPressEvent(PkToolKeyEvent *event) override;
    void pkKeyReleaseEvent(PkToolKeyEvent *event) override;

    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

    /// reimplemented from KoToolBase
    void activate(const PkSet<KoShape *> &shapes) override;
    /// reimplemented from KoToolBase
    void deactivate() override;

    KisPopupWidgetInterface* popupWidget() override;

    PkVariant inputMethodQuery(Pk::InputMethodQuery query) const override;
    void inputMethodEvent(QInputMethodEvent *event) override;

    /// reimplemented from superclass
    void copy() const override;
    /// reimplemented from superclass
    void deleteSelection() override;
    /// reimplemented from superclass
    bool paste() override;
    /// reimplemented from superclass
    bool hasSelection() override;

    bool selectAll() override;

    void deselect() override;
    /// reimplemented from superclass
    KoToolSelection * selection() override;
    
    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;

protected:
    KoSelection *koSelection() const;
    KoSvgTextShape *selectedShape() const;

private:
    qreal grabSensitivityInPt() const;

    KoSvgText::WritingMode writingMode() const;

    void connectCursorAction(const PkString &actionName);
    void addMappedAction(const PkString &actionName, int value, bool movementAction);

    /**
     * @brief nodeEditable
     * @see nodeEditable in KisTool.
     * @return whether the current Node is editable. If not, it'll display an oncanvas message.
     */
    bool nodeEditable();

private:

    void updateTextPathHelper();

    /**
     * @brief generateDefs
     * This generates a defs section with the appropriate
     * css and css strings assigned.
     */
    PkString generateDefs(const KoSvgTextProperties &properties = KoSvgTextProperties());

    /**
     * @brief propertiesForNewText
     * get the text properties that should be used for new text.
     */
    KoSvgTextProperties propertiesForNewText() const;

    /**
     * @brief selectionChanged
     * called when the canvas selection is changed.
     */
    void slotShapeSelectionChanged();

    /**
     * @brief updateCursor
     * update the canvas decorations in a particular update rect for the text cursor.
     * @param updateRect the rect to update in.
     */
    void slotUpdateCursorDecoration(PkRectF updateRect);

    /**
     * @brief slotConvertType
     * @param index
     */
    void slotConvertType(int index);

    /**
     * @brief slotUpdateVisualCursor
     * update the visual cursor mode on the text cursor.
     */
    void slotUpdateVisualCursor();

    /**
     * @brief slotUpdateTextPasteBehaviour
     * update the default text paste behaviour.
     */
    void slotUpdateTextPasteBehaviour();

    /**
     * @brief slotTextTypeUpdated
     * Update the text type in the tool options.
     */
    void slotTextTypeUpdated();

    /**
     * @brief slotMoveTextSelection
     * Move the start of the selection in typesetting mode by image 1 pix.
     * @param index -- Qt key for a direction.
     */
    void slotMoveTextSelection(int index);

private:
    enum class DragMode {
        None = 0,
        Create,
        Select,
        InlineSizeHandle,
        Move,
        TextPathHandle,
        InShapeOffset,
        TypeSetting,
    };
    enum class HighlightItem {
        None = 0,
        InlineSizeStartHandle,
        InlineSizeEndHandle,
        MoveBorder,
        TypeSettingHandle,
    };

    SvgTextToolOptionsData m_optionsData;
    bool m_optionsDataLoaded {false};
    PkPointF m_lastMousePos;
    DragMode m_dragging {DragMode::None};
    std::unique_ptr<KoInteractionStrategy> m_interactionStrategy;
    HighlightItem m_highlightItem {HighlightItem::None};
    bool m_strategyAddingCommand {false};

    std::unique_ptr<SvgTextCursor::HostSurface> m_cursorHost;
    SvgTextCursor m_textCursor;
    PkMap<PkString, QAction *> m_cursorActions;
    SvgTextOnPathDecorationHelper m_textOnPathHelper;
    std::unique_ptr<KoSvgTextShapeOutlineHelper> m_textOutlineHelper;

    PkPainterPath m_hoveredShapeHighlightRect;

    QCursor m_base_cursor;
    QCursor m_text_inline_horizontal;
    QCursor m_text_inline_vertical;
    QCursor m_text_on_path;
    QCursor m_text_in_shape;
    QCursor m_ibeam_vertical;
    QCursor m_ibeam_horizontal;
    QCursor m_ibeam_horizontal_done;

};

#endif
