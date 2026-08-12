#pragma once

#include "PkArrayData.h"
#include "PkList.h"

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// PkAssocIterator<InnerIt> —— **Qt 形状**的关联容器迭代器。
//
// 这是 Qt 与 STL 关联容器最根本的接口差异，也是本任务最容易整片编不过的一格：
//
//   |          | Qt QMap<K,V>::iterator | std::map<K,V>::iterator |
//   |----------|------------------------|-------------------------|
//   | `*it`    | **V&**（值）           | std::pair<const K,V>&   |
//   | `it->m`  | **V 的成员**           | pair 的成员（first/second）|
//   | 取 key   | **it.key()**           | it->first               |
//   | 取 value | **it.value()**         | it->second              |
//
// 所以 PkMap/PkHash **不能** `using iterator = typename std::map<K,V>::iterator`
// ——必须包一层。这不是纸上谈兵：R-02 的试接目标
// libs/image/tests/kis_fill_interval_map_test.cpp 与被测实现
// libs/image/floodfill/kis_fill_interval_map.cpp 用的正是 `QHash<int, QMap<int, POD>>`
// 嵌套 + `it->field`（解引用得 value 再取成员）+ `rowMap->insert(...)`（对
// QHash::iterator 用 operator-> 拿到里层 QMap 再调它的方法）+
// `QCOMPARE(beginIt, endIt)`（迭代器相等比较）。调研时第一次用 std::map 裸包
// 做垫片，就是在这个文件上编译失败的。
//
// **迭代器类别声明成 forward**：只提供前置/后置 ++，不提供 --。判据①「一项不多
// 一项不少」——`--it` 没有登记的调用点，给了就是凭空多一项。
// ---------------------------------------------------------------------------

template <typename InnerIt>
class PkAssocIterator
{
public:
    using PkInnerIterator = InnerIt;
    using PkPair = typename std::iterator_traits<InnerIt>::value_type; // std::pair<const K, V>
    using PkKeyType = typename PkPair::first_type;                     // const K
    using PkMappedType = typename PkPair::second_type;                 // V
    // 一份模板同时给出 iterator 与 const_iterator 两套，靠的就是这一行：
    // iterator 上 `it->second` 是 V 左值，const_iterator 上是 const V 左值。
    //
    // **外面那对括号不能省。** 不加括号时它是一个"未加括号的类成员访问"，
    // decltype 按规则给出**成员声明的类型**（`V`，不带引用），两套迭代器会得到
    // 同一个非引用类型，`*it` 于是变成按值返回——共享的容器再也写不进去，而且
    // const_iterator 那套的 pointer 会错成 `V*`。加了括号才是"表达式的类型"，
    // 得到 `V&` / `const V&`。
    using PkValueRef = decltype((std::declval<const InnerIt &>()->second));
    using PkValueType = typename std::remove_reference<PkValueRef>::type; // V 或 const V

    // STL traits：**value_type 是 V，不是 pair** —— 这正是与 std 的差异所在。
    using iterator_category = std::forward_iterator_tag;
    using value_type = PkMappedType;
    using difference_type = std::ptrdiff_t;
    using pointer = PkValueType *;
    using reference = PkValueRef;

    PkAssocIterator() = default;
    explicit PkAssocIterator(InnerIt it) : m_it(it) {}

    // iterator → const_iterator 的**隐式**转换（Qt 可以，调用点会依赖：
    // `QMap<K,V>::const_iterator ci = m.find(k);`）。反方向不成立，由
    // is_constructible 的约束自动挡住——std::map 的 iterator 不能由
    // const_iterator 构造。
    template <typename Other,
              typename std::enable_if<!std::is_same<Other, InnerIt>::value &&
                                          std::is_constructible<InnerIt, Other>::value,
                                      int>::type = 0>
    PkAssocIterator(const PkAssocIterator<Other> &o) : m_it(o.PkInner())
    {
    }

    PkValueRef operator*() const { return m_it->second; }
    pointer operator->() const { return &m_it->second; }

    const PkKeyType &key() const { return m_it->first; }
    PkValueRef value() const { return m_it->second; }

    PkAssocIterator &operator++()
    {
        ++m_it;
        return *this;
    }

    PkAssocIterator operator++(int)
    {
        PkAssocIterator tmp(*this);
        ++m_it;
        return tmp;
    }

    // 成员模板而不是两个定死类型的重载：`it == constEnd()` 与
    // `constBegin() == it` 两个方向都要能编（调用点两种写法都有）。
    // std::map / std::unordered_map 的 iterator 与 const_iterator 本来就可以
    // 直接互相比较，所以这里不需要先转换。
    template <typename Other>
    bool operator==(const PkAssocIterator<Other> &o) const
    {
        return m_it == o.PkInner();
    }

