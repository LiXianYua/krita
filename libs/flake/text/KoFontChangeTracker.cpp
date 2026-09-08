/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontChangeTracker.h"
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

struct KoFontChangeTracker::Private {

    explicit Private(PkStringList paths)
        : paths(std::move(paths)) {}
    PkStringList paths;
    std::vector<std::filesystem::file_time_type> timestamps;
};

KoFontChangeTracker::KoFontChangeTracker(PkStringList paths)
    : d(new Private(std::move(paths)))
{
    resetChangeTracker();
}

KoFontChangeTracker::~KoFontChangeTracker()
{
}

void KoFontChangeTracker::resetChangeTracker()
{
    d->timestamps.clear();
    for (const PkString &path : d->paths) {
        std::error_code error;
        d->timestamps.push_back(std::filesystem::last_write_time(path.PkToUtf8(), error));
    }
}

bool KoFontChangeTracker::directoriesChanged() const
{
    for (int i = 0; i < d->paths.size(); ++i) {
        std::error_code error;
        const auto timestamp = std::filesystem::last_write_time(d->paths.at(i).PkToUtf8(), error);
        if (static_cast<std::size_t>(i) >= d->timestamps.size() || timestamp != d->timestamps[static_cast<std::size_t>(i)]) {
            return true;
        }
    }
    return false;
}
