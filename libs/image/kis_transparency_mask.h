/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_TRANSPARENCY_MASK_
#define _KIS_TRANSPARENCY_MASK_

#include "kis_types.h"
#include "kis_effect_mask.h"

class PkRect;

/**
 *  A transparency mask is a single channel mask that applies a particular
 *  transparency to the layer the mask belongs to. It differs from an
 *  adjustment layer in that it only works on its parent layer, while
 *  adjustment layers work on all layers below it in its layer group.
 *
 *  XXX: Use KisConfig::useProjections() to enable/disable the caching of
 *       the projection.
 */
class KRITAIMAGE_EXPORT KisTransparencyMask : public KisEffectMask
{
    Q_OBJECT

public:

    KisTransparencyMask(KisImageWSP image, const PkString &name);
    KisTransparencyMask(const KisTransparencyMask& rhs);
    ~KisTransparencyMask() override;

    KisNodeSP clone() const override {
        return KisNodeSP(new KisTransparencyMask(*this));
    }

    PkRect decorateRect(KisPaintDeviceSP &src, KisPaintDeviceSP &dst,
                       const PkRect & rc,
                       PositionToFilthy maskPos,
                       KisRenderPassFlags flags) const override;
    bool accept(KisNodeVisitor &v) override;
    void accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter) override;

    PkRect extent() const override;
    PkRect exactBounds() const override;

    PkRect changeRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;
    PkRect needRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;

    bool paintsOutsideSelection() const override;
};

#endif //_KIS_TRANSPARENCY_MASK_
