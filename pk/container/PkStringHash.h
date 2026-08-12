#pragma once

#include "PkHashFunctions.h"

#include "PkString.h"

// ---------------------------------------------------------------------------
// qHash(const PkString &) —— 让 PkHash<PkString, V> 与 PkSet<PkString> 能用。
//
// **这条重载刻意住在 pk/container/，不去改 pk/string/。** 在自己的目录里给
// 别人的类型加一个自由函数重载是合法的，而 pk/string 是另一个任务的地盘
// （R-13 才动它）；把它塞过去等于越界改别人的交付件。
//
// 找得到的机制：PkString 在全局命名空间，`qHash(k)` 在 PkHasher 里是依赖调用，
// 实例化点的 ADL 把全局命名空间纳入关联集合——所以只要用到 PkHash<PkString,V>
// 的那个翻译单元 include 了本文件，就能命中。这与 Krita 那 18 处自定义
// `uint qHash(const KoID &)` 被找到的机制**完全相同**，因此本文件也是那条链路
// 的一份可编译的样例。
//
// 算法用 FNV-1a（32 位）逐码元推。**不要求与 Qt 的 qHash(QString) 逐位相同**
// ——哈希数值在 Qt 里也不可观察（QHash 迭代顺序未定义）。要求相同的是签名形状。
//
// 只用 PkString 的公开 API（size() / at()）：`_cdata()` 是 private，而且从
// pk/container 去戳别人的私有实现，正是"越界"的另一种写法。at() 逐个取码元
// 不额外分配，比 PkToU16() 拷一份出来更合适。
// ---------------------------------------------------------------------------

inline unsigned int qHash(const PkString &key, unsigned int seed = 0) noexcept
{
    unsigned int h = 2166136261u ^ seed;
    const int n = key.size();
    for (int i = 0; i < n; ++i) {
        h ^= static_cast<unsigned int>(key.at(i));
        h *= 16777619u;
    }
    return h;
}
