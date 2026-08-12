#pragma once

#include <cassert>

// ---------------------------------------------------------------------------
// Java 风格迭代器的**只读**两族共同实现。
//
// Qt 有两套迭代器：STL 风格（begin()/end()，容器自己给）与 Java 风格
// （`QListIterator<T> it(list); while (it.hasNext()) use(it.next());`）。
// 后者是独立的类模板，不是容器的成员类型 —— 所以要单独造。
//
// ---- 只读迭代器持**拷贝**，可变迭代器持**指针** ----
//
// 这是 Qt 的设计，也是两族最根本的差别：
//   QListIterator<T>        成员是 `QList<T> c;`   → 构造后修改原容器，迭代不受影响
//   QMutableListIterator<T> 成员是 `QList<T> *c;`  → remove() 真的改到原容器
// 照做，并在单测里正反两面各压一遍（pkJavaReadOnlyIteratorHoldsCopy /
// pkJavaMutableListIteratorRemove）。持拷贝之所以不贵，还是因为 PkArrayData
// 的拷贝是 O(1)。
//
// ---- 本文件只放"只读"两族，可变的两个各自跟着容器走 ----
//
// PkMutableListIterator 在 PkListIterator.h、PkMutableMapIterator 在
// PkMapIterator.h —— 可变迭代器要经容器的公开写入口做事（才不破 COW），
// 那些入口是容器专有的（PkList::removeAt / PkMap::insert），抽不成一份。
//
// ---- 方法面：逐处核实过的真实调用点，一项不多一项不少 ----
//
// 口径：`git ls-files` 的 C/C++ 源文件，先按类型名找出用它的文件，抽出该类型的
// 变量名，再要求 `.方法(` / `->方法(` 紧邻的 token 属于该变量集合；排除函数名与
// 实参名造成的误命中（KoProperties.cpp 的 `properties`、KoFontStorage.cpp 的
// `collectRepresentations`、KoProperties.h 的 `propertyIterator` 三个）；
// 落在删除范围（plugins/dockers|extensions|platforms|qt、krita/、libs/libkis、
// qmlmodules/、libs/ui）里的调用点**不算数**。
//
//   PkListIterator（QListIterator，保留范围 13 处声明）：
//     hasNext 6 · next 6 · peekNext 1 · hasPrevious 2 · previous 1 ·
//     peekPrevious 1 · toBack 1 · toFront 0
//   PkVectorIterator（QVectorIterator，7 处声明）：
//     hasNext 7 · next 5 · hasPrevious 2 · previous 2 · peekPrevious 1 ·
//     toFront 1 · toBack 2 · peekNext 0
//
// 两个类共用一份实现，取**并集**：8 个方法每一个都至少有一处真实调用点
// （toFront 由 kundo2stack.cpp:404 撑着，peekNext 由 KoRuler.cpp:661 撑着）。
// 为了各自砍掉对方那一项而写两份实现，只会制造一个"改一处忘一处"的漏洞面。
//
//   PkMapIterator（QMapIterator，20 处声明）：hasNext 17 · next 17 · key 21 · value 21
//   PkHashIterator（QHashIterator，5 处声明）：hasNext 5 · next 4 · key 3 · value 4
//
// 两族**其余 Qt 方法一律不做**（保留范围全 0 处）：关联侧的
// toFront/toBack/hasPrevious/previous/peekNext/peekPrevious/findNext/findPrevious，
// 以及两族的 `operator=(const 容器 &)`（调用点只有"用容器拷贝初始化"
// ——那是构造函数，如 kedittoolbar.cpp:960 `QListIterator<...> it = clients;`
// ——和"用另一个迭代器赋值"，那是隐式的拷贝赋值）。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// PkJavaSeqConstIterator<C> —— PkListIterator / PkVectorIterator 的实现。
//
// 游标用**下标**而不是内层迭代器：`c` 是一份 COW 拷贝，谁都改不动它，用下标
// 与用迭代器等价；而下标让 at() 成为唯一的元素访问入口，at() 只有 const 重载
// （PkArrayContainer 没给非 const 的 at），走的必然是 PkConst() —— 也就是说
// **这个类不可能意外 detach**。用迭代器的话，一个手滑写成 c.begin() 就悄悄
// detach 了。
//
// 游标语义照 Qt：它落在**元素之间**，不是落在元素上。i == 0 表示在第一个元素
// 之前，i == size() 表示在最后一个元素之后。next() 返回游标右边那个并右移，
// previous() 左移再返回左边那个。
// ---------------------------------------------------------------------------

template <typename C>
class PkJavaSeqConstIterator
{
public:
    using PkValue = typename C::value_type;

