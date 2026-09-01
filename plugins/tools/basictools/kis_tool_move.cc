/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 1999 Michael Koch <koch@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2016 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_move.h"
#include "kis_basic_tools_string_utils.h"

#include <PkPoint.h>

#include <KSharedConfig>
#include <KoCanvasBase.h>
#include <KoPointerEvent.h>

#include <KisCanvasToolServices.h>
#include "kis_selection.h"
#include "KisCanvasFeedback.h"
#include "KisCanvasInvalidation.h"
#include "kis_image.h"

#include "kis_paint_layer.h"
#include "strokes/move_stroke_strategy.h"
#include "strokes/move_selection_stroke_strategy.h"
#include "kis_resources_snapshot.h"
#include "krita_utils.h"
#include <KoCanvasResourceProvider.h>

#include "KisAnimAutoKey.h"
#include <boost/operators.hpp>
#include "KisMoveBoundsCalculationJob.h"
#include <KisOptimizedBrushOutline.h>


struct KisToolMoveState : KisToolChangesTrackerData, boost::equality_comparable<KisToolMoveState>
{
    KisToolMoveState(PkPoint _accumulatedOffset) : accumulatedOffset(_accumulatedOffset) {}
    KisToolChangesTrackerData* clone() const override { return new KisToolMoveState(*this); }

    bool operator ==(const KisToolMoveState &rhs) {
        return accumulatedOffset == rhs.accumulatedOffset;
    }

    PkPoint accumulatedOffset;
};


KisToolMove::KisToolMove(KoCanvasBase *canvas)
    : KisTool(canvas, dynamic_cast<KisCanvasToolServices *>(canvas)->toolMoveCursor())
    , m_updateCursorCompressor(100, KisSignalCompressor::FIRST_ACTIVE)
{
    setObjectName("tool_move");

    m_updateCursorConnection =
        PkObject::connect(&m_updateCursorCompressor, &KisSignalCompressor::timeout,
                          &m_updateCursorCompressor, [this]() { resetCursorStyle(); });
}

KisToolMove::~KisToolMove()
{
    PkObject::disconnect(m_updateCursorConnection);
    endStroke();
}

void KisToolMove::resetCursorStyle()
{
    if (!isActive()) return;

    bool canMove = true;

    if (m_strokeId && m_currentlyUsingSelection) {
        /// noop; whatever the cursor position, we always show move
        /// cursor, because we don't use 'layer under cursor' mode
        /// for moving selections
    } else if (m_strokeId && !m_currentlyUsingSelection) {
        /// we cannot pick layer's pixel data while the stroke is running,
        /// because it may run in lodN mode; therefore, we delegate this
        /// work to the stroke itself
        if (m_currentMode != MoveSelectedLayer &&
            (m_handlesRect.isEmpty() ||
             !m_handlesRect.translated(currentOffset()).contains(m_lastCursorPos))) {

            image()->addJob(m_strokeId, new MoveStrokeStrategy::PickLayerData(m_lastCursorPos));
            return;
        }
    } else {
        KisResourcesSnapshotSP resources =
            new KisResourcesSnapshot(this->image(), currentNode(), canvas()->resourceManager()->canvasResourcesInterface());
        KisSelectionSP selection = resources->activeSelection();

        KisPaintLayerSP paintLayer =
            dynamic_cast<KisPaintLayer*>(this->currentNode().data());

        const bool canUseSelectionMode =
                paintLayer && selection &&
                !selection->selectedRect().isEmpty() &&
                !selection->selectedExactRect().isEmpty();

        if (canUseSelectionMode) {
            canMove = (m_currentMode == MoveSelectedLayer ? paintLayer->isEditable() : true);
        } else {
            KisNodeSelectionRecipe nodeSelection =
                    KisNodeSelectionRecipe(
                        this->selectedNodes(),
                        (KisNodeSelectionRecipe::SelectionMode)moveToolMode(),
                        m_lastCursorPos);

            if (nodeSelection.selectNodesToProcess().isEmpty()) {
                canMove = false;
            }
        }
    }

    if (canMove) {
        KisTool::resetCursorStyle();
    } else {
       useCursor(Qt::ForbiddenCursor);
    }
}

