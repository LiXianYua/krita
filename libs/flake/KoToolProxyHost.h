/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Host contract between the retained tool manager and the host tool proxy.
 */
#ifndef KOTOOLPROXYHOST_H
#define KOTOOLPROXYHOST_H

#include "kritaflake_export.h"

class KoToolBase;
class KoCanvasController;
class KoPointerEvent;
class PkObject;

/**
 * Bucket-agnostic host surface of KoToolProxy.
 *
 * KoToolProxy's base list is spelled per configuration, so its definition lives
 * in the Qt translation unit only and a native translation unit must not see the
 * class layout at all. Everything native code needs from a tool proxy comes
 * through this interface: parameters and returns are pointers only, no
 * configuration-dependent type takes part, hence a single mangled name in both
 * buckets. Base-class upcasts then happen where the object is defined — on the
 * host side — and use the real (Qt) layout.
 */
class KRITAFLAKE_EXPORT KoToolProxyHost
{
public:
    virtual ~KoToolProxyHost() = default;

    virtual void setActiveTool(KoToolBase *tool) = 0;
    virtual void setCanvasController(KoCanvasController *controller) = 0;

    virtual PkObject *toolProxyObject() = 0;
    virtual void repaintToolDecorations() = 0;
    virtual KoPointerEvent *lastDeliveredToolPointerEvent() = 0;
};

#endif // KOTOOLPROXYHOST_H
