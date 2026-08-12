#pragma once

// Boost.Operators 的最小替身。`kis_fill_interval.h` 只用了 `equality_comparable1`：
// 类自己写 `operator==`，基类补出 `operator!=`。
//
// 为什么给 stub 而不是直接用系统 boost：试接要能在**没有 boost 的机器上**复现，
// 而且 boost 装没装、装的哪个版本会让"编不过"这件事失去判别力 —— 判据②要压的是
// 容器替代品，不是第三方库探测。`-I stubs` 排在系统头前面，所以本文件总是胜出。
//
// **脚手架，不是交付件。** boost 是否保留由 S 线的第三方依赖清点决定，与 R-02 无关。

namespace boost {

// 真品是 `equality_comparable1<T, B = empty_base<T>>`，通过 CRTP 用派生类的
// `operator==` 生成 `operator!=`。友元定义（隐藏友元）与真品一致：只能被 ADL
// 找到，不污染名字查找。
template <typename T>
struct equality_comparable1
{
    friend bool operator!=(const T &lhs, const T &rhs) { return !(lhs == rhs); }
};

} // namespace boost