bool KisToolMove::startStrokeImpl(MoveToolMode mode, const PkPoint *pos)
{
    KisNodeSP node;
    KisImageSP image = this->image();

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image, currentNode(), canvas()->resourceManager()->canvasResourcesInterface());
    KisSelectionSP selection = resources->activeSelection();

    KisPaintLayerSP paintLayer =
        dynamic_cast<KisPaintLayer*>(this->currentNode().data());



    const bool canUseSelectionMode =
            paintLayer && selection &&
            !selection->selectedRect().isEmpty() &&
            !selection->selectedExactRect().isEmpty();

    if (pos) {
        // finish stroke by clicking outside image bounds
        if (m_strokeId && !image->bounds().contains(*pos)) {
            endStroke();
            return false;
        }

        // restart stroke when the mode has changed or the user tried to
        // pick another layer in "layer under cursor" mode.
        if (m_strokeId &&
                (m_currentMode != mode ||
                 m_currentlyUsingSelection != canUseSelectionMode ||
                 (!m_currentlyUsingSelection &&
                  mode != MoveSelectedLayer &&
                  !m_handlesRect.translated(currentOffset()).contains(*pos)))) {

            endStroke();
        }
    }

    if (m_strokeId) return true;


    if (canUseSelectionMode && !nodeEditable()) {
        // if there is a selection, it would only use the current layer anyway
        // if the current layer is not editable, don't continue
        return false;
    }

    KisNodeList nodes;

    KisStrokeStrategy *strategy;

    bool isMoveSelection = false;
    if (canUseSelectionMode) {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(selection, false);

        MoveSelectionStrokeStrategy *moveStrategy =
            new MoveSelectionStrokeStrategy(paintLayer,
                                            selection,
                                            image.data(),
                                            image.data());

        PkObject::connect(moveStrategy, &MoveSelectionStrokeStrategy::sigHandlesRectCalculated,
                          this, &KisToolMove::slotHandlesRectCalculated);
        PkObject::connect(moveStrategy, &MoveSelectionStrokeStrategy::sigStrokeStartedEmpty,
                          this, &KisToolMove::slotStrokeStartedEmpty);

        strategy = moveStrategy;
        isMoveSelection = true;
        nodes = {paintLayer};

    } else {
        KisNodeSelectionRecipe nodeSelection =
            pos ?
                KisNodeSelectionRecipe(
                        this->selectedNodes(),
                        (KisNodeSelectionRecipe::SelectionMode)mode,
                        *pos) :
                KisNodeSelectionRecipe(this->selectedNodes());


        MoveStrokeStrategy *moveStrategy =
            new MoveStrokeStrategy(nodeSelection, image.data(), image.data());
        PkObject::connect(moveStrategy, &MoveStrokeStrategy::sigHandlesRectCalculated,
                          this, &KisToolMove::slotHandlesRectCalculated);
        PkObject::connect(moveStrategy, &MoveStrokeStrategy::sigStrokeStartedEmpty,
                          this, &KisToolMove::slotStrokeStartedEmpty);
        PkObject::connect(moveStrategy, &MoveStrokeStrategy::sigLayersPicked,
                          this, &KisToolMove::slotStrokePickedLayers);

        strategy = moveStrategy;
        nodes = nodeSelection.selectedNodes;
    }

    {
        KConfigGroup group = KSharedConfig::openConfig()->group(toolId());
        const bool forceLodMode = group.readEntry("forceLodMode", false);
        strategy->setForceLodModeIfPossible(forceLodMode);
    }

    // disable outline feedback until the stroke calculates
    // correct bounding rect
    m_handlesRect = PkRect();
    m_strokeId = image->startStroke(strategy);
    m_currentlyProcessingNodes = nodes;
    m_currentlyUsingSelection = isMoveSelection;
    m_currentMode = mode;
    m_accumulatedOffset = PkPoint();

    if (!isMoveSelection) {
        m_asyncUpdateHelper.startUpdateStream(image.data(), m_strokeId);
    }

    KIS_SAFE_ASSERT_RECOVER(m_changesTracker.isEmpty()) {
        m_changesTracker.reset();
    }
    commitChanges();

    return true;
}

