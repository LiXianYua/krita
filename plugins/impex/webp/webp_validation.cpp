#include "webp_validation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

bool buildWebPTimeline(const std::vector<int> &durations, WebPTimeline &timeline)
{
    timeline = {};
    if (durations.empty() || std::any_of(durations.begin(), durations.end(), [](int d) { return d <= 0; })) {
        return false;
    }
    int quantum = durations.front();
    for (int duration : durations) {
        quantum = std::gcd(quantum, duration);
        if (timeline.totalDurationMs > std::numeric_limits<int>::max() - duration) {
            return false;
        }
        timeline.totalDurationMs += duration;
    }
    timeline.animated = durations.size() > 1;
    timeline.framerate = timeline.animated
        ? std::max(1, static_cast<int>(std::lround(1000.0 / quantum)))
        : 0;
    int cumulative = 0;
    int previous = -1;
    for (int duration : durations) {
        int frame = timeline.animated
            ? static_cast<int>(std::lround(cumulative * static_cast<double>(timeline.framerate) / 1000.0))
            : 0;
        if (timeline.animated && frame <= previous) {
            frame = previous + 1;
        }
        timeline.frameTimes.push_back(frame);
        previous = frame;
        cumulative += duration;
    }
    return true;
}

bool isPlausibleIccProfile(const std::uint8_t *data, std::size_t size)
{
    if (!data || size < 128 || data[36] != 'a' || data[37] != 'c' ||
        data[38] != 's' || data[39] != 'p') {
        return false;
    }
    const std::size_t declaredSize =
        (static_cast<std::size_t>(data[0]) << 24) |
        (static_cast<std::size_t>(data[1]) << 16) |
        (static_cast<std::size_t>(data[2]) << 8) |
        static_cast<std::size_t>(data[3]);
    return declaredSize >= 128 && declaredSize <= size;
}
