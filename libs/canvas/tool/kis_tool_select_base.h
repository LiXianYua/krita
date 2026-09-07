/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISTOOLSELECTBASE_H
#define KISTOOLSELECTBASE_H

#include <QKeyEvent>
#include <QAction>

#include <PkList.h>
#include <PkPainterPath.h>
#include <PkSet.h>
#include <PkString.h>

#include <KoCanvasBase.h>
#include "KoPointerEvent.h"
#include "kis_tool.h"
#include "kis_selection.h"
#include "KisSelectionUtils.h"
#include "kis_selection_options.h"
#include "kis_selection_tool_config_widget_helper.h"
#include "kis_selection_modifier_mapper.h"
#include "strokes/move_stroke_strategy.h"
#include "kis_image.h"
#include "KisQtConnectionsStore.h"
#include "kis_assert.h"
#include "canvas/kis_coordinates_converter.h"
#include <KisCanvasToolServices.h>

/**
 * This is a basic template to create selection tools from basic path based drawing tools.
 * The template overrides the ability to execute alternate actions correctly.
 * The default behavior for the modifier keys is as follows:
 *
 * Shift: add to selection
 * Alt: subtract from selection
 * Shift+Alt: intersect current selection
 * Ctrl+Alt: symmetric difference
 * Ctrl: replace selection
 *
 * The mapping itself is done in KisSelectionModifierMapper.
 *
 * Certain tools also use modifier keys to alter their behavior, e.g. forcing square proportions with the rectangle tool.
 * The template enables the following rules for forwarding keys:

 * 1) If the user is not selecting, then changing the modifier combination
 *    changes the selection method.
 * 
 * 2) If the user is selecting then the modifier keys are forwarded to the
 *    specific tool, so that it can do with them whatever it wants. The selection
 *    method is not changed in this stage and it will be the same as just before
 *    the user started selecting.
 * 
 * 3) Once the user finishes selecting, the selection method is updated to reflect
 *    the current modifier combination
 * 
 * 4) If the user is moving the selection, then changing the modifiers 
 */

template <class BaseClass>
class KisToolSelectBase : public BaseClass
{

public:

    KisToolSelectBase(KoCanvasBase *canvas, const PkString &toolName)
        : BaseClass(canvas)
        , m_widgetHelper(toolName)
        , m_selectionActionAlternate(SELECTION_DEFAULT)
    {
        KisSelectionModifierMapper::instance();
        initializeSelectionState();
    }

    KisToolSelectBase(KoCanvasBase *canvas, const QCursor cursor, const PkString &toolName)
        : BaseClass(canvas, cursor)
        , m_widgetHelper(toolName)
        , m_selectionActionAlternate(SELECTION_DEFAULT)
    {
        KisSelectionModifierMapper::instance();
        initializeSelectionState();
    }

    template<typename DelegateTool>
    KisToolSelectBase(KoCanvasBase *canvas,
                      QCursor cursor,
                      const PkString &toolName,
                      DelegateTool *delegateTool)
        : BaseClass(canvas, cursor, delegateTool)
        , m_widgetHelper(toolName)
        , m_selectionActionAlternate(SELECTION_DEFAULT)
    {
        KisSelectionModifierMapper::instance();
        initializeSelectionState();
    }

    enum SampleLayersMode
    {
        SampleAllLayers,
        SampleCurrentLayer,
        SampleColorLabeledLayers,
    };