PkPoint KisToolMove::currentOffset() const
{
    return m_accumulatedOffset + m_dragPos - m_dragStart;
}

void KisToolMove::notifyGuiAfterMove(bool showFloatingMessage)
{
    if (m_handlesRect.isEmpty()) return;

    const PkPoint currentTopLeft = m_handlesRect.topLeft() + currentOffset();

    if (m_showCoordinates && showFloatingMessage) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(
            PkString("X: %1 px, Y: %2 px")
                .arg(KisBasicToolsString::number(currentTopLeft.x()))
                .arg(KisBasicToolsString::number(currentTopLeft.y())),
            {}, 1000, KisCanvasFeedback::Priority::High);
    }
}

bool KisToolMove::tryEndPreviousStroke(const KisNodeList &nodes)
{
    if (!m_strokeId) return false;

    bool strokeEnded = false;

    if (!KritaUtils::compareListsUnordered(nodes, m_currentlyProcessingNodes)) {
        endStroke();
        strokeEnded = true;
    }

    return strokeEnded;
}

void KisToolMove::commitChanges()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_strokeId);

    PkSharedPointer<KisToolMoveState> newState(new KisToolMoveState(m_accumulatedOffset));
    KisToolMoveState *lastState = dynamic_cast<KisToolMoveState*>(m_changesTracker.lastState().data());
    if (lastState && *lastState == *newState) return;

    m_changesTracker.commitConfig(newState);
}

void KisToolMove::slotHandlesRectCalculated(const PkRect &handlesRect)
{
    m_handlesRect = handlesRect;
    notifyGuiAfterMove(false);
}

void KisToolMove::slotStrokeStartedEmpty()
{
    /**
     * Notify that move-selection stroke ended unexpectedly
     */
    if (m_currentlyUsingSelection) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_NOOP(feedback);
        if (feedback) {
            feedback->showFloatingMessage(
                PkString("Selected area has no pixels"),
                {}, 1000, KisCanvasFeedback::Priority::High);
        }
    }

    /**
     * Since the choice of nodes for the operation happens in the
     * stroke itself, it may happen that there are no nodes at all.
     * In such a case, we should just cancel already started stroke.
     */
    cancelStroke();
}

void KisToolMove::slotStrokePickedLayers(const KisNodeList &nodes)
{
    if (nodes.isEmpty()) {
        useCursor(Qt::ForbiddenCursor);
    } else {
        KisTool::resetCursorStyle();
    }
}

void KisToolMove::moveDiscrete(MoveDirection direction, bool big)
{
    if (mode() == KisTool::PAINT_MODE) return;  // Don't interact with dragging
    if (!currentNode()) return;
    if (!image()) return;
    if (!currentNode()->isEditable()) return; // Don't move invisible nodes

    if (startStrokeImpl(MoveSelectedLayer, nullptr)) {
        setMode(KisTool::PAINT_MODE);
    }

    // Larger movement if "shift" key is pressed.
    qreal scale = big ? 10.0 : 1.0;
    qreal moveStep = 1 * scale;

    const PkPoint offset =
        direction == Up   ? PkPoint( 0, -moveStep) :
        direction == Down ? PkPoint( 0,  moveStep) :
        direction == Left ? PkPoint(-moveStep,  0) :
        PkPoint( moveStep,  0) ;

    m_accumulatedOffset += offset;
    image()->addJob(m_strokeId, new MoveStrokeStrategy::Data(m_accumulatedOffset));

    notifyGuiAfterMove();
    commitChanges();
    setMode(KisTool::HOVER_MODE);
}

