/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISBRUSHENCLOSINGPRODUCER
#define KISBRUSHENCLOSINGPRODUCER

#include <kis_pixel_selection.h>
#include <PkPainterPath.h>

#include "KisToolBasicBrushBase.h"
#include "KisDynamicDelegatedTool.h"

class KisBrushEnclosingProducer : public KisDynamicDelegateTool<KisToolBasicBrushBase>
{
public:
    KisBrushEnclosingProducer(KoCanvasBase *canvas);
    ~KisBrushEnclosingProducer() override;
    
    bool hasUserInteractionRunning() const;
    
    void enclosingMaskProduced(KisPixelSelectionSP enclosingMask);

protected:
    void finishStroke(const PkPainterPath &stroke) override;
    void beginShape() override;
    void endShape() override;

private:
    bool m_hasUserInteractionRunning {false};

protected:
    void resetCursorStyle() override;
};

#endif
