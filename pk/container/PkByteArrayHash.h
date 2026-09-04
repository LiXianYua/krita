#pragma once

#include "PkHashFunctions.h"

#include "PkByteArray.h"

// ---------------------------------------------------------------------------
// pkHash(const PkByteArray &) —— 让 PkHash<PkByteArray, V> / PkSet<PkByteArray> 能用。
//
// 与 PkStringHash.h 同一条思路：容器头里不该塞具体类型的哈希，在自己的目录里给
// 别人的类型加一个自由函数重载是合法的。找得到的机制也一样——PkByteArray 在全局
// 命名空间，`pkHash(k)` 在 PkHasher 里是依赖调用，实例化点的 ADL 把全局命名空间
// 纳入关联集合，所以只要用到 PkHash<PkByteArray,V> 的那个翻译单元 include 了
// 本文件就能命中。
//
// 算法用 FNV-1a（32 位）逐字节推，常数与 PkStringHash.h 同一套（两条哈希不必
// 彼此相等，但同一套常数便于对照）。**不要求与 Qt 的 qHash(QByteArray) 逐位相同**
// ——哈希数值不可观察（容器迭代顺序未定义），要求相同的是签名形状。
//
// 只用公开 API（data() / size()），不拷一份出来：data() 直接给内部缓冲，逐字节
// 走一遍不额外分配。空数组时 size()==0，循环不进，返回初始常数 ^ seed。
// ---------------------------------------------------------------------------

inline unsigned int pkHash(const PkByteArray &key, unsigned int seed = 0) noexcept
{
    unsigned int h = 2166136261u ^ seed;
    const int n = key.size();
    const char *d = key.data();
    for (int i = 0; i < n; ++i) {
        h ^= static_cast<unsigned char>(d[i]);
        h *= 16777619u;
    }
    return h;
}