    void activate(const PkSet<KoShape *> &shapes) override
    {
        BaseClass::activate(shapes);

        m_widgetHelper.setConfigGroupForExactTool(this->toolId());
        m_widgetHelper.slotToolActivatedChanged(true);

        m_modeConnections.addUniqueConnection(
            this->action("selection_tool_mode_replace"), &QAction::triggered,
            &m_widgetHelper, &KisSelectionToolConfigWidgetHelper::slotReplaceModeRequested);

        m_modeConnections.addUniqueConnection(
            this->action("selection_tool_mode_add"), &QAction::triggered,
            &m_widgetHelper, &KisSelectionToolConfigWidgetHelper::slotAddModeRequested);

        m_modeConnections.addUniqueConnection(
            this->action("selection_tool_mode_subtract"), &QAction::triggered,
            &m_widgetHelper, &KisSelectionToolConfigWidgetHelper::slotSubtractModeRequested);

        m_modeConnections.addUniqueConnection(
            this->action("selection_tool_mode_intersect"), &QAction::triggered,
            &m_widgetHelper, &KisSelectionToolConfigWidgetHelper::slotIntersectModeRequested);

    }

    void deactivate() override
    {
        m_widgetHelper.slotToolActivatedChanged(false);
        BaseClass::deactivate();
        m_modeConnections.clear();
    }

    SelectionMode selectionMode() const
    {
        return m_widgetHelper.selectionMode();
    }

    SelectionAction selectionAction() const
    {
        if (alternateSelectionAction() == SELECTION_DEFAULT) {
            return m_widgetHelper.selectionAction();
        }
        return alternateSelectionAction();
    }

    bool antiAliasSelection() const
    {
        return m_widgetHelper.antiAliasSelection();
    }

    int growSelection() const
    {
        return m_widgetHelper.growSelection();
    }

    bool stopGrowingAtDarkestPixel() const
    {
        return m_widgetHelper.stopGrowingAtDarkestPixel();
    }

    int featherSelection() const
    {
        return m_widgetHelper.featherSelection();
    }

    PkList<int> colorLabelsSelected() const
    {
        return m_widgetHelper.selectedColorLabels();
    }

    SampleLayersMode sampleLayersMode() const
    {
        KisSelectionOptions::ReferenceLayers referenceLayers =
            m_widgetHelper.referenceLayers();
        if (referenceLayers == KisSelectionOptions::AllLayers) {
            return SampleAllLayers;
        } else if (referenceLayers == KisSelectionOptions::CurrentLayer) {
            return SampleCurrentLayer;
        } else if (referenceLayers == KisSelectionOptions::ColorLabeledLayers) {
            return SampleColorLabeledLayers;
        }
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(true, SampleAllLayers);
        return SampleAllLayers;
    }

    SelectionAction alternateSelectionAction() const
    {
        return m_selectionActionAlternate;
    }

    virtual void setAlternateSelectionAction(SelectionAction action)
    {
        m_selectionActionAlternate = action;
    }

    void activateAlternateAction(KisTool::AlternateAction action) override
    {
        Q_UNUSED(action);
        BaseClass::activatePrimaryAction();
    }

    void deactivateAlternateAction(KisTool::AlternateAction action) override
    {
        Q_UNUSED(action);
        BaseClass::deactivatePrimaryAction();
    }

    void beginAlternateAction(KoPointerEvent *event,
                              KisTool::AlternateAction action) override
    {
        Q_UNUSED(action);
        beginPrimaryAction(event);
    }

    void continueAlternateAction(KoPointerEvent *event,
                                 KisTool::AlternateAction action) override
    {
        Q_UNUSED(action);
        continuePrimaryAction(event);
    }

    void endAlternateAction(KoPointerEvent *event,
                            KisTool::AlternateAction action) override
    {
        Q_UNUSED(action);
        endPrimaryAction(event);
    }

