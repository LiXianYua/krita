/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 1999 Michael Koch <koch@kde.org>
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_MOVE_H_
#define KIS_TOOL_MOVE_H_

#include <KisToolPaintFactoryBase.h>
#include <kis_types.h>
#include <kis_tool.h>
#include <flake/kis_node_shape.h>
#include <PkList.h>
#include <PkPainter.h>
#include <PkString.h>
#include <PkVariant.h>
#include "KisToolChangesTracker.h"
#include "kis_signal_compressor.h"
#include "kis_signal_auto_connection.h"
#include "KisAsynchronousStrokeUpdateHelper.h"

class KoCanvasBase;

class KisToolMove : public KisTool
{
public:
    KisToolMove(KoCanvasBase * canvas);
    ~KisToolMove() override;

    /**
     * @brief wantsAutoScroll
     * reimplemented from KoToolBase
     * there's an issue where autoscrolling with this tool never makes the
     * stroke end, so we return false here so that users don't get stuck with
     * the tool. See bug 362659
     * @return false
     */
    bool wantsAutoScroll() const override {
        return false;
    }

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

public:
    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;
    void requestUndoDuringStroke() override;
    void requestRedoDuringStroke() override;

protected:
    void resetCursorStyle() override;

public:
    enum MoveToolMode {
        MoveSelectedLayer,
        MoveFirstLayer,
        MoveGroup
    };

    enum MoveDirection {
        Up,
        Down,
        Left,
        Right
    };

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;

    void mouseMoveEvent(KoPointerEvent *event) override;

    void startAction(KoPointerEvent *event, MoveToolMode mode);
    void continueAction(KoPointerEvent *event);
    void endAction(KoPointerEvent *event);

    void paint(PkPainter& gc, const KoViewConverter &converter) override;

    void updateUIUnit(int newUnit);

    MoveToolMode moveToolMode() const;

    void setShowCoordinates(bool value);
    PkList<PkString> moveActionIds() const;
    bool triggerMoveAction(const PkString &id, bool checked = false);

public:
    void moveDiscrete(MoveDirection direction, bool big);

    void slotNodeChanged(const KisNodeList &nodes);
    void slotSelectionChanged();
    void commitChanges();

    void slotHandlesRectCalculated(const PkRect &handlesRect);
    void slotStrokeStartedEmpty();
    void slotStrokePickedLayers(const KisNodeList &nodes);

    void moveToolModeChanged();

private:
    void drag(const PkPoint& newPos);
    void cancelStroke();
    PkPoint applyModifiers(Qt::KeyboardModifiers modifiers, PkPoint pos);

    bool startStrokeImpl(MoveToolMode mode, const PkPoint *pos);

    PkPoint currentOffset() const;
    void notifyGuiAfterMove(bool showFloatingMessage = true);
    bool tryEndPreviousStroke(const KisNodeList &nodes);
    void requestHandlesRectUpdate();
    void invalidateCanvas();


private:
    void endStroke();
    void slotTrackerChangedConfig(KisToolChangesTrackerDataSP state);
    void slotCanvasResourceChanged(int key, const PkVariant &value);

    void slotMoveDiscreteLeft();
    void slotMoveDiscreteRight();
    void slotMoveDiscreteUp();
    void slotMoveDiscreteDown();
    void slotMoveDiscreteLeftMore();
    void slotMoveDiscreteRightMore();
    void slotMoveDiscreteUpMore();
    void slotMoveDiscreteDownMore();

private:

    PkPoint m_dragStart; ///< Point where current cursor dragging began
    PkPoint m_accumulatedOffset; ///< Total offset including multiple clicks, up/down/left/right keys, etc. added together

    KisStrokeId m_strokeId;

    KisNodeList m_currentlyProcessingNodes;
    bool m_currentlyUsingSelection {false};
    MoveToolMode m_currentMode {MoveSelectedLayer};

    int m_resolution {0};

    bool m_showCoordinates {false};

    PkPoint m_dragPos;
    PkRect m_handlesRect;

    KisToolChangesTracker m_changesTracker;

    PkPoint m_lastCursorPos;
    KisSignalCompressor m_updateCursorCompressor;
    PkConnection m_updateCursorConnection;
    KisSignalAutoConnectionsStore m_canvasConnections;

    KisAsynchronousStrokeUpdateHelper m_asyncUpdateHelper;
};


class KisToolMoveFactory : public KisToolPaintFactoryBase
{

public:
    KisToolMoveFactory()
            : KisToolPaintFactoryBase("KritaTransform/KisToolMove") {
        setToolTip(PkString("Move Tool"));
        setSection(ToolBoxSection::Transform);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setPriority(3);
        setShortcut(PkString("T"));
    }

    ~KisToolMoveFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolMove(canvas);
    }

};

#endif // KIS_TOOL_MOVE_H_
