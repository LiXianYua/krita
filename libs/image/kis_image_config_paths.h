/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMAGE_CONFIG_PATHS_H
#define KIS_IMAGE_CONFIG_PATHS_H

#include <array>
#include <filesystem>
#include <functional>

namespace KisImageConfigPaths
{

enum class TransientFallback {
    Allow,
    Reject,
};

inline std::filesystem::path chooseWritableLocation(
    const std::filesystem::path &preferred,
    const std::filesystem::path &stableFallback,
    const std::filesystem::path &transientFallback,
    const std::filesystem::path &homeFallback,
    TransientFallback transientPolicy,
    const std::function<bool(const std::filesystem::path &)> &isWritable)
{
    const std::array<std::filesystem::path, 4> candidates = {
        preferred,
        stableFallback,
        transientPolicy == TransientFallback::Allow ? transientFallback : std::filesystem::path(),
        homeFallback,
    };

    std::filesystem::path previous;
    for (const std::filesystem::path &candidate : candidates) {
        if (candidate.empty() || candidate == previous) {
            continue;
        }
        previous = candidate;
        if (isWritable(candidate)) {
            return candidate;
        }
    }
    return std::filesystem::path();
}

} // namespace KisImageConfigPaths

#endif // KIS_IMAGE_CONFIG_PATHS_H
