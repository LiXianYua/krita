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
    require(webpPlaybackRangeEndFrame(450, 20) == 8,
            "import duration must include the complete final frame");
    require(webpPlaybackRangeEndFrame(400, 20) == 7,
            "import duration must map exactly to its final frame");
    require(webpExportPlaybackEndFrame(0, 8, 0, 7) == 8 &&
                webpFrameToDurationMs(8, 0, 20) == 450,
            "export must honor a playback range extending past the last keyframe");
    require(webpExportPlaybackEndFrame(0, 3, 0, 7) == 7 &&
                webpExportPlaybackEndFrame(9, 3, 0, 7) == 7,
            "export must safely fall back for an invalid or early playback range");
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