    KisNodeSP locateSelectionMaskUnderCursor(const PkPointF &pos, Qt::KeyboardModifiers modifiers) {
        if (modifiers != Qt::NoModifier) return 0;

        KisSelectionSP selection = KisSelectionUtils::activeSelectionForNode(
            this->currentImage().toStrongRef(), this->currentNode());
        if (selection &&
            selection->outlineCacheValid()) {

            const auto *converter = dynamic_cast<const KisCoordinatesConverter *>(
                this->canvas()->viewConverter());
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(converter, 0);
            const qreal handleRadius =
                qreal(this->handleRadius()) / converter->effectiveZoom();
            PkPainterPath samplePath;
            samplePath.addEllipse(pos, handleRadius, handleRadius);

            const PkPainterPath selectionPath = selection->outlineCache();

            if (selectionPath.intersects(samplePath) && !selectionPath.contains(samplePath)) {
                KisNodeSP parent = selection->parentNode();
                if (parent && parent->isEditable()) {
                    return parent;
                }
            }
        }

        return 0;
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        const Qt::Key key = event->key() == Qt::Key_Meta &&
                event->modifiers().testFlag(Qt::ShiftModifier)
            ? Qt::Key_Alt : static_cast<Qt::Key>(event->key());
        // Assume all the modifiers were unpressed...
        m_currentModifiers = Qt::NoModifier;
        // ...and add those which are right now
        if (key == Qt::Key_Control || event->modifiers().testFlag(Qt::ControlModifier)) {
            m_currentModifiers.setFlag(Qt::ControlModifier);
        }
        if (key == Qt::Key_Shift || event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_currentModifiers.setFlag(Qt::ShiftModifier);
        }
        if (key == Qt::Key_Alt || event->modifiers().testFlag(Qt::AltModifier)) {
            m_currentModifiers.setFlag(Qt::AltModifier);
        }
        
        // Avoid changing the selection mode and cursor if the user is interacting
        if (isSelecting()) {
            BaseClass::keyPressEvent(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
        this->resetCursorStyle();
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        const Qt::Key key = event->key() == Qt::Key_Meta &&
                event->modifiers().testFlag(Qt::ShiftModifier)
            ? Qt::Key_Alt : static_cast<Qt::Key>(event->key());
        // Assume all the modifiers were pressed...
        m_currentModifiers = Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier;
        // ...and remove those which aren't right now
        if (key == Qt::Key_Control || !event->modifiers().testFlag(Qt::ControlModifier)) {
            m_currentModifiers.setFlag(Qt::ControlModifier, false);
        }
        if (key == Qt::Key_Shift || !event->modifiers().testFlag(Qt::ShiftModifier)) {
            m_currentModifiers.setFlag(Qt::ShiftModifier, false);
        }
        if (key == Qt::Key_Alt || !event->modifiers().testFlag(Qt::AltModifier)) {
            m_currentModifiers.setFlag(Qt::AltModifier, false);
        }

        // Avoid changing the selection mode and cursor if the user is interacting
        if (isSelecting()) {
            BaseClass::keyReleaseEvent(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
        if (m_currentModifiers == Qt::NoModifier) {
            KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, m_currentModifiers);
            if (selectionMask) {
                this->useCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursor());
            } else {
                this->resetCursorStyle();
            }
        } else {
            this->resetCursorStyle();
        }
    }

    void mouseMoveEvent(KoPointerEvent *event) override
    {
        m_currentPos = this->convertToPixelCoord(event->point);

        if (isSelecting()) {
            BaseClass::mouseMoveEvent(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, event->modifiers());
        if (selectionMask) {
            this->useCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursor());
        } else {
            setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
            this->resetCursorStyle();
        }
    }

    void beginPrimaryAction(KoPointerEvent *event) override
    {
        if (isSelecting()) {
            BaseClass::beginPrimaryAction(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        const PkPointF pos = this->convertToPixelCoord(event->point);
        KisNodeSP selectionMask = locateSelectionMaskUnderCursor(pos, event->modifiers());
        if (selectionMask) {
            if (this->beginMoveSelectionInteraction()) {
                KisStrokeStrategy *strategy = new MoveStrokeStrategy({selectionMask}, this->image().data(), this->image().data());
                m_moveStrokeId = this->image()->startStroke(strategy);
                m_dragStartPos = pos;
                m_didMove = true;
                return;
            }
        }

        m_didMove = false;
        BaseClass::beginPrimaryAction(event);
    }

    void continuePrimaryAction(KoPointerEvent *event) override
    {
        if (isMovingSelection()) {
            const PkPointF pos = this->convertToPixelCoord(event->point);
            const PkPoint offset((pos - m_dragStartPos).toPoint());

            this->image()->addJob(m_moveStrokeId, new MoveStrokeStrategy::Data(offset));
            return;
        }

        BaseClass::continuePrimaryAction(event);
    }

    void endPrimaryAction(KoPointerEvent *event) override
    {
        if (isMovingSelection()) {
            this->image()->endStroke(m_moveStrokeId);
            m_moveStrokeId = nullptr;
            this->endMoveSelectionInteraction();
            return;
        }

        BaseClass::endPrimaryAction(event);
    }

    bool selectionDidMove() const
    {
        return m_didMove;
    }

    KisPopupWidgetInterface* popupWidget() override
    {
        if (isSelecting()) {
            return BaseClass::popupWidget();
        }
        return nullptr;
    }

    bool beginMoveSelectionInteraction() {
        if (m_currentInteraction != Interaction_None) {
            return false;
        }
        m_currentInteraction = Interaction_MoveSelection;
        return true;
    }

    bool endMoveSelectionInteraction() {
        if (!isMovingSelection()) {
            return false;
        }
        m_currentInteraction = Interaction_None;
        updateCursorDelayed();
        return true;
    }

    bool beginSelectInteraction() {
        if (m_currentInteraction != Interaction_None) {
            return false;
        }
        m_currentInteraction = Interaction_Select;
        return true;
    }

    bool endSelectInteraction() {
        if (!isSelecting()) {
            return false;
        }
        m_currentInteraction = Interaction_None;
        updateCursorDelayed();
        return true;
    }

    bool isMovingSelection() const {
        return m_currentInteraction == Interaction_MoveSelection;
    }

    bool isSelecting() const {
        return m_currentInteraction == Interaction_Select;
    }

    void updateCursorDelayed() {
        setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
        QTimer::singleShot(100, Qt::CoarseTimer,
            this,
            [this]()
            {
                KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, m_currentModifiers);
                if (selectionMask) {
                    this->useCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursor());
                } else {
                    this->resetCursorStyle();
                }
            }
        );
    }

protected:
    using BaseClass::canvas;

    void initializeSelectionState()
    {
        QObject::connect(&m_widgetHelper,
                         &KisSelectionToolConfigWidgetHelper::selectionActionChanged,
                         this,
                         [this]() { this->resetCursorStyle(); });
    }

    KisSelectionToolConfigWidgetHelper m_widgetHelper;
    SelectionAction m_selectionActionAlternate;

    virtual bool isPixelOnly() const {
        return false;
    }

    virtual bool usesColorLabels() const {
        return false;
    }

private:
    enum Interaction
    {
        Interaction_None,
        Interaction_Select,
        Interaction_MoveSelection
    };

    Interaction m_currentInteraction{Interaction_None};

    Qt::KeyboardModifiers m_currentModifiers;

    PkPointF m_dragStartPos;
    PkPointF m_currentPos;
    KisStrokeId m_moveStrokeId;
    bool m_didMove = false;

    KisQtConnectionsStore m_modeConnections;
};

struct FakeBaseTool : KisTool
{
    FakeBaseTool(KoCanvasBase* canvas)
        : KisTool(canvas, QCursor())
    {
    }

    FakeBaseTool(KoCanvasBase *canvas, const PkString &toolName)
        : KisTool(canvas, QCursor())
    {
        Q_UNUSED(toolName);
    }

    FakeBaseTool(KoCanvasBase* canvas, const QCursor &cursor)
        : KisTool(canvas, cursor)
    {
    }
};


typedef KisToolSelectBase<FakeBaseTool> KisToolSelect;


#endif // KISTOOLSELECTBASE_H
