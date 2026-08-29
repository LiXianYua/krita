/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMAGE_CONFIG_PATHS_H
#define KIS_IMAGE_CONFIG_PATHS_H

#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <string>
#include <thread>

namespace KisImageConfigPaths
{

enum class TransientFallback {
    Allow,
    Reject,
};

struct LocationSelection {
    std::filesystem::path location;
    bool probeSucceeded = false;
};

inline bool ensureDirectory(const std::filesystem::path &directory)
{
    if (directory.empty()) {
        return false;
    }

    std::error_code error;
    if (std::filesystem::create_directories(directory, error)) {
        return true;
    }
    return !error && std::filesystem::is_directory(directory, error) && !error;
}

inline std::uint64_t probeProcessNonce() noexcept
{
    static const std::uint64_t nonce = []() noexcept {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::uint64_t value = static_cast<std::uint64_t>(now);
        try {
            std::random_device random;
            value ^= static_cast<std::uint64_t>(random()) << 32;
            value ^= static_cast<std::uint64_t>(random());
        } catch (...) {
            // The clock, thread id, and process-local sequence below still
            // provide collision resistance when random_device is unavailable.
        }
        return value;
    }();
    return nonce;
}

inline bool probeWritableDirectory(const std::filesystem::path &directory)
{
    if (directory.empty()) {
        return false;
    }

    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        return false;
    }

    static std::atomic<std::uint64_t> sequence{0};
    const std::uint64_t serial = sequence.fetch_add(1, std::memory_order_relaxed);
    const auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const std::filesystem::path candidate = directory /
        (std::string("krita_test_swap_location_") +
         std::to_string(probeProcessNonce()) + "_" +
         std::to_string(static_cast<std::uint64_t>(threadId)) + "_" +
         std::to_string(serial));

    if (std::filesystem::exists(candidate, error) || error) {
        return false;
    }

    std::ofstream probe(candidate, std::ios::binary | std::ios::out);
    if (!probe.is_open()) {
        return false;
    }
    probe.put('\0');
    probe.flush();
    const bool writeSucceeded = probe.good();
    probe.close();
    const bool closeSucceeded = !probe.fail();

    error.clear();
    const bool cleanupSucceeded = std::filesystem::remove(candidate, error) && !error;
    return writeSucceeded && closeSucceeded && cleanupSucceeded;
}

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

inline LocationSelection selectWritableLocation(
    const std::filesystem::path &preferred,
    const std::filesystem::path &stableFallback,
    const std::filesystem::path &transientFallback,
    const std::filesystem::path &homeFallback,
    const std::filesystem::path &lastResort,
    TransientFallback transientPolicy,
    const std::function<bool(const std::filesystem::path &)> &isWritable)
{
    const std::filesystem::path chosen = chooseWritableLocation(
        preferred, stableFallback, transientFallback, homeFallback,
        transientPolicy, isWritable);
    return chosen.empty()
        ? LocationSelection{lastResort, false}
        : LocationSelection{chosen, true};
}

} // namespace KisImageConfigPaths

#endif // KIS_IMAGE_CONFIG_PATHS_H