void KisToolMove::activate(const PkSet<KoShape*> &shapes)
{
    KisTool::activate(shapes);

    m_canvasConnections.addUniqueConnection(
        canvas()->resourceManager(),
        &KoCanvasResourceProvider::canvasResourceChanged,
        this,
        &KisToolMove::slotCanvasResourceChanged);

    m_canvasConnections.addUniqueConnection(
        &m_changesTracker,
        &KisToolChangesTracker::sigConfigChanged,
        this,
        &KisToolMove::slotTrackerChangedConfig);


    slotNodeChanged(this->selectedNodes());
}



void KisToolMove::paint(PkPainter& gc, const KoViewConverter &converter)
{
    (void)converter;

    if (m_strokeId && !m_handlesRect.isEmpty() && !m_currentlyUsingSelection) {
        PkPainterPath handles;
        handles.addRect(m_handlesRect.translated(currentOffset()));

        PkPainterPath path = pixelToView(handles);
        paintToolOutline(&gc, path);
    }
}

void KisToolMove::deactivate()
{
    m_canvasConnections.clear();

    endStroke();
    KisTool::deactivate();
}

void KisToolMove::requestStrokeEnd()
{
    endStroke();
}

void KisToolMove::requestStrokeCancellation()
{
    cancelStroke();
}

void KisToolMove::requestUndoDuringStroke()
{
    if (!m_strokeId) return;

    if (!m_changesTracker.canUndo()) {
        cancelStroke();
    } else {
        m_changesTracker.requestUndo();
    }
}

void KisToolMove::requestRedoDuringStroke()
{
    if (!m_strokeId) return;

    if (m_changesTracker.canRedo()) {
        m_changesTracker.requestRedo();
    }
}

void KisToolMove::beginPrimaryAction(KoPointerEvent *event)
{
    startAction(event, moveToolMode());
}

void KisToolMove::continuePrimaryAction(KoPointerEvent *event)
{
    continueAction(event);
}

void KisToolMove::endPrimaryAction(KoPointerEvent *event)
{
    endAction(event);
}

void KisToolMove::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    // Ctrl+Right click toggles between moving current layer and moving layer w/ content
    if (action == SampleFgNode || action == SampleBgImage) {
        MoveToolMode mode = moveToolMode();

        if (mode == MoveSelectedLayer) {
            mode = MoveFirstLayer;
        } else if (mode == MoveFirstLayer) {
            mode = MoveSelectedLayer;
        }

        startAction(event, mode);
    } else {
        startAction(event, MoveGroup);
    }
}

void KisToolMove::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    (void)action;
    continueAction(event);
}

void KisToolMove::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    (void)action;
    endAction(event);
}

void KisToolMove::mouseMoveEvent(KoPointerEvent *event)
{
    m_lastCursorPos = convertToPixelCoord(event).toPoint();
    KisTool::mouseMoveEvent(event);

    if (moveToolMode() != MoveSelectedLayer ||
            (m_strokeId && m_currentMode != MoveSelectedLayer)) {

        m_updateCursorCompressor.start();
    }
}

void KisToolMove::startAction(KoPointerEvent *event, MoveToolMode mode)
{
    PkPoint pos = convertToPixelCoordAndSnap(event).toPoint();
    m_dragStart = pos;
    m_dragPos = pos;

    if (startStrokeImpl(mode, &pos)) {
        setMode(KisTool::PAINT_MODE);

        if (m_currentlyUsingSelection) {
            KisImageSP image = currentImage();
            image->addJob(m_strokeId,
                          new MoveSelectionStrokeStrategy::ShowSelectionData(false));
        }

    } else {
        event->ignore();
        m_dragPos = PkPoint();
        m_dragStart = PkPoint();
    }
    invalidateCanvas();
}

