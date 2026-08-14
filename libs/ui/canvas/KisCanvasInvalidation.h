/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CANVAS_INVALIDATION_H
#define KIS_CANVAS_INVALIDATION_H

#include <kritacanvas_export.h>

/**
 * Narrow canvas capability for invalidating the complete projection and all
 * overlays. This is intentionally distinct from KoCanvasBase's rectangular
 * update: tool decorations can extend beyond the image bounds.
 */
class KRITACANVAS_EXPORT KisCanvasInvalidation
{
public:
    virtual ~KisCanvasInvalidation();

    virtual void invalidateAll() = 0;
};

#endif // KIS_CANVAS_INVALIDATION_H
