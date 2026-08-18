#pragma once

// Boost.Operators 的最小替身。KisCumulativeUndoData.h 只用了单类型形态
// `boost::equality_comparable<KisCumulativeUndoData>`（真 Boost 里这是
// `equality_comparable1` 经宏重载伪装出的单参数用法）：类自己写
// `operator==`，基类补出 `operator!=`。
//
// 为什么给 stub 而不是直接用系统 boost：试接要能在**没有 boost 的机器上**
// 复现，而且 boost 装没装、装的哪个版本会让"编不过"这件事失去判别力——判据
// 要压的是 Q-6/Q-7 配置替代品，不是第三方库探测。`-I stubs` 排在系统头前面，
// 所以本文件总是胜出。
//
// **脚手架，不是交付件。** boost 是否保留由 S 线的第三方依赖清点决定，与本
// 任务无关。内容与 pk/container/tests/graft/stubs/boost/operators.hpp 的
// equality_comparable1 同一手法，这里按真实调用点的单参数名字叫
// `equality_comparable`（KisCumulativeUndoData.h:15 写的就是这个名字，不是
// `equality_comparable1`）。

namespace boost {

// 真品是 `equality_comparable1<T, B = empty_base<T>>`，通过 CRTP 用派生类的
// `operator==` 生成 `operator!=`；真 Boost 里 `equality_comparable<T>`（单
// 模板参数）经预处理器重载机关转发到同一实现。友元定义（隐藏友元）与真品
// 一致：只能被 ADL 找到，不污染名字查找。
template <typename T>
struct equality_comparable
{
    friend bool operator!=(const T &lhs, const T &rhs) { return !(lhs == rhs); }
};

} // namespace boost
