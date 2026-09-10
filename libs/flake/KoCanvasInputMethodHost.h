/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Input-method notification contract between retained tools and the UI host.
 */
#ifndef KOCANVASINPUTMETHODHOST_H
#define KOCANVASINPUTMETHODHOST_H

#include <PkNamespace.h>

#include "kritaflake_export.h"

/**
 * Host-side receiver for the input-method state changes a tool raises.
 *
 * A tool discovers its host with dynamic_cast<KoCanvasInputMethodHost *>(canvas);
 * a canvas that is not an input-method host returns nullptr and the tool raises
 * nothing. The host is owned by its canvas and must outlive every tool bound to
 * that canvas; updateInputMethod() runs on the thread owning the canvas.
 *
 * queries carries the Pk::InputMethodQuery bits whose host-visible value may have
 * changed — the host re-queries those and updates its platform input method. The
 * core is the only side that knows the tool state behind such a bit, so the
 * notification exists to hand that fact to the host, not to carry the value.
 */
class KRITAFLAKE_EXPORT KoCanvasInputMethodHost
{
public:
    virtual ~KoCanvasInputMethodHost() = default;

    virtual void updateInputMethod(Pk::InputMethodQueries) {}
};

#endif // KOCANVASINPUTMETHODHOST_H
