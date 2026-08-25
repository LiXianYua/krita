/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SELECTION_COMPONENT_H
#define KIS_SELECTION_COMPONENT_H

#include <kritaimage_export.h>
#include <PkPainterPath.h>
#include <PkRect.h>

#include <PkTransform.h>

class PkRect;
class KUndo2Command;
class KisSelection;

#include "kis_types.h"

class KRITAIMAGE_EXPORT KisSelectionComponent
{
public:
    KisSelectionComponent() {}
    virtual ~KisSelectionComponent() {}

    virtual KisSelectionComponent* clone(KisSelection* selection) = 0;

    virtual void renderToProjection(KisPaintDeviceSP projection) = 0;
    virtual void renderToProjection(KisPaintDeviceSP projection, const PkRect& r) = 0;

    virtual void moveX(qint32 x);
    virtual void moveY(qint32 y) { Q_UNUSED(y); }

    virtual KUndo2Command* transform(const PkTransform &transform) {
        Q_UNUSED(transform);
        return 0;
    }

    virtual bool isEmpty() const = 0;
    virtual PkPainterPath outlineCache() const = 0;
    virtual bool outlineCacheValid() const = 0;
    virtual void recalculateOutlineCache() = 0;

    virtual KUndo2Command* resetToEmpty() { return 0; }

    virtual void setResolutionProxy(KisImageResolutionProxySP /* resolutionProxy */) { };
};

#endif
