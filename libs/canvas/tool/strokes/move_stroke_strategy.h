/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __MOVE_STROKE_STRATEGY_H
#define __MOVE_STROKE_STRATEGY_H

#include <PkElapsedTimer.h>
#include <PkHash.h>
#include <PkObject.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkScopedPointer.h>
#include <PkSet.h>
#include <PkSharedPointer.h>

#include <kritacanvas_export.h>
#include "kis_stroke_strategy_undo_command_based.h"
#include "kis_types.h"
#include "kis_lod_transform.h"
#include "KisAsynchronousStrokeUpdateHelper.h"
#include "KisNodeSelectionRecipe.h"
#include "kis_transaction.h"

#include <memory>
#include <unordered_map>

class KisUpdatesFacade;
class KisPostExecutionUndoAdapter;


class KRITACANVAS_EXPORT MoveStrokeStrategy : public PkObject, public KisStrokeStrategyUndoCommandBased
{
public:
    class KRITACANVAS_EXPORT Data : public KisStrokeJobData {
    public:
        Data(PkPoint _offset);
        KisStrokeJobData* createLodClone(int levelOfDetail) override;

        PkPoint offset;

    private:
        Data(const Data &rhs, int levelOfDetail);
    };

    class KRITACANVAS_EXPORT PickLayerData : public KisStrokeJobData {
    public:
        PickLayerData(PkPoint _pos);

        KisStrokeJobData* createLodClone(int levelOfDetail) override;

        PkPoint pos;

    private:
        PickLayerData(const PickLayerData &rhs, int levelOfDetail);
    };


    struct KRITACANVAS_EXPORT BarrierUpdateData : public KisAsynchronousStrokeUpdateHelper::UpdateData
    {
        BarrierUpdateData(bool forceUpdate);
        KisStrokeJobData* createLodClone(int levelOfDetail) override;
    protected:
        BarrierUpdateData(const BarrierUpdateData &rhs, int levelOfDetail);
    };

public:
    MoveStrokeStrategy(KisNodeSelectionRecipe nodeSelection,
                       KisUpdatesFacade *updatesFacade,
                       KisStrokeUndoFacade *undoFacade);

    MoveStrokeStrategy(KisNodeList nodes,
                       KisUpdatesFacade *updatesFacade,
                       KisStrokeUndoFacade *undoFacade);

    ~MoveStrokeStrategy() override;

    void initStrokeCallback() override;
    void finishStrokeCallback() override;
    void cancelStrokeCallback() override;
    void doStrokeCallback(KisStrokeJobData *data) override;

    KisStrokeStrategy* createLodClone(int levelOfDetail) override;

    void sigHandlesRectCalculated(const PkRect &handlesRect);
    void sigStrokeStartedEmpty();
    void sigLayersPicked(const KisNodeList &nodes);

private:
    MoveStrokeStrategy(const MoveStrokeStrategy &rhs, int lod);
    void setUndoEnabled(bool value);
    void setUpdatesEnabled(bool value);
private:
    void doCanvasUpdate(bool forceUpdate = false);
    void tryPostUpdateJob(bool forceUpdate);

private:
    struct Private;
    PkScopedPointer<Private> m_d;

    KisNodeSelectionRecipe m_requestedNodeSelection;
    KisNodeList m_nodes;
    PkSharedPointer<std::pair<KisNodeList, PkSet<KisNodeSP>>> m_sharedNodes;
    PkSet<KisNodeSP> m_blacklistedNodes;
    KisUpdatesFacade *m_updatesFacade {nullptr};
    PkPoint m_finalOffset;
    PkHash<KisNodeSP, PkRect> m_dirtyRects;
    bool m_updatesEnabled {true};

    PkElapsedTimer m_updateTimer;
    bool m_hasPostponedJob {false};
    const int m_updateInterval {30};

    template <typename Functor>
    void recursiveApplyNodes(const KisNodeList &nodes, Functor &&func);
};

#endif /* __MOVE_STROKE_STRATEGY_H */
