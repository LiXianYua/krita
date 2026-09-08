/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Resource-only cursor contract between retained tools and the UI host.
 */
#ifndef KOCANVASCURSORHOST_H
#define KOCANVASCURSORHOST_H

#include <PkPoint.h>
#include <PkSize.h>
#include <PkString.h>

#include "kritaflake_export.h"

class QCursor;

class KRITAFLAKE_EXPORT KoCanvasCursorHost
{
public:
    virtual ~KoCanvasCursorHost() = default;

    virtual QCursor loadCursorResource(const PkString &resource,
                                       const PkSize &size,
                                       const PkPoint &hotspot) const = 0;
};

#endif // KOCANVASCURSORHOST_H
