/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_transparency_mask.h"

#include "kis_debug.h"
#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoCompositeOpRegistry.h>
#include "kis_paint_device.h"
#include "kis_painter.h"
#include "kis_node_visitor.h"
#include "kis_processing_visitor.h"
#include "kis_image.h"

KisTransparencyMask::KisTransparencyMask(KisImageWSP image, const PkString &name)
        : KisEffectMask(image, name)
{
}

KisTransparencyMask::KisTransparencyMask(const KisTransparencyMask& rhs)
        : KisEffectMask(rhs)
{
}

KisTransparencyMask::~KisTransparencyMask()
{
}

PkRect KisTransparencyMask::decorateRect(KisPaintDeviceSP &src,
                                        KisPaintDeviceSP &dst,
                                        const PkRect & rc,
                                        PositionToFilthy maskPos,
                                        KisRenderPassFlags flags) const
{
    Q_UNUSED(maskPos);
    Q_UNUSED(flags);

    if (src != dst) {
        KisPainter::copyAreaOptimized(rc.topLeft(), src, dst, rc);
        src->fill(rc, KoColor::createTransparent(src->colorSpace()));
    }

    return rc;
}

PkRect KisTransparencyMask::extent() const
{
    return parent() ? parent()->extent() : PkRect();
}

PkRect KisTransparencyMask::exactBounds() const
{
    return parent() ? parent()->exactBounds() : PkRect();
}

PkRect KisTransparencyMask::changeRect(const PkRect &rect, PositionToFilthy pos) const
{
    /**
     * Selection on transparency masks have no special meaning:
     * They do crop both: change and need area
     */
    return KisMask::changeRect(rect, pos);
}

PkRect KisTransparencyMask::needRect(const PkRect &rect, PositionToFilthy pos) const
{
    /**
     * Selection on transparency masks have no special meaning:
     * They do crop both: change and need area
     */
    return KisMask::needRect(rect, pos);
}

bool KisTransparencyMask::paintsOutsideSelection() const
{
    return true;
}

bool KisTransparencyMask::accept(KisNodeVisitor &v)
{
    return v.visit(this);
}

void KisTransparencyMask::accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter)
{
    return visitor.visit(this, undoAdapter);
}

