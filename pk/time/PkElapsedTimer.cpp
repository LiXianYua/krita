#include "PkElapsedTimer.h"

// std::chrono::steady_clock 必须是单调时钟——elapsed()/nsecsElapsed()/restart()
// "真实增长、不受系统时间调整影响"这条语义全靠它。std::chrono::system_clock
// 不保证这点（它可能被 NTP/用户手动调整往回拨），所以本类型故意选
// steady_clock 而不是 PkDateTime（R-16 Task 2）要用的 system_clock。
static_assert(std::chrono::steady_clock::is_steady,
              "PkElapsedTimer requires a steady (monotonic) clock");

// 哨兵设计（TimePoint::min() 作"未 start()"标记）不应该给类型带来额外状态：
// 整个类型必须仍是对单个 TimePoint 的薄包装，没有隐藏的 bool 有效位。
static_assert(sizeof(PkElapsedTimer) == sizeof(PkElapsedTimer::TimePoint),
              "PkElapsedTimer must stay a thin wrapper over one TimePoint, no extra state");
