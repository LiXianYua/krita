#include "../webp_validation.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    WebPTimeline timeline;
    require(!buildWebPTimeline({}, timeline), "empty WebP frame list must fail");
    require(!buildWebPTimeline({0}, timeline), "zero frame duration must fail");
    require(buildWebPTimeline({100}, timeline) && !timeline.animated &&
                timeline.frameTimes == std::vector<int>{0},
            "one frame must remain a still image");
    require(buildWebPTimeline({100, 250, 50}, timeline) && timeline.animated &&
                timeline.frameTimes.size() == 3 && timeline.frameTimes[0] == 0 &&
                timeline.frameTimes[0] < timeline.frameTimes[1] &&
                timeline.frameTimes[1] < timeline.frameTimes[2],
            "variable durations must map cumulative milliseconds monotonically");
    const std::uint8_t malformedIcc[] = {'n', 'o', 't', 'i', 'c', 'c'};
    require(!isPlausibleIccProfile(malformedIcc, sizeof(malformedIcc)),
            "malformed embedded ICC must fall back without registry dereference");
    std::vector<std::uint8_t> minimalIcc(128, 0);
    minimalIcc[3] = 128;
    minimalIcc[36] = 'a';
    minimalIcc[37] = 'c';
    minimalIcc[38] = 's';
    minimalIcc[39] = 'p';
    require(isPlausibleIccProfile(minimalIcc.data(), minimalIcc.size()),
            "structurally valid ICC header must reach the color registry");
    return 0;
}
