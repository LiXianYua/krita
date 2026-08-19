#include "PkDateTime.h"

#include <type_traits>

// 哨兵设计（TimePoint::min() 作"无效/未设置"标记）不应该给类型带来额外状态：
// 整个类型必须仍是对单个 TimePoint 的薄包装，没有隐藏的 bool 有效位——与
// PkElapsedTimer.cpp 的同名断言同一个理由。
static_assert(sizeof(PkDateTime) == sizeof(PkDateTime::TimePoint),
              "PkDateTime must stay a thin wrapper over one TimePoint, no extra state");

// PkDateTime 表达墙钟时间（跟随系统时间调整），与 PkElapsedTimer 故意选
// steady_clock（单调、不受系统时间调整影响）分工明确：这里必须是 system_clock，
// 不能反过来。
static_assert(std::is_same<PkDateTime::Clock, std::chrono::system_clock>::value,
              "PkDateTime requires std::chrono::system_clock (wall-clock semantics), not a steady clock");
