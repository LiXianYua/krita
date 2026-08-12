#pragma once

#include "PkAssocContainer.h"
#include "PkHashFunctions.h"

#include <initializer_list>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------------
// PkHash<K,V> —— Qt5 QHash<K,V> 的替代品。COW，内层
// std::unordered_map<K, V, PkHasher<K>>。
//
// 共同 API 全在 PkAssocContainer<K, V, …, PkHash<K,V>> 里（PkMut() 是唯一写
// 入口、PkConst() 绝不 detach、拷贝 O(1)、**迭代器解引用得 value 不是 pair**）。
// **QHash 没有 lowerBound/upperBound**——那是 QMap 靠有序性给的，本类不给。
// 也就是说本文件除了内层类型与构造函数，没有自己的方法。
//
// **无序**：迭代顺序在 Qt 里本就未定义，单测只断言集合相等（排序后比较），
// 不断言顺序。
//
// **哈希靠 qHash 自由函数 + ADL**，机制与理由见 PkHashFunctions.h 的类头。
// 一句话：Krita 全仓 18 处自定义 `uint qHash(const X &)` 重载在 S 线替换调用点
// 时原样保留，PkHash 必须能找到它们，所以 hasher 转调 `qHash(k)` 而不是
// std::hash。
//
// K 还要求 operator==（std::equal_to<K>）——QHash 同样要求。
//
// **QMultiHash 不做**（9 处真实声明，归 S 线按点改写）。
// ---------------------------------------------------------------------------

template <typename K, typename V>
class PkHash
    : public PkAssocContainer<K, V, std::unordered_map<K, V, PkHasher<K>>, PkHash<K, V>>
{
    using PkBase =
        PkAssocContainer<K, V, std::unordered_map<K, V, PkHasher<K>>, PkHash<K, V>>;
    using PkInner = typename PkBase::PkInner;

public:
    using iterator = typename PkBase::iterator;
    using const_iterator = typename PkBase::const_iterator;

    PkHash() = default;

    PkHash(std::initializer_list<std::pair<K, V>> args)
    {
        PkInner &m = this->m_d.PkMut();
        for (const auto &entry : args) {
            m.insert_or_assign(entry.first, entry.second);
        }
    }

    // 理由与 PkMap 同：声明了移动构造就会把隐式拷贝 deleted 掉，
    // 而拷贝 O(1) 是 2286 处 Q_FOREACH 的命根子。
    ~PkHash() = default;
    PkHash(const PkHash &) = default;
    PkHash &operator=(const PkHash &) = default;
    PkHash(PkHash &&) = default;
    PkHash &operator=(PkHash &&) = default;
};