    template <typename Other>
    bool operator!=(const PkAssocIterator<Other> &o) const
    {
        return m_it != o.PkInner();
    }

    // 给容器实现取回内层迭代器。Pk 前缀 → 不在改名表里，垫片映射不到，
    // 调用点看不见它。
    InnerIt PkInner() const { return m_it; }

private:
    InnerIt m_it{};
};

// ---------------------------------------------------------------------------
// PkAssocContainer<K, V, Inner, Derived> —— PkMap<K,V> 与 PkHash<K,V> 的共同实现。
//
// Inner 是内层标准容器（std::map<K,V> / std::unordered_map<K,V,PkHasher<K>>）。
// 两者的公开面除了有序性与 lowerBound/upperBound 完全一致，抄两份的话任何一次
// 修正都要记得改两个地方，漏改的那一半就是一个静默的 COW 漏洞。
//
// **不套用 PkArrayContainer**：那是为序列容器写的（下标、连续内存、
// data()/resize()/indexOf），关联容器一项都用不上。
//
// CRTP（把 Derived 当模板参数）的理由：`operator==` / `operator!=` /
// `PkIsSharedWith` 的形参必须是**派生类**（`const PkMap<K,V> &`）——这是 Qt
// 的签名形状，也是这三个函数能读到 `other.m_d` 的前提。写成基类引用的话，
// `bool operator==(const PkAssocContainer &)` 会把"两个不同派生类型能不能比"
// 这件事交给基类是否相同来决定，而不是由类型本身说了算。
//
// （序列侧 PkArrayContainer 用 CRTP 的理由是另一条：`operator<<`/`+=`/`fill`
//  必须返回派生类引用才能链式写 `v << 1 << 2`。关联容器没有这类链式运算符。）
//
// ---- detach 时机：逐条按实测（真 Qt 5.15.7 与 5.15.13，两份输出一致）----
//
//   拷贝后                 x.isDetached = 0
//   调 x.begin()（非 const） x.isDetached = 1   ← 非 const begin() 触发 detach
//   调 x2.constBegin()      x2.isDetached = 0   ← constBegin() 不触发
//   const 对象上 begin()    x3.isDetached = 0   ← const 重载不触发
//
// 规则：**拿到可写迭代器 = 写操作**。于是
//   begin/end（非 const 重载）· find（非 const）· lowerBound/upperBound（非 const）
//     → 走 PkMut()
//   constBegin/constEnd/cbegin/cend · constFind · 全部 const 重载
//     → 走 PkConst()，**绝不** detach
//
// 四条硬要求（与 PkArrayData 的契约一致，不许各写各的）：
//
// 1. PkMut() 是**唯一**的写入口，漏用一个方法就是一个 COW 漏洞。
// 2. PkConst() **绝不** detach。
// 3. 拷贝构造/赋值必须 O(1)（只拷 shared_ptr）——2286 处 Q_FOREACH 靠它。
// 4. PkUseCount()/PkIsSharedWith() **只给单测用**，不进 compat 垫片。
//
// size() 返回 **int** 不是 size_t：Qt5 的口径，与序列侧同因。
// ---------------------------------------------------------------------------

template <typename K, typename V, typename Inner, typename Derived>
class PkAssocContainer
{
public:
    using PkInner = Inner;

    using key_type = K;
    using mapped_type = V;
    using iterator = PkAssocIterator<typename Inner::iterator>;
    using const_iterator = PkAssocIterator<typename Inner::const_iterator>;
    // Qt 的驼峰别名：调用点写 QMap<K,V>::Iterator 的地方靠它。
    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using size_type = int;

    // ---- 容量 ----

    int size() const noexcept { return static_cast<int>(m_d.PkConst().size()); }
    int count() const noexcept { return size(); }
    // count(key) 与无参 count() 是两个重载。单键容器下取值只能是 0 或 1。
    int count(const K &key) const
    {
        return static_cast<int>(m_d.PkConst().count(key));
    }

    bool isEmpty() const noexcept { return m_d.PkConst().empty(); }
    bool empty() const noexcept { return isEmpty(); }

    // ---- 读 ----

    bool contains(const K &key) const
    {
        const Inner &m = m_d.PkConst();
        return m.find(key) != m.end();
    }

