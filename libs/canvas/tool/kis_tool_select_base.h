/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISTOOLSELECTBASE_H
#define KISTOOLSELECTBASE_H

#include <PkList.h>
#include <PkPainterPath.h>
#include <PkSet.h>
#include <PkString.h>
#include <PkTimer.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

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

    KisToolSelectBase(KoCanvasBase *canvas, KisCanvasCursorToken cursor, const PkString &toolName)
        : BaseClass(canvas, cursor)
        , m_widgetHelper(toolName)
        , m_selectionActionAlternate(SELECTION_DEFAULT)
    {
        KisSelectionModifierMapper::instance();
        initializeSelectionState();
    }

    template<typename DelegateTool>
    KisToolSelectBase(KoCanvasBase *canvas,
                      KisCanvasCursorToken cursor,
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

        auto *services = dynamic_cast<KisCanvasToolServices *>(this->canvas());
        KIS_ASSERT_RECOVER_RETURN(services);
        services->toolSetActionCallback(
            "selection_tool_mode_replace", this, this->callLifetime(),
            [this] { m_widgetHelper.slotReplaceModeRequested(); }, true);
        services->toolSetActionCallback(
            "selection_tool_mode_add", this, this->callLifetime(),
            [this] { m_widgetHelper.slotAddModeRequested(); }, true);
        services->toolSetActionCallback(
            "selection_tool_mode_subtract", this, this->callLifetime(),
            [this] { m_widgetHelper.slotSubtractModeRequested(); }, true);
        services->toolSetActionCallback(
            "selection_tool_mode_intersect", this, this->callLifetime(),
            [this] { m_widgetHelper.slotIntersectModeRequested(); }, true);

    }

    void deactivate() override
    {
        m_widgetHelper.slotToolActivatedChanged(false);
        auto *services = dynamic_cast<KisCanvasToolServices *>(this->canvas());
        if (services) services->toolClearActionCallbacks(this);
        BaseClass::deactivate();
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

    KisNodeSP locateSelectionMaskUnderCursor(const PkPointF &pos, Pk::KeyboardModifiers modifiers) {
        if (modifiers != Pk::NoModifier) return 0;

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

    void pkKeyPressEvent(PkToolKeyEvent *event) override
    {
        const Pk::Key key = event->key() == Pk::Key_Meta &&
                event->modifiers().testFlag(Pk::ShiftModifier)
            ? Pk::Key_Alt : event->key();
        // Assume all the modifiers were unpressed...
        m_currentModifiers = Pk::NoModifier;
        // ...and add those which are right now
        if (key == Pk::Key_Control || event->modifiers().testFlag(Pk::ControlModifier)) {
            m_currentModifiers.setFlag(Pk::ControlModifier);
        }
        if (key == Pk::Key_Shift || event->modifiers().testFlag(Pk::ShiftModifier)) {
            m_currentModifiers.setFlag(Pk::ShiftModifier);
        }
        if (key == Pk::Key_Alt || event->modifiers().testFlag(Pk::AltModifier)) {
            m_currentModifiers.setFlag(Pk::AltModifier);
        }
        
        // Avoid changing the selection mode and cursor if the user is interacting
        if (isSelecting()) {
            BaseClass::pkKeyPressEvent(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
        this->resetCursorStyle();
    }

    void pkKeyReleaseEvent(PkToolKeyEvent *event) override
    {
        const Pk::Key key = event->key() == Pk::Key_Meta &&
                event->modifiers().testFlag(Pk::ShiftModifier)
            ? Pk::Key_Alt : event->key();
        // Assume all the modifiers were pressed...
        m_currentModifiers = Pk::ControlModifier | Pk::ShiftModifier | Pk::AltModifier;
        // ...and remove those which aren't right now
        if (key == Pk::Key_Control || !event->modifiers().testFlag(Pk::ControlModifier)) {
            m_currentModifiers.setFlag(Pk::ControlModifier, false);
        }
        if (key == Pk::Key_Shift || !event->modifiers().testFlag(Pk::ShiftModifier)) {
            m_currentModifiers.setFlag(Pk::ShiftModifier, false);
        }
        if (key == Pk::Key_Alt || !event->modifiers().testFlag(Pk::AltModifier)) {
            m_currentModifiers.setFlag(Pk::AltModifier, false);
        }

        // Avoid changing the selection mode and cursor if the user is interacting
        if (isSelecting()) {
            BaseClass::pkKeyReleaseEvent(event);
            return;
        }
        if (isMovingSelection()) {
            return;
        }

        setAlternateSelectionAction(KisSelectionModifierMapper::map(m_currentModifiers));
        if (m_currentModifiers == Pk::NoModifier) {
            KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, m_currentModifiers);
            if (selectionMask) {
                this->applyCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursorToken());
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

        const Pk::KeyboardModifiers modifiers(
            PkFlag(static_cast<int>(event->modifiers())));
        KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, modifiers);
        if (selectionMask) {
            this->applyCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursorToken());
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
        const Pk::KeyboardModifiers modifiers(
            PkFlag(static_cast<int>(event->modifiers())));
        KisNodeSP selectionMask = locateSelectionMaskUnderCursor(pos, modifiers);
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
        auto timer = std::make_unique<PkTimer>();
        PkTimer *timerIdentity = timer.get();
        m_cursorUpdateTimers.push_back(std::move(timer));
        timerIdentity->start(
            std::chrono::milliseconds(100),
            [this, timerIdentity]
            {
                KisNodeSP selectionMask = locateSelectionMaskUnderCursor(m_currentPos, m_currentModifiers);
                if (selectionMask) {
                    this->applyCursor(dynamic_cast<KisCanvasToolServices*>(this->canvas())->toolMoveSelectionCursorToken());
                } else {
                    this->resetCursorStyle();
                }
                const auto timer = std::find_if(
                    m_cursorUpdateTimers.begin(), m_cursorUpdateTimers.end(),
                    [timerIdentity](const auto &candidate) {
                        return candidate.get() == timerIdentity;
                    });
                if (timer != m_cursorUpdateTimers.end()) {
                    m_cursorUpdateTimers.erase(timer);
                }
            }, true);
    }

protected:
    using BaseClass::canvas;

    void initializeSelectionState()
    {
        PkObject::connect(&m_widgetHelper,
                          &KisSelectionToolConfigWidgetHelper::selectionActionChanged,
                          this,
                          [this](SelectionAction) { this->resetCursorStyle(); });
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

    Pk::KeyboardModifiers m_currentModifiers;

    PkPointF m_dragStartPos;
    PkPointF m_currentPos;
    KisStrokeId m_moveStrokeId;
    bool m_didMove = false;

    std::vector<std::unique_ptr<PkTimer>> m_cursorUpdateTimers;
};

struct FakeBaseTool : KisTool
{
    FakeBaseTool(KoCanvasBase* canvas)
        : KisTool(canvas, {})
    {
    }

    FakeBaseTool(KoCanvasBase *canvas, const PkString &toolName)
        : KisTool(canvas, {})
    {
        Q_UNUSED(toolName);
    }

    FakeBaseTool(KoCanvasBase* canvas, KisCanvasCursorToken cursor)
        : KisTool(canvas, cursor)
    {
    }
};


typedef KisToolSelectBase<FakeBaseTool> KisToolSelect;


#endif // KISTOOLSELECTBASE_H
