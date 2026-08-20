/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

#include <PkResourceStorage.h>

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
};
