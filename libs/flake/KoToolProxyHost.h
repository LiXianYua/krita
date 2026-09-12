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
 * Host-side narrow surface of KoToolProxy.
 *
 * D-B (2026-09-12) removed the per-configuration base list of KoToolProxy: it is
 * now a single `PkObject` definition visible to every translation unit. This
 * interface is kept because it is the narrow, pointer-only face through which
 * the retained manager and the canvas reach a proxy without depending on the
 * proxy's concrete type — parameters and returns are pointers only, no
 * configuration-dependent type takes part.
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
