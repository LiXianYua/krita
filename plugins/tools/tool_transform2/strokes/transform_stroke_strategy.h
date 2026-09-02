/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TRANSFORM_STROKE_STRATEGY_H
#define __TRANSFORM_STROKE_STRATEGY_H

#include <PkObject.h>
#include <PkHash.h>
#include <PkSignalCompat.h>
#include <PkMutex.h>
#include <KoUpdater.h>
#include <kis_stroke_strategy_undo_command_based.h>
#include <kis_types.h>
#include "tool_transform_args.h"
#include "transform_transaction_properties.h"
#include <kis_processing_visitor.h>
#include <kritatooltransform_export.h>
#include <boost/optional.hpp>
#include "commands_new/KisUpdateCommandEx.h"

class KisPostExecutionUndoAdapter;
class KisUpdatesFacade;
class KisDecoratedNodeInterface;
class KisSavedMacroCommand;


class TransformStrokeStrategy : public PkShellObject, public KisStrokeStrategyUndoCommandBased
{
public:
    struct TransformAllData : public KisStrokeJobData {
        TransformAllData(const ToolTransformArgs &_config)
            : KisStrokeJobData(SEQUENTIAL, NORMAL),
              config(_config) {}

        ToolTransformArgs config;
    };


    class TransformData : public KisStrokeJobData {
    public:
        enum Destination {
            PAINT_DEVICE,
            SELECTION,
        };

    public:
    TransformData(Destination _destination, const ToolTransformArgs &_config, KisNodeSP _node)
            : KisStrokeJobData(CONCURRENT, NORMAL),
            destination(_destination),
            config(_config),
            node(_node)
        {
        }

        Destination destination;
        ToolTransformArgs config;
        KisNodeSP node;
    };

    class ClearSelectionData : public KisStrokeJobData {
    public:
        ClearSelectionData(KisNodeSP _node)
            : KisStrokeJobData(SEQUENTIAL, EXCLUSIVE),
              node(_node)
        {
        }
        KisNodeSP node;
    };

    class PreparePreviewData : public KisStrokeJobData {
    public:
        PreparePreviewData()
            : KisStrokeJobData(BARRIER, NORMAL)
        {
        }
    };

    class CalculateConvexHullData : public KisStrokeJobData {
    public:
        CalculateConvexHullData()
            : KisStrokeJobData(SEQUENTIAL, NORMAL) // Is this right?
        {}
    };

public:
    TransformStrokeStrategy(ToolTransformArgs::TransformMode mode,
                            const PkString &filterId,
                            bool forceReset,
                            KisNodeList rootNodes,
                            KisSelectionSP selection,
                            KisStrokeUndoFacade *undoFacade, KisUpdatesFacade *updatesFacade);

    ~TransformStrokeStrategy() override;

    void initStrokeCallback() override;
    void finishStrokeCallback() override;
    void cancelStrokeCallback() override;
    void doStrokeCallback(KisStrokeJobData *data) override;

signals:
    void sigTransactionGenerated(TransformTransactionProperties transaction, ToolTransformArgs args, void *cookie);
    void sigPreviewDeviceReady(KisPaintDeviceSP device);
    void sigConvexHullCalculated(PkPolygon convexHull, void *cookie);

protected:
    void postProcessToplevelCommand(KUndo2Command *command) override;

private:
    KoUpdaterPtr fetchUpdater(KisNodeSP node);

    void clearSelection(KisPaintDeviceSP device);

    bool checkBelongsToSelection(KisPaintDeviceSP device) const;

    KisPaintDeviceSP createDeviceCache(KisPaintDeviceSP src);

    bool haveDeviceInCache(KisPaintDeviceSP src);
    void putDeviceCache(KisPaintDeviceSP src, KisPaintDeviceSP cache);
    KisPaintDeviceSP getDeviceCache(KisPaintDeviceSP src);

    void finishStrokeImpl(bool applyTransform,
                          const ToolTransformArgs &args);

    PkPolygon calculateConvexHull();

private:
    KisUpdatesFacade *m_updatesFacade;
    KisBatchNodeUpdateSP m_updateData;
    bool m_updatesDisabled = false;
    ToolTransformArgs::TransformMode m_mode;
    PkString m_filterId;
    bool m_forceReset;

    KisSelectionSP m_selection;

    PkMutex m_devicesCacheMutex;
    PkHash<KisPaintDevice*, KisPaintDeviceSP> m_devicesCacheHash;

    KisTransformMaskSP writeToTransformMask;

    ToolTransformArgs m_initialTransformArgs;
    boost::optional<ToolTransformArgs> m_savedTransformArgs;
    KisNodeList m_rootNodes;
    KisNodeList m_processedNodes;
    int m_currentTime = -1;
    PkList<KisSelectionSP> m_deactivatedSelections;
    PkList<KisNodeSP> m_hiddenProjectionLeaves;
    PkList<KisSelectionMaskSP> m_deactivatedOverlaySelectionMasks;
    PkVector<KisDecoratedNodeInterface*> m_disabledDecoratedNodes;

    const KisSavedMacroCommand *m_overriddenCommand = 0;
    PkVector<const KUndo2Command*> m_skippedWhileMergeCommands;

    bool m_finalizingActionsStarted = false;
    bool m_convexHullHasBeenCalculated = false;
};

#endif /* __TRANSFORM_STROKE_STRATEGY_H */
