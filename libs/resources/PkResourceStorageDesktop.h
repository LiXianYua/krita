/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <PkResourceStorage.h>

#include <cstdint>

class PkResourceStorageDesktop final : public PkResourceStorage
{
public:
    std::unique_ptr<EntryIterator> listEntries(const PkString &path,
                                               const std::vector<PkString> &nameFilters,
                                               EntryKind kind,
                                               bool recursive) const override;
    bool exists(const PkString &path) const override;
    bool mkpath(const PkString &path) const override;
    bool remove(const PkString &path) const override;
    PkString absolutePath(const PkString &path) const override;
    PkString platformDir(PlatformDir kind) const override;

    // Desktop-only metadata used by the resource backends. This stats the
    // requested target directly, so symlinked storage roots and resources do
    // not depend on comparing canonical and iterator spellings of a path.
    int64_t lastModified(const PkString &path) const;
};