void KisToolMove::continueAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    if (!m_strokeId) return;

    PkPoint pos = convertToPixelCoordAndSnap(event).toPoint();
    pos = applyModifiers(event->modifiers(), pos);
    m_dragPos = pos;

    drag(pos);
    notifyGuiAfterMove();

    invalidateCanvas();
}

void KisToolMove::endAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);
    if (!m_strokeId) return;

    PkPoint pos = convertToPixelCoordAndSnap(event).toPoint();
    pos = applyModifiers(event->modifiers(), pos);
    drag(pos);

    m_accumulatedOffset += pos - m_dragStart;
    m_dragStart = PkPoint();
    m_dragPos = PkPoint();
    commitChanges();

    if (m_currentlyUsingSelection) {
        KisImageSP image = currentImage();
        image->addJob(m_strokeId,
                      new MoveSelectionStrokeStrategy::ShowSelectionData(true));
    }

    notifyGuiAfterMove();

    invalidateCanvas();
}

void KisToolMove::drag(const PkPoint& newPos)
{
    KisImageSP image = currentImage();

    PkPoint offset = m_accumulatedOffset + newPos - m_dragStart;

    image->addJob(m_strokeId,
                  new MoveStrokeStrategy::Data(offset));
}

void KisToolMove::endStroke()
{
    if (!m_strokeId) return;

    if (m_asyncUpdateHelper.isActive()) {
        m_asyncUpdateHelper.endUpdateStream();
    }

    KisImageSP image = currentImage();
    image->endStroke(m_strokeId);
    m_strokeId.clear();
    m_changesTracker.reset();
    m_currentlyProcessingNodes.clear();
    m_currentlyUsingSelection = false;
    m_currentMode = MoveSelectedLayer;
    m_accumulatedOffset = PkPoint();
    invalidateCanvas();
}

void KisToolMove::slotTrackerChangedConfig(KisToolChangesTrackerDataSP state)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_strokeId);

    KisToolMoveState *newState = dynamic_cast<KisToolMoveState*>(state.data());
    KIS_SAFE_ASSERT_RECOVER_RETURN(newState);

    if (mode() == KisTool::PAINT_MODE) return;  // Don't interact with dragging
    m_accumulatedOffset = newState->accumulatedOffset;
    image()->addJob(m_strokeId, new MoveStrokeStrategy::Data(m_accumulatedOffset));
    notifyGuiAfterMove();
}

void KisToolMove::slotMoveDiscreteLeft()
{
    moveDiscrete(MoveDirection::Left, false);
}

void KisToolMove::slotMoveDiscreteRight()
{
    moveDiscrete(MoveDirection::Right, false);
}

void KisToolMove::slotMoveDiscreteUp()
{
    moveDiscrete(MoveDirection::Up, false);
}

void KisToolMove::slotMoveDiscreteDown()
{
    moveDiscrete(MoveDirection::Down, false);
}

void KisToolMove::slotMoveDiscreteLeftMore()
{
    moveDiscrete(MoveDirection::Left, true);
}

void KisToolMove::slotMoveDiscreteRightMore()
{
    moveDiscrete(MoveDirection::Right, true);
}

void KisToolMove::slotMoveDiscreteUpMore()
{
    moveDiscrete(MoveDirection::Up, true);
}

void KisToolMove::slotMoveDiscreteDownMore()
{
    moveDiscrete(MoveDirection::Down, true);
}

void KisToolMove::cancelStroke()
{
    if (!m_strokeId) return;

    if (m_asyncUpdateHelper.isActive()) {
        m_asyncUpdateHelper.cancelUpdateStream();
    }

    KisImageSP image = currentImage();
    image->cancelStroke(m_strokeId);
    m_strokeId.clear();
    m_changesTracker.reset();
    m_currentlyProcessingNodes.clear();
    m_currentlyUsingSelection = false;
    m_currentMode = MoveSelectedLayer;
    m_accumulatedOffset = PkPoint();
    notifyGuiAfterMove();
    invalidateCanvas();
}