    // key 不存在返回 V()（实测 Qt：`value(99)='' isEmpty=1`）。
    V value(const K &key) const
    {
        const Inner &m = m_d.PkConst();
        const auto it = m.find(key);
        return it == m.end() ? V() : it->second;
    }

    // key 不存在返回 def。
    V value(const K &key, const V &defaultValue) const
    {
        const Inner &m = m_d.PkConst();
        const auto it = m.find(key);
        return it == m.end() ? defaultValue : it->second;
    }

    // const 版**按值返回且不插入**（实测 Qt：`const operator[](777): size 4 -> 4`）。
    // Qt 的签名就是 `const T operator[](const Key &) const`。
    const V operator[](const K &key) const { return value(key); }

    // 非 const 版：**key 不存在时插入默认值并返回引用**（实测 Qt：
    // `非 const operator[](99): size 3 -> 4`）。
    //
    // **它看起来像读，其实是写** —— 这是本类最容易实现错的一格。必须走
    // PkMut()：共享状态下先 detach，否则一次下标取值就会把另一个容器也撑大。
    V &operator[](const K &key) { return m_d.PkMut()[key]; }

    // 反查：找不到返回 K() / defaultKey（实测 Qt：
    // `key("c")=3  key("zz")=0(默认)  key("zz",-5)=-5`）。
    K key(const V &value) const { return key(value, K()); }

    K key(const V &value, const K &defaultKey) const
    {
        for (const auto &entry : m_d.PkConst()) {
            if (entry.second == value) {
                return entry.first;
            }
        }
        return defaultKey;
    }

    PkList<K> keys() const
    {
        PkList<K> result;
        result.reserve(size());
        for (const auto &entry : m_d.PkConst()) {
            result.append(entry.first);
        }
        return result;
    }

    // keys(value)：所有映射到该 value 的 key。
    PkList<K> keys(const V &value) const
    {
        PkList<K> result;
        for (const auto &entry : m_d.PkConst()) {
            if (entry.second == value) {
                result.append(entry.first);
            }
        }
        return result;
    }

    PkList<V> values() const
    {
        PkList<V> result;
        result.reserve(size());
        for (const auto &entry : m_d.PkConst()) {
            result.append(entry.second);
        }
        return result;
    }

    // values(key)：Qt 下是多值容器的遗留接口，单键容器上结果是 0 或 1 个。
    PkList<V> values(const K &key) const
    {
        PkList<V> result;
        const Inner &m = m_d.PkConst();
        const auto it = m.find(key);
        if (it != m.end()) {
            result.append(it->second);
        }
        return result;
    }

    // ---- 写 ----

    // insert 已存在的 key 时**覆盖**旧值，不是多值（实测 Qt：
    // `insert 同 key 两次: size=1 value='y'`）。返回指向该项的 iterator（Qt 语义）。
    iterator insert(const K &key, const V &value)
    {
        Inner &m = m_d.PkMut();
        return iterator(m.insert_or_assign(key, value).first);
    }

    // 删掉几个（单键容器下是 0 或 1）。key 不存在返回 0（实测 Qt：`remove(555)=0`）。
    int remove(const K &key) { return static_cast<int>(m_d.PkMut().erase(key)); }

    // 取走并返回。key 不存在返回 V()（实测 Qt：`take(555)=''(空)`）。
    V take(const K &key)
    {
        Inner &m = m_d.PkMut();
        const auto it = m.find(key);
        if (it == m.end()) {
            return V();
        }
        V taken = std::move(it->second);
        m.erase(it);
        return taken;
    }

    void clear() { m_d.PkMut().clear(); }

    // erase(it) 返回**下一个**（实测 Qt：`erase(find(2)) 返回的迭代器 key=3`）。
    //
    // 关键的一格：传进来的迭代器指向**调用时**那份缓冲区，而 PkMut() 可能
    // detach，之后它对新缓冲区无效。序列容器靠"先算下标偏移"解决，关联容器没有
    // 下标——所以先把 key 抄一份出来，detach 之后按 key 重新 find。Qt 5.15 的
    // QMap::erase 在共享态下走的也是"按 key 重新定位"这条路。
    //
    // **一条与 Qt 的已知差异，留在这里备查**：`erase(constEnd())` 在我们这边
    // 会 detach（返回类型是 iterator，只能经 PkMut() 拿到内层的非 const end()），
    // Qt5 的 QMap::erase 则是 `if (it == end()) return it;` 直接返回、不 detach。
    // 之所以不为它开例外：Qt5 的 QMap::erase 收的是 **iterator**（调用点已经
    // detach 过了），我们收 const_iterator 是更宽松的形状，这一格没有等价物。
    // 删 end() 是没有调用点的写法，代价只落在"多一次深拷贝"，不影响任何语义。
    iterator erase(const_iterator pos)
    {
        {
            const Inner &cm = m_d.PkConst();
            if (pos == const_iterator(cm.end())) {
                return iterator(m_d.PkMut().end());
            }
        }
        const K wanted = pos.key();
        Inner &m = m_d.PkMut();
        const auto it = m.find(wanted);
        if (it == m.end()) {
            return iterator(m.end());
        }
        return iterator(m.erase(it));
    }