    // **不加 explicit**：调用点有 `QListIterator<KisKXMLGUIClient *> it = clients;`
    // （libs/widgetutils/xmlgui/kedittoolbar.cpp:960）这种拷贝初始化写法，
    // explicit 会当场编不过。
    PkJavaSeqConstIterator(const C &container) : m_c(container), m_i(0) {}

    void toFront() { m_i = 0; }
    void toBack() { m_i = m_c.size(); }

    bool hasNext() const { return m_i < m_c.size(); }
    // 返回 const 引用（Qt 的 QListIterator::next() 同样是 `const T &`）。
    // 引用指向 m_c 的缓冲区，与本对象同寿。
    const PkValue &next()
    {
        assert(hasNext());
        return m_c.at(m_i++);
    }
    const PkValue &peekNext() const
    {
        assert(hasNext());
        return m_c.at(m_i);
    }

    bool hasPrevious() const { return m_i > 0; }
    const PkValue &previous()
    {
        assert(hasPrevious());
        return m_c.at(--m_i);
    }
    const PkValue &peekPrevious() const
    {
        assert(hasPrevious());
        return m_c.at(m_i - 1);
    }

private:
    // 一份 COW 拷贝（O(1)）。拷贝构造/拷贝赋值用隐式的就够：m_c 的拷贝仍与
    // 原来那份共享同一缓冲区，m_i 是纯下标、跟着一起拷贝也仍然正确。
    // 调用点 libs/image/kis_base_node.cpp:58 靠拷贝构造
    // （`QMapIterator<...> iter = rhs.properties.propertyIterator();` 的序列侧同型写法）。
    C m_c;
    int m_i;
};

// ---------------------------------------------------------------------------
// PkJavaAssocConstIterator<C> —— PkMapIterator / PkHashIterator 的实现。
//
// 关联容器没有下标，游标只能是内层迭代器。**两个游标**：
//   m_i = 下一个待返回的项（Qt 的 `i`）
//   m_n = 上一次 next() 返回的项（Qt 的 `n`）
// key()/value() 读的是 **m_n**，不是 m_i —— 这是本类最容易实现错的一格，
// 而所有 42 处 key()/value() 调用点都长成这个样子：
//     while (it.hasNext()) { it.next(); use(it.key(), it.value()); }
// 读成 m_i 的话整体错位一个元素，最后一轮还会解引用 end()。
//
// 两个游标都取自 constBegin()/constEnd()（PkConst()）→ **绝不 detach**。
// m_c 是 COW 拷贝，拷贝本对象时 m_c 与源共享同一缓冲区，m_i/m_n 指进去仍然
// 有效 —— Qt 的 QMapIterator 靠的是同一条性质（隐式共享 + 迭代器指向共享块）。
// 调用点两种写法都要靠它：
//   libs/widgetutils/KoProperties.cpp:38  按值返回一个 QMapIterator
//   plugins/impex/libkra/kis_kra_savexml_visitor.cpp:186  `i = QMapIterator<...>(...)` 拷贝赋值
// ---------------------------------------------------------------------------

template <typename C>
class PkJavaAssocConstIterator
{
public:
    using PkKey = typename C::key_type;
    using PkMapped = typename C::mapped_type;
    // Qt 的名字：QMapIterator<K,T>::Item 就是 QMap<K,T>::const_iterator。
    // next() 返回它。**保留范围内没有一处调用点用这个返回值**（全是
    // `it.next();` 单独成句），给出来纯粹是因为它零风险、也零额外实现
    // ——const_iterator 是容器已经有的类型。
    using Item = typename C::const_iterator;

    // 不加 explicit：调用点有 `QMapIterator<QAction *, int> it = contextIconSizes;`
    // （libs/widgetutils/xmlgui/ktoolbar.cpp:638）。
    PkJavaAssocConstIterator(const C &container)
        : m_c(container), m_i(m_c.constBegin()), m_n(m_c.constEnd())
    {
    }

    bool hasNext() const { return m_i != m_c.constEnd(); }

    Item next()
    {
        assert(hasNext());
        m_n = m_i;
        ++m_i;
        return m_n;
    }

    // Qt 下 next() 之前调 key()/value() 是 UB（n 还等于 end()）。
    // 「Qt 是 UB」不等于「我们可以随便做」——它等于「对拍证明不了这一格」，
    // 所以由 assert 兜住（NDEBUG 下与 Q_ASSERT 一样消失）。
    const PkKey &key() const
    {
        assert(m_n != m_c.constEnd());
        return m_n.key();
    }
    const PkMapped &value() const
    {
        assert(m_n != m_c.constEnd());
        return m_n.value();
    }

private:
    C m_c;
    Item m_i;
    Item m_n;
};
