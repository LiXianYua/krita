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

#include <cstdint>

#include "kritaflake_export.h"

class QCursor;

/**
 * Opaque identity for an immutable snapshot owned by one cursor host.
 *
 * Import is synchronous and may only run on the owning UI thread. A successful
 * import copies the cursor and returns a nonzero token scoped to one
 * KoCanvasCursorHost instance. Re-importing the same snapshot in that instance
 * returns the same token. Every nonzero token remains bound to that immutable
 * snapshot until the host instance is destroyed and is never reused or
 * rebound. Import failure, including wrong-thread import, returns zero.
 *
 * Token zero denotes the default platform cursor. Applying zero restores that
 * default platform cursor. Tokens from another host instance are invalid.
 */
class KRITAFLAKE_EXPORT KisCanvasCursorToken
{
public:
    constexpr KisCanvasCursorToken() = default;
    explicit constexpr KisCanvasCursorToken(std::uint64_t value)
        : m_value(value)
    {
    }

    constexpr std::uint64_t value() const { return m_value; }
    explicit constexpr operator bool() const { return m_value != 0; }

    friend constexpr bool operator==(KisCanvasCursorToken lhs,
                                     KisCanvasCursorToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

    friend constexpr bool operator!=(KisCanvasCursorToken lhs,
                                     KisCanvasCursorToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    std::uint64_t m_value = 0;
};

class KRITAFLAKE_EXPORT KoCanvasCursorHost
{
public:
    virtual ~KoCanvasCursorHost() = default;

    virtual QCursor loadCursorResource(const PkString &resource,
                                       const PkSize &size,
                                       const PkPoint &hotspot) const = 0;

    /** Import a synchronous immutable cursor snapshot under the contract above. */
    virtual KisCanvasCursorToken toolImportCursor(const QCursor &) const { return {}; }

    /**
     * Resolve a token for compatibility observers and validate host ownership.
     * Zero resolves to the default platform cursor. A nonzero token resolves
     * only while it belongs to this host; nullptr means foreign, stale, or
     * otherwise rejected. The returned immutable snapshot remains valid for
     * the lifetime of this host.
     */
    virtual const QCursor *toolCursorSnapshot(KisCanvasCursorToken) const { return nullptr; }

    /**
     * Apply a token accepted by toolCursorSnapshot(). Zero restores the default
     * platform cursor. A direct call with an invalid token must be a no-op.
     */
    virtual void toolApplyCursor(KisCanvasCursorToken) {}
};

#endif // KOCANVASCURSORHOST_H
