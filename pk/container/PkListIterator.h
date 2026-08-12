#pragma once

#include "PkJavaIterator.h"
#include "PkList.h"

#include <cassert>

// ---------------------------------------------------------------------------
// PkList 的两个 Java 风格迭代器。
//
// 归属照 Qt：`QListIterator` / `QMutableListIterator` 是 <QList> 提供的，调用点
// 写的就是 `#include <QList>` 然后用它们 —— 所以 compat/QList 垫片同时映射这
// 三个名字，而实现放在本文件（与 PkList.h 分开只是为了不让不用迭代器的 TU
// 白白多编一份模板）。
// ---------------------------------------------------------------------------

// 只读版：别名模板即可，实现全在 PkJavaSeqConstIterator。
//
// 用别名而不是派生类：派生要把构造函数转发一遍，而
// `QListIterator<T> it = someList;` 这种拷贝初始化对转发是否 explicit 很敏感
// ——别名没有这个面。前置声明的风险不存在：全仓 0 处 `class QListIterator;`。
template <typename T>
using PkListIterator = PkJavaSeqConstIterator<PkList<T>>;

// ---------------------------------------------------------------------------
// PkMutableListIterator<T> —— QMutableListIterator<T> 的替代品。
//
// 保留范围内 8 处声明（另 3 处落在 libs/ui、plugins/dockers，不算数），
// 方法用量逐处核实：
//   hasNext 7 · next 10 · peekNext 4 · hasPrevious 3 · previous 3 ·
//   toBack 3 · remove 7
// **toFront / peekPrevious / setValue / value / insert / findNext 全 0 处，不做。**
// （对比只读版给了 toFront/peekPrevious —— 那两个在只读族里有真实调用点，
//   这里没有。这不是笔误，是判据①「一项不多一项不少」逐族算的结果。）
//
// 典型调用点（三种形态都在，别漏）：
//   libs/image/kis_simple_update_queue.cpp:251  iter.toBack(); while(iter.hasPrevious()) { item = iter.previous(); ... iter.remove(); }
//   libs/widgetutils/xmlgui/kxmlguifactory_p.cpp:332  QMutableListIterator<MergingIndex> cmIt = mergingIndices;   ← 从**非 const** 容器拷贝初始化
//   libs/widgetutils/xmlgui/kxmlguifactory_p.cpp:75   void removeChild(QMutableListIterator<ContainerNode *> &childIterator)   ← 按引用传参
//   libs/widgetutils/xmlgui/kxmlguifactory_p.cpp:78   delete childIterator.next();    ← next() 的返回值**被使用**，必须是 T&
//
// ---- 为什么持指针 + 下标，而不是持容器迭代器 ----
//
// Qt 的 QMutableListIterator 存 `QList<T> *c; iterator i, n;`，构造时就
// `c->begin()`（=detach）并调 setSharable(false) 把容器钉成不可共享。我们没有
// setSharable 的等价物，而 std::vector 的迭代器在 detach（换缓冲区）与 erase
// 之后都会失效 —— 存迭代器等于给自己埋一串悬垂。
//
// 存**下标**则完全没有这个面：每次访问都经容器的公开写入口
// （operator[] / removeAt），也就是每次都经 PkMut()。这比 Qt 更强
// ——即使迭代期间容器被别人拷走变回共享态，下一次写仍会正确 detach。
//
// 游标语义与只读版一致（落在元素之间）；m_n 记的是"上一次 next()/previous()
// 返回的那个元素的下标"，-1 表示无效 —— Qt 用 `n == c->end()` 表达同一件事，
// remove() 只在它有效时才动手。
// ---------------------------------------------------------------------------

template <typename T>
class PkMutableListIterator
{
public:
    // 收**非 const** 引用（要能改原容器），且不加 explicit
    // （kxmlguifactory_p.cpp:332/366 是拷贝初始化写法）。
    PkMutableListIterator(PkList<T> &container) : m_c(&container), m_i(0), m_n(-1) {}

    void toBack()
    {
        m_i = m_c->size();
        m_n = -1;
    }

    bool hasNext() const { return m_i < m_c->size(); }

    // 返回 T&（Qt 同样）：kxmlguifactory_p.cpp:78 `delete childIterator.next();`
    // 与 :334 `cmIt.next().clientName` 都在用这个返回值。
    T &next()
    {
        assert(hasNext());
        m_n = m_i++;
        return (*m_c)[m_n];
    }

    T &peekNext() const
    {
        assert(hasNext());
        return (*m_c)[m_i];
    }

    bool hasPrevious() const { return m_i > 0; }

    T &previous()
    {
        assert(hasPrevious());
        m_n = --m_i;
        return (*m_c)[m_n];
    }

    // 删掉"上一次 next()/previous() 返回的那个"。没有有效的上一次就什么都不做
    // （Qt 的 `if (c->constEnd() != const_iterator(n))` 同义）。
    //
    // 删完游标落回被删元素的位置：next() 之后 m_i == m_n + 1，删掉 m_n 那格后
    // 原本 m_i 指的元素前移一位，游标应当是 m_n；previous() 之后 m_i == m_n，
    // 本来就等于 m_n。两条合起来就是 `m_i = m_n`（Qt 的 `i = c->erase(n)` 同义）。
    void remove()
    {
        if (m_n < 0) {
            return;
        }
        m_c->removeAt(m_n);
        m_i = m_n;
        m_n = -1;
    }

private:
    PkList<T> *m_c;
    int m_i;
    int m_n;
};
