#pragma once

#include <utility>

// ---------------------------------------------------------------------------
// PkPair<A, B> —— Qt5 QPair<A, B> 的替代品。SRC 出现 535 次，qMakePair 41 次
// （另有 std::make_pair 65 次，那些本来就是标准库调用，不归我们）。
//
// **用别名实现，不自写模板。** Qt5 的 QPair 是它自己的模板（Qt6 才改成
// std::pair 的别名），但公开语义——`.first` / `.second` / 六个比较运算符的
// 字典序——与 std::pair 逐条相同。自写一份只会多出一个把字典序写歪的机会。
//
// 实测（真 Qt 5.15.7）：
//   (1,2)<(1,3) 真 · (1,2)<(2,0) 真 · (1,2)==(1,2) 真 · qMakePair(1,2)==(1,2) 真
// std::pair 的比较运算符给出同样的结果（单测逐条压过，不是假设）。
//
// **别名不是新类型**：`PkPair<int,int>` 与 `std::pair<int,int>` 是同一个类型。
// 好处是与标准库算法（std::map 的 value_type、std::minmax 的返回值等）天然互通，
// 调用点里 `qMakePair(...)` 与 `std::make_pair(...)` 的结果可以互相赋值。
// 代价是不能给 PkPair 做偏特化或重载解析上的区分——Qt 调用点里没有这种用法。
//
// ---- 为什么函数叫 qMakePair 而不是 pkMakePair ----
//
// 与 PkStringHash.h 的 `qHash(const PkString &)` 同一条理由：**它是自由函数，
// 不是类名**。「全局 Pk 前缀」这条约定管的是类名（compat 垫片靠
// `#define QPair PkPair` 之类的宏改写类型名）。自由函数走的是普通名字查找 /
// ADL，调用点写 `qMakePair(a, b)` 就该直接命中，不必再多一层宏。
// ---------------------------------------------------------------------------

template <typename A, typename B>
using PkPair = std::pair<A, B>;

// Qt5 的 qMakePair 按值推导两个类型（不做 decay 之外的手脚），返回一个新 pair。
// 与 std::make_pair 的区别只在名字，语义一致。

// R 线让位守卫（R-35/R-37/R-38 同型，S-08 主树 flake 链接验证压出）：真 Qt
// qpair.h 的 qMakePair 是 `template <typename T1, typename T2> QPair<...>
// qMakePair(const T1 &, const T2 &)` —— 参数列表与本文件逐形同、只差返回类型
// （QPair 对 PkPair），两组 inline 自由函数模板在同一翻译单元里是硬重定义。
// 真 Qt 的 qpair.h 在场时本文件让位，由真 Qt 版本覆盖（其返回 QPair 与 std::pair
// 在 Qt5 是不同类，但混合 TU 里调用点走真 Qt 语义、自洽）。薄壳（QT_CORE_LIB
// 未定义）与主树纯 Pk TU（QT_CORE_LIB 定义但 qpair.h 不在场）由第二析取让位
// 条件继续由本文件提供。
#if !defined(QT_CORE_LIB) || !defined(QPAIR_H)

template <typename A, typename B>
PkPair<A, B> qMakePair(const A &a, const B &b)
{
    return PkPair<A, B>(a, b);
}

#endif  // !defined(QT_CORE_LIB) || !defined(QPAIR_H)
