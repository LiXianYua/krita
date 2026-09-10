/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Native clipboard payload value shared by the shape-clipboard producer and the
 * application-owned clipboard service.
 */
#ifndef PKCLIPBOARDDATA_H
#define PKCLIPBOARDDATA_H

#include <PkByteArray.h>
#include <PkString.h>

#include "kritaflake_export.h"

/**
 * One clipboard payload, format-tagged the way the clipboard contract tags it.
 *
 * A `has*` flag says the corresponding member carries the payload for that
 * format. An unset flag means the format is absent, and the member is then
 * empty and must be ignored. `text` and `html` are decoded text; `svg` is the
 * raw SVG document bytes.
 *
 * Moving a payload into the platform clipboard, and reading one back out, is
 * the host's job: nothing in the kernel opens a platform clipboard, so this
 * value is the kernel-side boundary both sides of that transfer agree on.
 *
 * The type lives in flake because flake is the lowest layer that both the
 * producer and the application-owned consumer already depend on. It carries no
 * host dependency and is header-only, so it can be re-homed to a `pk/` provider
 * directory without a consumer change once such a directory exists.
 */
struct KRITAFLAKE_EXPORT PkClipboardData {
    bool hasText = false;
    bool hasHtml = false;
    bool hasSvg = false;
    PkString text;
    PkString html;
    PkByteArray svg;
};

#endif // PKCLIPBOARDDATA_H
