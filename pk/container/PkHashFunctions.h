#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// ---------------------------------------------------------------------------
// qHash 自由函数族 + PkHasher —— PkHash/PkSet 求哈希的入口。
//
// **为什么不是 std::hash**：Qt 的 QHash<K,V>/QSet<T> 通过自由函数 `qHash(K)` 求
// 哈希，而 Krita 全仓有 **18 处自定义 qHash 重载定义**（实测：`qHash` 共 42 处
// 调用/定义，其中 18 处是形如 `uint qHash(const KoID &id)` 的重载定义）。这些
// 定义散落在 Krita 自己的头文件里，**S 线替换调用点时原样保留**——改用
// std::hash 就等于把这 18 处全部作废，每一处都要人工改写成 std::hash 特化。
//
// 所以默认 hasher 转调 `qHash(k)`，靠**非限定名字查找 + ADL** 命中它们：
//
//   - 内建类型（int/double/指针/…）没有关联命名空间，ADL 找不到东西，
//     全靠**本文件在 PkHasher 定义点之前声明的这批重载**（模板定义点的
//     普通查找）。所以这批重载必须写在 PkHasher **上面**，顺序不能调。
//   - Krita 自己的类型（KoID/KisNodeSP/…）在全局命名空间，ADL 在实例化点
//     把全局命名空间纳入关联集合，于是能找到 Krita 那 18 处重载 —— 哪怕它们
//     的头文件是在本文件之后才被 include 的。
//
// **哈希值不要求与 Qt 逐位相同**：QHash/QSet 的迭代顺序在 Qt 里本就未定义，
// 没有任何调用点能观察到具体的哈希数值。要求相同只会凭空多一条对不上的约束。
// 要求相同的是**签名形状**——单实参可调用、返回无符号整数，因为 Krita 那 18 处
// 写的就是 `uint qHash(const X &)`。
//
// 返回类型写 `unsigned int` 而不是 `uint`：本工程零 Qt，没有 <QtGlobal> 的
// typedef；`uint` 由 compat 垫片在调用点侧提供，与本文件无关。
// ---------------------------------------------------------------------------

// 所有整型先折算成 64 位再混合成 32 位，避免为每个宽度各写一套混合逻辑。
inline unsigned int pkHashMix64(unsigned long long key, unsigned int seed) noexcept
{
    // 高低 32 位异或后再与 seed 异或。对齐 Qt 对 quint64 的做法
    // （`uint(((key >> 32) ^ key) & 0xffffffff) ^ seed`）。
    return static_cast<unsigned int>(((key >> 32) ^ key) & 0xffffffffULL) ^ seed;
}

inline unsigned int qHash(bool key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key ? 1u : 0u) ^ seed;
}

inline unsigned int qHash(char key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(static_cast<unsigned char>(key)) ^ seed;
}

inline unsigned int qHash(signed char key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(static_cast<unsigned char>(key)) ^ seed;
}

inline unsigned int qHash(unsigned char key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key) ^ seed;
}

inline unsigned int qHash(short key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(static_cast<unsigned short>(key)) ^ seed;
}

inline unsigned int qHash(unsigned short key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key) ^ seed;
}

inline unsigned int qHash(int key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key) ^ seed;
}

inline unsigned int qHash(unsigned int key, unsigned int seed = 0) noexcept
{
    return key ^ seed;
}

inline unsigned int qHash(long key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(static_cast<unsigned long long>(static_cast<unsigned long>(key)), seed);
}

inline unsigned int qHash(unsigned long key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(static_cast<unsigned long long>(key), seed);
}

inline unsigned int qHash(long long key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(static_cast<unsigned long long>(key), seed);
}

inline unsigned int qHash(unsigned long long key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(key, seed);
}

inline unsigned int qHash(char16_t key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key) ^ seed;
}

inline unsigned int qHash(char32_t key, unsigned int seed = 0) noexcept
{
    return static_cast<unsigned int>(key) ^ seed;
}

inline unsigned int qHash(wchar_t key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(static_cast<unsigned long long>(key), seed);
}

// 浮点：**-0.0 与 +0.0 必须给出同一个哈希**，否则 `s.contains(-0.0)` 会在
// `s.insert(0.0)` 之后为假，而 `-0.0 == 0.0` 为真——哈希容器的不变量当场破掉。
// Qt 的做法是 `key != 0.0 ? 位模式哈希 : seed`，负零走 else 分支，我们照抄。
// 位模式用 memcpy 取（reinterpret_cast 一个按值形参是别名规则上的灰区）。
inline unsigned int qHash(float key, unsigned int seed = 0) noexcept
{
    if (key == 0.0f) {
        return seed;
    }
    unsigned int bits = 0;
    std::memcpy(&bits, &key, sizeof(bits));
    return bits ^ seed;
}

inline unsigned int qHash(double key, unsigned int seed = 0) noexcept
{
    if (key == 0.0) {
        return seed;
    }
    unsigned long long bits = 0;
    std::memcpy(&bits, &key, sizeof(bits));
    return pkHashMix64(bits, seed);
}

// 指针：按地址值哈希。低位在对齐的分配里恒为 0，先右移三位再混合。
template <typename T>
inline unsigned int qHash(const T *key, unsigned int seed = 0) noexcept
{
    return pkHashMix64(static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(key)) >> 3,
                       seed);
}

// 枚举：折算到底层整型。没有这一条的话 PkHash<某枚举, V> 会在上面那一堆整型
// 重载之间报歧义（枚举到每个整型都是同等的转换），而 QHash<某枚举, V> 是
// Krita 里常见的写法。
template <typename Enum>
inline typename std::enable_if<std::is_enum<Enum>::value, unsigned int>::type
qHash(Enum key, unsigned int seed = 0) noexcept
{
    return qHash(static_cast<typename std::underlying_type<Enum>::type>(key), seed);
}

// ---------------------------------------------------------------------------
// PkHasher —— 交给 std::unordered_map / std::unordered_set 的 Hash 策略。
//
// 必须定义在上面那批重载**之后**：`qHash(k)` 里的 k 是依赖类型，所以查找分两段
// ——模板定义点的普通查找（管内建类型）+ 实例化点的 ADL（管 Krita 自己的类型）。
// 把本结构挪到文件开头，内建类型那一半就会当场失效。
//
// **只用一个实参调用**，不传 seed：Krita 那 18 处自定义重载写的是
// `uint qHash(const KoID &id)`，没有第二个形参。
// ---------------------------------------------------------------------------
template <typename K>
struct PkHasher
{
    std::size_t operator()(const K &k) const
    {
        return static_cast<std::size_t>(qHash(k));
    }
};
