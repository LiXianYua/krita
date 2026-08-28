#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct WebPTimeline
{
    bool animated = false;
    int framerate = 0;
    int totalDurationMs = 0;
    std::vector<int> frameTimes;
};

bool buildWebPTimeline(const std::vector<int> &durations, WebPTimeline &timeline);
int webpPlaybackRangeEndFrame(int totalDurationMs, int framerate);
int webpExportPlaybackEndFrame(int playbackRangeStart,
                               int playbackRangeEnd,
                               int firstKeyframe,
                               int lastKeyframe);
int webpFrameToDurationMs(int endFrame, int firstKeyframe, int framerate);
bool isPlausibleIccProfile(const std::uint8_t *data, std::size_t size);
