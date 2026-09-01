/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISLASSOENCLOSINGPRODUCER
#define KISLASSOENCLOSINGPRODUCER

#include <kis_pixel_selection.h>
#include <KisToolOutlineBase.h>
#include <PkPoint.h>
#include <PkVector.h>

#include "KisDynamicDelegatedTool.h"

class KisLassoEnclosingProducer : public KisDynamicDelegateTool<KisToolOutlineBase>
{
public:
    KisLassoEnclosingProducer(KoCanvasBase *canvas);
    ~KisLassoEnclosingProducer() override;
    
    bool hasUserInteractionRunning() const;
    
    void enclosingMaskProduced(KisPixelSelectionSP enclosingMask);

protected:
    void finishOutline(const PkVector<PkPointF> &points) override;
    void beginShape() override;
    void endShape() override;

private:
    bool m_hasUserInteractionRunning {false};

protected:
    void resetCursorStyle() override;
};

#endif
