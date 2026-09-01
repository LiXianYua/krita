/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_PAINTER_BASED_STROKE_STRATEGY_H
#define __KIS_PAINTER_BASED_STROKE_STRATEGY_H

#include <PkString.h>
#include <PkVector.h>

#include "KisRunnableBasedStrokeStrategy.h"
#include <kritapaintop_export.h>
#include "kis_resources_snapshot.h"
#include "kis_selection.h"
#include "kis_indirect_painting_support.h"

class KisPainter;
class KisDistanceInformation;
class KisTransaction;
class KisFreehandStrokeInfo;
class KisMaskedFreehandStrokePainter;
class KisMaskingBrushRenderer;
class KisRunnableStrokeJobData;
class KisUndoStore;

class PAINTOP_EXPORT KisPainterBasedStrokeStrategy : public KisRunnableBasedStrokeStrategy
{
public:
    KisPainterBasedStrokeStrategy(const PkString &id,
                                  const KUndo2MagicString &name,
                                  KisResourcesSnapshotSP resources,
                                  PkVector<KisFreehandStrokeInfo*> strokeInfos);

    KisPainterBasedStrokeStrategy(const PkString &id,
                                  const KUndo2MagicString &name,
                                  KisResourcesSnapshotSP resources,
                                  KisFreehandStrokeInfo *strokeInfo);

    ~KisPainterBasedStrokeStrategy();

    void initStrokeCallback() override;
    void finishStrokeCallback() override;
    void cancelStrokeCallback() override;

    void suspendStrokeCallback() override;
    void resumeStrokeCallback() override;

protected:
    KisNodeSP targetNode() const;
    KisPaintDeviceSP targetDevice() const;
    KisSelectionSP activeSelection() const;

    KisMaskedFreehandStrokePainter* maskedPainter(int strokeInfoId);
    int numMaskedPainters() const;

    void setUndoEnabled(bool value);

    /**
     * Return true if the descendant should execute a few more jobs before issuing setDirty()
     * call on the layer.
     *
     * If the returned value is true, then the stroke actually paints **not** on the
     * layer's paint device, but on some intermediate device owned by
     * KisPainterBasedStrokeStrategy and one should merge it first before asking the
     * update.
     *
     * The value can be true only when the stroke is declared to support masked brush!
     * \see supportsMaskingBrush()
     */
    bool needsMaskingUpdates() const;

    /**
     * Create a list of update jobs that should be run before issuing the setDirty()
     * call on the node
     *
     * \see needsMaskingUpdates()
     */
    PkVector<KisRunnableStrokeJobData*> doMaskingBrushUpdates(const PkVector<PkRect> &rects);

protected:

    /**
     * The descendants may declare if this stroke should support auto-creation
     * of the masked brush. Default value: false
     */
    void setSupportsMaskingBrush(bool value);

    /**
     * Return if the stroke should auto-create a masked brush from the provided
     * paintop preset or not
     */
    bool supportsMaskingBrush() const;

    void setSupportsIndirectPainting(bool value);
    bool supportsIndirectPainting() const;

    bool supportsContinuedInterstrokeData() const;
    void setSupportsContinuedInterstrokeData(bool value);

    bool supportsTimedMergeId() const;
    void setSupportsTimedMergeId(bool value);

protected:
    KisPainterBasedStrokeStrategy(const KisPainterBasedStrokeStrategy &rhs, int levelOfDetail);

private:
    void init();
    void initPainters(KisPaintDeviceSP targetDevice, KisPaintDeviceSP maskingDevice,
                      KisSelectionSP selection,
                      bool hasIndirectPainting,
                      const PkString &indirectPaintingCompositeOp);
    void deletePainters();
    inline int timedID(const PkString &id){
        return int(qHash(id));
    }

private:
    KisResourcesSnapshotSP m_resources;
    PkVector<KisFreehandStrokeInfo*> m_strokeInfos;
    PkVector<KisFreehandStrokeInfo*> m_maskStrokeInfos;
    PkVector<KisMaskedFreehandStrokePainter*> m_maskedPainters;

    PkScopedPointer<KisTransaction> m_transaction;

    PkScopedPointer<KisMaskingBrushRenderer> m_maskingBrushRenderer;

    KisPaintDeviceSP m_targetDevice;
    KisSelectionSP m_activeSelection;

    std::unique_ptr<KUndo2Command> m_autokeyCommand;

    bool m_useMergeID {false};

    bool m_supportsMaskingBrush {false};
    bool m_supportsIndirectPainting {false};
    bool m_supportsContinuedInterstrokeData {false};

    KisIndirectPaintingSupport::FinalMergeSuspenderSP m_finalMergeSuspender;

    struct FakeUndoData {
        FakeUndoData();
        ~FakeUndoData();
        PkScopedPointer<KisUndoStore> undoStore;
        PkScopedPointer<KisPostExecutionUndoAdapter> undoAdapter;
    };
    PkScopedPointer<FakeUndoData> m_fakeUndoData;

};

#endif /* __KIS_PAINTER_BASED_STROKE_STRATEGY_H */
