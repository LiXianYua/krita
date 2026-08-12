#pragma once

#include "PkJavaIterator.h"
#include "PkMap.h"

#include <cassert>

// ---------------------------------------------------------------------------
// PkMap 的两个 Java 风格迭代器。归属照 Qt（<QMap> 提供 QMapIterator 与
// QMutableMapIterator，调用点写的是 `#include <QMap>`）。
// ---------------------------------------------------------------------------

// 只读版：20 处声明、hasNext 17 · next 17 · key 21 · value 21。
// 实现全在 PkJavaAssocConstIterator（PkJavaIterator.h）。
template <typename K, typename V>
using PkMapIterator = PkJavaAssocConstIterator<PkMap<K, V>>;

// ---------------------------------------------------------------------------
// PkMutableMapIterator<K,V> —— QMutableMapIterator<K,V> 的替代品。
//
// **保留范围内只有一处调用点**，全文抄在这里，因为本类的方法面就是照它定的：
//
//   libs/flake/svg/SvgStyleParser.cpp:527
//     QMutableMapIterator<QString, QString> it(styleMap);
//     while (it.hasNext()) {
//         it.next();
//         if (it.value() == "inherit") {
//             it.setValue(inheritedAttribute(it.key(), e));
//         }
//     }
//
// 于是方法面 = hasNext · next · key · value · setValue，**一项不多一项不少**：
// remove / toFront / toBack / hasPrevious / previous / peekNext / peekPrevious /
// findNext / findPrevious 全 0 处，不做。
//
// ---- 与 Qt 的两条已知偏离，都是为了守住 COW，列在这里备查 ----
//
// ① **next() 返回 void**，Qt 返回 `Item`（= QMap<K,T>::iterator，**可写**）。
//    唯一调用点是 `it.next();` 单独成句，不用返回值。给一个可写 Item 出去就等于
//    给了一条绕开 PkMut() 的写路径 —— 拿着它写值，容器若在迭代期间被拷走变回
//    共享态，写入会同时污染另一份。收益为零、风险实在，所以不给。
//    （只读版 PkMapIterator::next() 照常返回 Item，那是 const_iterator，无此问题。）
//
// ② **value() 返回 const V&**，Qt 返回 `T&`。同一条理由：唯一调用点只做比较
//    （`it.value() == "inherit"`）。要给可写引用就得经 operator[] 走 PkMut()，
//    而那一步可能 detach，会把本对象的两个游标当场作废 —— 为一个没人用的可写性
//    引入一整套"写后重建游标"的机制，不划算。
//
// ---- setValue 怎么在不破 COW 的前提下写回 ----
//
// 游标是 const_iterator（constBegin/constEnd → PkConst()，绝不 detach），
// 写值必须经容器的公开写入口 PkMap::insert()（内部 PkMut()）。麻烦在于 insert
// 若真的 detach 了，两个游标就指向被丢弃的老缓冲区。
//
// 解法：先把 key 抄出来，写完再按 key 重新定位 —— 这与 PkAssocContainer::erase
// 处理同一问题的手法一致（那里的注释写着"先把 key 抄一份出来，detach 之后按 key
// 重新 find"）。本类没有 previous()，所以 m_i **恒等于 m_n 的后继**，重建时
// `m_i = m_n; ++m_i;` 是精确的，不是近似。
//
// 代价是每次 setValue 多一次 O(log n) 查找。唯一调用点一轮最多调一次，可忽略。
// 换来的是**比 Qt 更强**的保证：迭代期间容器被拷走再写，我们照样正确 detach，
// Qt5 得靠 setSharable(false) 把容器钉死才做得到（我们没有那个等价物）。
// ---------------------------------------------------------------------------

template <typename K, typename V>
class PkMutableMapIterator
{
public:
    using PkContainer = PkMap<K, V>;
    using PkCursor = typename PkContainer::const_iterator;

    // 收非 const 引用（要能改原容器）。不加 explicit，与其余几族一致。
    PkMutableMapIterator(PkContainer &container)
        : m_c(&container), m_i(container.constBegin()), m_n(container.constEnd())
    {
    }

    bool hasNext() const { return m_i != m_c->constEnd(); }

    // 返回 void —— 偏离①，理由见类头。
    void next()
    {
        assert(hasNext());
        m_n = m_i;
        ++m_i;
    }

    const K &key() const
    {
        assert(m_n != m_c->constEnd());
        return m_n.key();
    }

    // 返回 const V& —— 偏离②，理由见类头。
    const V &value() const
    {
        assert(m_n != m_c->constEnd());
        return m_n.value();
    }

    void setValue(const V &t)
    {
        assert(m_n != m_c->constEnd());
        // key **和** value 都先抄成值：insert() 可能 detach（丢弃老缓冲区），
        // 而 t 完全可能是一个指向本容器内部的引用（`it.setValue(it.value())`
        // 这类写法），detach 之后它就悬垂了。抄一份是最省事的堵法。
        const K wanted = m_n.key();
        const V newValue = t;
        m_c->insert(wanted, newValue);
        // 重建游标。m_i 恒为 m_n 的后继（本类没有 previous()），所以这是精确的。
        m_n = m_c->constFind(wanted);
        m_i = m_n;
        ++m_i;
    }

private:
    PkContainer *m_c;
    PkCursor m_i;
    PkCursor m_n;
};
