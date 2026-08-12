#pragma once

#include "PkAssocContainer.h"

#include <initializer_list>
#include <map>
#include <utility>

// ---------------------------------------------------------------------------
// PkMap<K,V> —— Qt5 QMap<K,V> 的替代品。COW，内层 std::map<K,V>。
//
// 共同 API（value/insert/take/remove/keys/values/迭代器/…）全在
// PkAssocContainer<K, V, std::map<K,V>, PkMap<K,V>> 里，见那个文件的类头注释：
// PkMut() 是唯一写入口、PkConst() 绝不 detach、拷贝 O(1)、**迭代器解引用得
// value 不是 pair**。**本文件只放 QMap 专有的两项**：lowerBound / upperBound。
//
// **按 key 有序**（实测 Qt：`QMap keys 顺序 = 1,2,3`），K 要求 operator<
// ——QMap 同样要求。lowerBound/upperBound 就是靠这条有序性才有意义。
//
// 全局 Pk 前缀、不进 C++ namespace：compat 垫片靠 `#define QMap PkMap`，
// 而 Krita 里有 `template <class Key, class T> class QMap;` 一类前置声明
// ——套进 namespace 这个技巧就废了。
//
// **QMultiMap 不做**（11 处真实声明，归 S 线按点改写）；`insertMulti`
// 实测 0 处调用点（4 个 grep 命中全是 insertMultipleKeyframes 误报），
// 与 uniqueKeys/equal_range/toSet/fromSet/isDetached/isSharedWith 一同不做。
// ---------------------------------------------------------------------------

template <typename K, typename V>
class PkMap : public PkAssocContainer<K, V, std::map<K, V>, PkMap<K, V>>
{
    using PkBase = PkAssocContainer<K, V, std::map<K, V>, PkMap<K, V>>;
    using PkInner = typename PkBase::PkInner;

public:
    using iterator = typename PkBase::iterator;
    using const_iterator = typename PkBase::const_iterator;

    PkMap() = default;

    // QMap<K,V>(std::initializer_list<std::pair<K,V>>)：Qt 5.1 起就有。
    PkMap(std::initializer_list<std::pair<K, V>> args)
    {
        PkInner &m = this->m_d.PkMut();
        for (const auto &entry : args) {
            m.insert_or_assign(entry.first, entry.second);
        }
    }

    // 五个特殊成员全部显式 = default：一旦声明了移动构造，隐式的拷贝构造与
    // 拷贝赋值就会被定义为 deleted（[class.copy.ctor]/8），而「拷贝 O(1)」是
    // 2286 处 Q_FOREACH 的命根子。移动本身直接 = default 就够——「移动之后源是
    // 空且完全可用的容器」这条 Qt 语义由 PkArrayData 兜住。
    ~PkMap() = default;
    PkMap(const PkMap &) = default;
    PkMap &operator=(const PkMap &) = default;
    PkMap(PkMap &&) = default;
    PkMap &operator=(PkMap &&) = default;

    // ---- QMap 专有：靠 std::map 的有序性 ----
    //
    // lowerBound(k)：第一个 **>= k** 的项；upperBound(k)：第一个 **> k** 的项。
    // 都找不到就是 end()（实测 Qt：表里是 {1,3} 时 `lowerBound(2).key=3`、
    // `upperBound(2).key=3`、`lowerBound(0)==begin` 为真、`upperBound(9)==end`
    // 为真）。
    //
    // 非 const 版返回可写迭代器 → 按实测规则走 PkMut()（拿到可写迭代器即视为
    // 写）；const 版走 PkConst()，绝不 detach。

    iterator lowerBound(const K &key)
    {
        return iterator(this->m_d.PkMut().lower_bound(key));
    }

    const_iterator lowerBound(const K &key) const
    {
        return const_iterator(this->m_d.PkConst().lower_bound(key));
    }

    iterator upperBound(const K &key)
    {
        return iterator(this->m_d.PkMut().upper_bound(key));
    }

    const_iterator upperBound(const K &key) const
    {
        return const_iterator(this->m_d.PkConst().upper_bound(key));
    }
};
