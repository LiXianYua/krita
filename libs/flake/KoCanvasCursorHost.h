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
#include <PkNamespace.h>

#include <cstdint>

#include "kritaflake_export.h"

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

    /** 载入具名游标资源并导入为不可变快照；返回标识它的 token，失败返回零。 */
    virtual KisCanvasCursorToken loadCursorResource(const PkString &resource,
                                                    const PkSize &size,
                                                    const PkPoint &hotspot) const = 0;

    /** 导入一个平台游标形状（无资源、无热点）。宿主不能服务该形状时返回零。 */
    virtual KisCanvasCursorToken toolShapeCursorToken(Pk::CursorShape) const { return {}; }

    /** 宿主归属判定。token 零 = 平台默认游标，任何宿主都拥有；非零 token 只属本宿主。 */
    virtual bool toolOwnsCursor(KisCanvasCursorToken token) const
    { return token.value() == 0; }

    /** 应用 token。零恢复平台默认游标。非本宿主 token 必须是 no-op。 */
    virtual void toolApplyCursor(KisCanvasCursorToken) {}
};

#endif // KOCANVASCURSORHOST_H