KisToolMove::MoveToolMode KisToolMove::moveToolMode() const
{
    return MoveSelectedLayer;
}

PkPoint KisToolMove::applyModifiers(Qt::KeyboardModifiers modifiers, PkPoint pos)
{
    PkPoint move = pos - m_dragStart;

    // Snap to axis
    if (modifiers & Qt::ShiftModifier) {
        move = snapToClosestAxis(move);
    }

    // "Precision mode" - scale down movement by 1/5
    if (modifiers & Qt::AltModifier) {
        const qreal SCALE_FACTOR = .2;
        move = SCALE_FACTOR * move;
    }

    return m_dragStart + move;
}

void KisToolMove::requestHandlesRectUpdate()
{
    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), canvas()->resourceManager()->canvasResourcesInterface());
    KisSelectionSP selection = resources->activeSelection();

    KisMoveBoundsCalculationJob *job = new KisMoveBoundsCalculationJob(this->selectedNodes(),
                                                                       selection, this);
    PkObject::connect(job, &KisMoveBoundsCalculationJob::sigCalculationFinished,
                      this, &KisToolMove::slotHandlesRectCalculated);

    KisImageSP image = this->image();
    image->addSpontaneousJob(job);

    notifyGuiAfterMove(false);
}

void KisToolMove::slotNodeChanged(const KisNodeList &nodes)
{
    if (m_strokeId && !tryEndPreviousStroke(nodes)) {
        return;
    }
    requestHandlesRectUpdate();
}

void KisToolMove::slotCanvasResourceChanged(int key, const PkVariant &)
{
    if (key == KoCanvasResource::CurrentKritaSelectedNodesRevision) {
        slotNodeChanged(selectedNodes());
    } else if (key == KoCanvasResource::CurrentKritaSelectionRevision) {
        slotSelectionChanged();
    }
}

void KisToolMove::slotSelectionChanged()
{
    if (m_strokeId) return;
    requestHandlesRectUpdate();
}

void KisToolMove::invalidateCanvas()
{
    KisCanvasInvalidation *invalidation =
        dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();
}

void KisToolMove::setShowCoordinates(bool value)
{
    m_showCoordinates = value;
}

PkList<PkString> KisToolMove::moveActionIds() const
{
    return {
        PkString("movetool-move-up"),
        PkString("movetool-move-down"),
        PkString("movetool-move-left"),
        PkString("movetool-move-right"),
        PkString("movetool-move-up-more"),
        PkString("movetool-move-down-more"),
        PkString("movetool-move-left-more"),
        PkString("movetool-move-right-more"),
        PkString("movetool-show-coordinates")
    };
}

bool KisToolMove::triggerMoveAction(const PkString &id, bool checked)
{
    if (id == PkString("movetool-move-up")) { slotMoveDiscreteUp(); return true; }
    if (id == PkString("movetool-move-down")) { slotMoveDiscreteDown(); return true; }
    if (id == PkString("movetool-move-left")) { slotMoveDiscreteLeft(); return true; }
    if (id == PkString("movetool-move-right")) { slotMoveDiscreteRight(); return true; }
    if (id == PkString("movetool-move-up-more")) { slotMoveDiscreteUpMore(); return true; }
    if (id == PkString("movetool-move-down-more")) { slotMoveDiscreteDownMore(); return true; }
    if (id == PkString("movetool-move-left-more")) { slotMoveDiscreteLeftMore(); return true; }
    if (id == PkString("movetool-move-right-more")) { slotMoveDiscreteRightMore(); return true; }
    if (id == PkString("movetool-show-coordinates")) { setShowCoordinates(checked); return true; }
    return false;
}

void KisToolMove::moveToolModeChanged()
{
    activateSignal<>(this, PkMemberFnKey::from(&KisToolMove::moveToolModeChanged));
}