    // **不做 unite**：核实下来 QMap/QHash 的 unite 是 **0 处调用点**
    // （`git grep -nE '\.(unite|intersect|subtract)\('` 的 5 处命中里，
    //  kis_layer_utils.cpp:1384 的 frames 是 QSet<int>、
    //  kis_transform_worker.cc:214 的 dstBounds 是
    //  KisFilterWeightsApplicator::LinePos，都不是 Map 系）。
    // 给了就是凭空多一项，违反线级 spec 判据①「一项不多一项不少」——而且它还
    // 必然是一条与 Qt 的偏离：Qt5 的 QMap/QHash::unite 走 insertMulti 语义，
    // 同 key 并存多份，而我们的内层是单键的 std::map/std::unordered_map，
    // 只能覆盖。**QSet::unite 有 1 处真实调用点，保留在 PkSet 里。**

    // ---- 迭代器 ----
    //
    // 非 const 的 begin()/end()/find() 会 detach（实测 Qt：调 begin() 后
    // isDetached 0→1）；constBegin/constEnd/cbegin/cend/constFind 与全部 const
    // 重载绝不 detach。

    iterator begin() { return iterator(m_d.PkMut().begin()); }
    const_iterator begin() const noexcept { return const_iterator(m_d.PkConst().begin()); }
    iterator end() { return iterator(m_d.PkMut().end()); }
    const_iterator end() const noexcept { return const_iterator(m_d.PkConst().end()); }

    const_iterator cbegin() const noexcept { return const_iterator(m_d.PkConst().begin()); }
    const_iterator cend() const noexcept { return const_iterator(m_d.PkConst().end()); }
    const_iterator constBegin() const noexcept { return const_iterator(m_d.PkConst().begin()); }
    const_iterator constEnd() const noexcept { return const_iterator(m_d.PkConst().end()); }

    iterator find(const K &key) { return iterator(m_d.PkMut().find(key)); }
    const_iterator find(const K &key) const
    {
        return const_iterator(m_d.PkConst().find(key));
    }
    const_iterator constFind(const K &key) const
    {
        return const_iterator(m_d.PkConst().find(key));
    }

    // ---- 比较 ----

    bool operator==(const Derived &other) const
    {
        // 共享同一缓冲区时 O(1) 判真（Qt 的 `if (d == other.d) return true`）。
        return m_d.PkIsSharedWith(other.m_d) || m_d.PkConst() == other.m_d.PkConst();
    }
    bool operator!=(const Derived &other) const { return !(*this == other); }

    // ---- 只给单测用，不进 compat 垫片 ----
    //
    // Qt 的 isDetached()/isSharedWith() 在 Krita 调用点实测都是 0 处。这两个
    // Pk 前缀的观测器不在改名表里，只有单测会调——而单测非有它们不可：
    // 「写方法漏用 PkMut()」只有直接看共享状态才查得出来。
    long PkUseCount() const noexcept { return m_d.PkUseCount(); }
    bool PkIsSharedWith(const Derived &other) const noexcept
    {
        return m_d.PkIsSharedWith(other.m_d);
    }

protected:
    // 只给派生类构造/析构：析构非虚且 protected，杜绝经基类指针 delete。
    PkAssocContainer() = default;
    explicit PkAssocContainer(Inner init) : m_d(std::move(init)) {}
    ~PkAssocContainer() = default;

    // 五个特殊成员全部显式写出：一旦声明了析构，移动构造/移动赋值就不再隐式
    // 生成；而「移动之后源是空且完全可用的容器」这条 Qt 语义是 PkArrayData
    // 兜住的，退回拷贝会让它悄悄失效。拷贝同理——必须留着且是 O(1)。
    PkAssocContainer(const PkAssocContainer &) = default;
    PkAssocContainer &operator=(const PkAssocContainer &) = default;
    PkAssocContainer(PkAssocContainer &&) = default;
    PkAssocContainer &operator=(PkAssocContainer &&) = default;

    PkArrayData<Inner> m_d;
};
