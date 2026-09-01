/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_FILTER_MASK_
#define _KIS_FILTER_MASK_

#include "kis_types.h"
#include "kis_effect_mask.h"

#include "kis_node_filter_interface.h"
#include "kis_filter_configuration.h"
#include <PkScopedPointer.h>


/**
   An filter mask is a single channel mask that applies a particular
   filter to the layer the mask belongs to. It differs from an
   adjustment layer in that it only works on its parent layer, while
   adjustment layers work on all layers below it in its layer group.
*/

class KRITAIMAGE_EXPORT KisFilterMask : public KisEffectMask, public KisNodeFilterInterface
{
public:
    /**
     * Create an empty filter mask.
     */
    KisFilterMask(KisImageWSP image, const PkString &name = PkString());

    KisFilterMask(const KisFilterMask& rhs);

    ~KisFilterMask() override;

    KisNodeSP clone() const override {
        return KisNodeSP(new KisFilterMask(*this));
    }

    bool accept(KisNodeVisitor &v) override;
    void accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter) override;

    void setFilter(KisFilterConfigurationSP filterConfig, bool checkCompareConfig = true) override;

    PkRect decorateRect(KisPaintDeviceSP &src,
                       KisPaintDeviceSP &dst,
                       const PkRect & rc,
                       PositionToFilthy maskPos,
                       KisRenderPassFlags flags) const override;

    PkRect extent() const override;
    PkRect exactBounds() const override;

    PkRect changeRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;
    PkRect needRect(const PkRect &rect, PositionToFilthy pos = N_FILTHY) const override;

private:
    bool filterNeedsTransparentPixels() const;

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif //_KIS_FILTER_MASK_
