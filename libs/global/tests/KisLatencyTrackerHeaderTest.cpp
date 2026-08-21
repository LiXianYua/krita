#include "kis_latency_tracker.h"

#include <type_traits>

namespace {

class HeaderProbeTracker final : public KisLatencyTracker
{
public:
    using KisLatencyTracker::KisLatencyTracker;

protected:
    qint64 currentTimestamp() const override
    {
        return 0;
    }
};

static_assert(std::is_abstract<KisLatencyTracker>::value,
              "KisLatencyTracker must retain its timestamp source contract");
static_assert(sizeof(HeaderProbeTracker) >= sizeof(KisLatencyTracker),
              "the latency tracker header must define a complete derived type");

} // namespace
