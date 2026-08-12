#pragma once

// ---------------------------------------------------------------------------
// PkMap<K,V> 与 PkHash<K,V> 的**共同**单测。
//
// 两个容器共用一份实现（PkAssocContainer<K, V, Inner, Derived>），测试也共用
// 一份：每个用例都是 `template <template <typename, typename> class Map>`，在
// tests/test_pkmap.cpp 里用 PkMap 实例化、在 tests/test_pkhash.cpp 里用 PkHash
// 实例化。抄两份的话，改一条断言就得记得改两个地方，漏改的那一半会静默地
// 一直绿着。
//
// **不断言迭代顺序**：PkHash 无序（Qt 的 QHash 迭代顺序本就未定义），所以共同
// 用例一律"排序后比集合"。PkMap 的有序性是它专有的一格，在 test_pkmap.cpp 单列。
//
// 断言宏在这些模板里照常用：PK_VERIFY/PK_COMPARE 失败时 `return` 的是**本模板
// 函数**而不是测试函数，但失败已经通过 PkTestCase::checkResult() 记下了，
// 整个测试函数照样判失败。
// ---------------------------------------------------------------------------

#include "PkTest.h"

#include "../PkList.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// 带拷贝计数器的值类型：证明"拷贝容器 = O(1)"不是靠 PkUseCount 自我印证的，
// 而是真的一个元素都没拷。移动显式 noexcept，否则内层重哈希/再平衡会退化成
// 逐元素拷贝，计数器就分不清"detach 拷的"与"扩容拷的"。
struct PkAssocCounted
{
    int v = 0;

    PkAssocCounted() = default;
    explicit PkAssocCounted(int x) : v(x) {}
    PkAssocCounted(const PkAssocCounted &o) : v(o.v) { ++s_copies; }
    PkAssocCounted &operator=(const PkAssocCounted &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    PkAssocCounted(PkAssocCounted &&) noexcept = default;
    PkAssocCounted &operator=(PkAssocCounted &&) noexcept = default;
    ~PkAssocCounted() = default;

    bool operator==(const PkAssocCounted &o) const { return v == o.v; }

    inline static int s_copies = 0;
};

// 嵌套容器用的 POD：试接目标 kis_fill_interval_map_test.cpp 里的
// `QHash<int, QMap<int, POD>>` 就是这个形状，`it->field` 直达 value 的成员。
struct PkAssocPod
{
    int begin = 0;
    int end = 0;

    bool operator==(const PkAssocPod &o) const { return begin == o.begin && end == o.end; }
};

// 排序后比集合：PkHash/PkSet 无序，只能这么断言。
template <typename Seq>
std::vector<typename Seq::value_type> pkAssocSorted(const Seq &seq)
{
    std::vector<typename Seq::value_type> out;
    for (auto it = seq.constBegin(); it != seq.constEnd(); ++it) {
        out.push_back(*it);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ---------------------------------------------------------------------------
// 1. 基本查询与越界语义（实测 Qt 输出第 4、7、11、12 条）
// ---------------------------------------------------------------------------

template <template <typename, typename> class Map>
void pkAssocTestLookupAndDefaults()
{
    using M = Map<int, std::string>;
    M m{{1, "a"}, {2, "b"}, {3, "c"}};

    PK_COMPARE(m.size(), 3);
    PK_COMPARE(m.count(), 3);
    PK_VERIFY(!m.isEmpty());
    PK_VERIFY(!m.empty());

    PK_VERIFY(m.value(1) == "a");
    PK_VERIFY(m.value(3) == "c");

    // 实测 4：`value(99)=''  isEmpty=1` —— key 不存在返回 V()
    PK_VERIFY(m.value(99) == std::string());
    PK_VERIFY(m.value(99).empty());
    // value(k, def)：key 不存在返回 def
    PK_VERIFY(m.value(99, "zz") == "zz");
    PK_VERIFY(m.value(1, "zz") == "a");

    PK_VERIFY(m.contains(1));
    PK_VERIFY(!m.contains(99));

    // 实测 12：`QHash count(1)=1  count(9)=0`
    PK_COMPARE(m.count(1), 1);
    PK_COMPARE(m.count(9), 0);

    // 实测 11：`key("c")=3  key("zz")=0(默认)  key("zz",-5)=-5`
    PK_COMPARE(m.key("c"), 3);
    PK_COMPARE(m.key("zz"), 0);
    PK_COMPARE(m.key("zz", -5), -5);

    // 空容器
    M empty;
    PK_VERIFY(empty.isEmpty());
    PK_COMPARE(empty.size(), 0);
    PK_VERIFY(empty.value(1) == std::string());
    PK_COMPARE(empty.key("a"), 0);
    PK_COMPARE(empty.count(1), 0);
}

// 实测 5 / 6：非 const operator[] **插入**，const operator[] **不插入**
template <template <typename, typename> class Map>
void pkAssocTestSubscript()
{
    using M = Map<int, std::string>;
    M m{{1, "a"}, {2, "b"}, {3, "c"}};

    // 已存在的 key：取到值，size 不变
    PK_VERIFY(m[1] == "a");
    PK_COMPARE(m.size(), 3);

    // 实测 5：`非 const operator[](99): size 3 -> 4`
    PK_COMPARE(m.size(), 3);
    const std::string fresh = m[99];
    PK_VERIFY(fresh == std::string());
    PK_COMPARE(m.size(), 4);
    PK_VERIFY(m.contains(99));

    // 返回的是可写引用
    m[99] = "inserted";
    PK_VERIFY(m.value(99) == "inserted");
    PK_COMPARE(m.size(), 4);

    // 实测 6：`const operator[](777): size 4 -> 4` —— const 版不插入
    const M &cm = m;
    PK_COMPARE(m.size(), 4);
    PK_VERIFY(cm[777] == std::string());
    PK_COMPARE(m.size(), 4);
    PK_VERIFY(!m.contains(777));
    // const 版按值返回，取已有的 key 也照常
    PK_VERIFY(cm[1] == "a");
    PK_COMPARE(m.size(), 4);
}

// 实测 7 / 8：take / remove 的缺失语义，insert 覆盖
template <template <typename, typename> class Map>
void pkAssocTestInsertTakeRemove()
{
    using M = Map<int, std::string>;
    M m{{1, "a"}, {2, "b"}, {3, "c"}};

    // 实测 7：`take(1)='a'  take(555)=''(空)  remove(2)=1  remove(555)=0`
    PK_VERIFY(m.take(1) == "a");
    PK_COMPARE(m.size(), 2);
    PK_VERIFY(!m.contains(1));
    PK_VERIFY(m.take(555) == std::string());
    PK_COMPARE(m.size(), 2);
    PK_COMPARE(m.remove(2), 1);
    PK_COMPARE(m.size(), 1);
    PK_COMPARE(m.remove(555), 0);
    PK_COMPARE(m.size(), 1);

    // 实测 8：`insert 同 key 两次: size=1 value='y'` —— 覆盖，不是多值
    M dup;
    dup.insert(7, "x");
    PK_COMPARE(dup.size(), 1);
    dup.insert(7, "y");
    PK_COMPARE(dup.size(), 1);
    PK_VERIFY(dup.value(7) == "y");
    PK_COMPARE(dup.count(7), 1);

    // insert 返回指向该项的 iterator（Qt 语义）
    auto it = dup.insert(8, "z");
    PK_COMPARE(it.key(), 8);
    PK_VERIFY(it.value() == "z");
    PK_VERIFY(*it == "z");

    dup.clear();
    PK_VERIFY(dup.isEmpty());
    PK_COMPARE(dup.size(), 0);
    // clear 之后照常可用
    dup.insert(1, "back");
    PK_VERIFY(dup.value(1) == "back");
}

// ---------------------------------------------------------------------------
// 2. 迭代器 —— 本任务最容易整片编不过的一格
// ---------------------------------------------------------------------------

// 实测 1 / 2：`*it 的类型是 QString&`、`it.key()=1  it.value()=a`
template <template <typename, typename> class Map>
void pkAssocTestIteratorShape()
{
    using M = Map<int, std::string>;
    M m{{1, "a"}};

    auto it = m.begin();

    // 实测 1：解引用得 **value**，不是 std::map 的 pair
    static_assert(std::is_same<decltype(*it), std::string &>::value,
                  "*it 必须是 V&（Qt 语义），不是 std::pair<const K,V>&");
    static_assert(std::is_same<decltype(it.value()), std::string &>::value,
                  "it.value() 必须是 V&");
    static_assert(std::is_same<decltype(it.key()), const int &>::value,
                  "it.key() 必须是 const K&");
    // iterator 的 STL traits：value_type 是 V，不是 pair
    static_assert(std::is_same<typename M::iterator::value_type, std::string>::value,
                  "iterator::value_type 必须是 V");

    // 实测 2
    PK_COMPARE(it.key(), 1);
    PK_VERIFY(it.value() == "a");
    PK_VERIFY(*it == "a");

    // operator-> 直达 **value 的成员**（不是 pair 的 first/second）。
    // 这正是试接目标 kis_fill_interval_map.cpp 里 `it->field` 的写法。
    PK_COMPARE(static_cast<int>(it->size()), 1);
    it->append("bc");
    PK_VERIFY(m.value(1) == "abc");

    // 经解引用写回去
    *it = "rewritten";
    PK_VERIFY(m.value(1) == "rewritten");
    it.value() = "again";
    PK_VERIFY(m.value(1) == "again");

    // const_iterator 那一套：解引用得 const V&
    const M &cm = m;
    auto cit = cm.constBegin();
    static_assert(std::is_same<decltype(*cit), const std::string &>::value,
                  "const_iterator 的 *it 必须是 const V&");
    PK_COMPARE(cit.key(), 1);
    PK_VERIFY(cit.value() == "again");
    PK_COMPARE(static_cast<int>(cit->size()), 5);
}

template <template <typename, typename> class Map>
void pkAssocTestIteratorTraversal()
{
    using M = Map<int, int>;
    M m{{1, 10}, {2, 20}, {3, 30}};

    // 前置 ++ 走完一圈，收集 key 与 value（顺序不断言，排序后比集合）
    std::vector<int> keys;
    std::vector<int> vals;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        keys.push_back(it.key());
        vals.push_back(it.value());
    }
    std::sort(keys.begin(), keys.end());
    std::sort(vals.begin(), vals.end());
    PK_VERIFY((keys == std::vector<int>{1, 2, 3}));
    PK_VERIFY((vals == std::vector<int>{10, 20, 30}));

    // 后置 ++ 返回**自增前**的那一个
    auto it = m.constBegin();
    const int firstKey = it.key();
    auto prev = it++;
    PK_COMPARE(prev.key(), firstKey);
    PK_VERIFY(prev != it);

    // 相等比较（试接目标里的 `QCOMPARE(range.beginIt, range.endIt)`）
    PK_VERIFY(m.constBegin() == m.constBegin());
    PK_VERIFY(m.constBegin() != m.constEnd());
    PK_COMPARE(m.constEnd(), m.constEnd());

    // 空容器：begin() == end()
    M empty;
    PK_VERIFY(empty.constBegin() == empty.constEnd());
    PK_VERIFY(empty.begin() == empty.end());

    // range-for 走 begin()/end()，拿到的是 **value**
    int sum = 0;
    for (const int &v : m) {
        sum += v;
    }
    PK_COMPARE(sum, 60);

    // cbegin/cend 与 constBegin/constEnd 完全一致
    PK_VERIFY(m.cbegin() == m.constBegin());
    PK_VERIFY(m.cend() == m.constEnd());
}

// 实测 14：`iterator 隐式转 const_iterator: OK`
template <template <typename, typename> class Map>
void pkAssocTestIteratorConversion()
{
    using M = Map<int, int>;
    M m{{1, 10}, {2, 20}};

    typename M::iterator it = m.find(1);
    // 隐式转换（**不写 static_cast**，调用点靠的就是隐式）
    typename M::const_iterator cit = it;
    PK_COMPARE(cit.key(), 1);
    PK_COMPARE(cit.value(), 10);

    // 两个方向的混合比较都要能编
    PK_VERIFY(cit == it);
    PK_VERIFY(it == cit);
    PK_VERIFY(!(cit != it));
    PK_VERIFY(m.find(99) == m.end());
    PK_VERIFY(m.constFind(99) == m.constEnd());
    // 非 const 的 end() 与 const 的 constEnd() 指的是同一处
    PK_VERIFY(m.find(99) == m.constEnd());

    static_assert(std::is_convertible<typename M::iterator, typename M::const_iterator>::value,
                  "iterator 必须能隐式转成 const_iterator");
    static_assert(!std::is_convertible<typename M::const_iterator, typename M::iterator>::value,
                  "const_iterator 不得能转回 iterator");
}

// 实测 9：`erase(find(2)) 返回的迭代器 key=3`
template <template <typename, typename> class Map>
void pkAssocTestFindAndErase()
{
    using M = Map<int, int>;
    M m{{1, 10}, {2, 20}, {3, 30}};

    auto found = m.find(2);
    PK_VERIFY(found != m.end());
    PK_COMPARE(found.key(), 2);
    PK_COMPARE(found.value(), 20);

    auto missing = m.find(99);
    PK_VERIFY(missing == m.end());

    auto cfound = m.constFind(3);
    PK_VERIFY(cfound != m.constEnd());
    PK_COMPARE(cfound.value(), 30);
    PK_VERIFY(m.constFind(99) == m.constEnd());

    // erase 返回**下一个**。PkHash 无序，只能断言"返回的不是被删的那个、
    // 且剩下的集合正确"；PkMap 的有序版本（返回 key=3）在 test_pkmap.cpp 里。
    auto next = m.erase(m.find(2));
    PK_COMPARE(m.size(), 2);
    PK_VERIFY(!m.contains(2));
    if (next != m.end()) {
        PK_VERIFY(next.key() != 2);
    }
    PK_VERIFY((pkAssocSorted(m.keys()) == std::vector<int>{1, 3}));

    // erase(constEnd()) 是 no-op，返回 end()
    auto atEnd = m.erase(m.constEnd());
    PK_VERIFY(atEnd == m.end());
    PK_COMPARE(m.size(), 2);

    // 一路 erase 到空
    while (m.constBegin() != m.constEnd()) {
        m.erase(m.constBegin());
    }
    PK_VERIFY(m.isEmpty());

    // 共享状态下 erase：迭代器指向的是老缓冲区，detach 之后必须仍然删对项
    M a{{1, 10}, {2, 20}, {3, 30}};
    M b(a);
    auto bit = b.constFind(2);      // constFind 不 detach，b 仍与 a 共享
    PK_VERIFY(a.PkIsSharedWith(b));
    b.erase(bit);                   // erase 内部 detach，然后按 key 重新定位
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(b.size(), 2);
    PK_VERIFY(!b.contains(2));
    PK_COMPARE(a.size(), 3);
    PK_VERIFY(a.contains(2));
    PK_COMPARE(a.value(2), 20);
}

// ---------------------------------------------------------------------------
// 3. keys / values / unite / 比较
// ---------------------------------------------------------------------------

template <template <typename, typename> class Map>
void pkAssocTestKeysAndValues()
{
    using M = Map<int, int>;
    M m{{1, 10}, {2, 20}, {3, 10}};

    PK_VERIFY((pkAssocSorted(m.keys()) == std::vector<int>{1, 2, 3}));
    PK_VERIFY((pkAssocSorted(m.values()) == std::vector<int>{10, 10, 20}));

    // keys(value)：所有映射到该 value 的 key
    PK_VERIFY((pkAssocSorted(m.keys(10)) == std::vector<int>{1, 3}));
    PK_VERIFY((pkAssocSorted(m.keys(20)) == std::vector<int>{2}));
    PK_VERIFY(m.keys(99).isEmpty());

    // values(key)：单键容器上是 0 或 1 个。
    // **先落到具名变量再 at()**：`m.values(2).at(0)` 里的 PkList 是临时量，
    // at() 返回的 const T& 指向它的缓冲区，而临时量在整个完整表达式结束时就析构
    // 了（引用生存期延长对"经函数调用取到的子对象引用"不适用），PK_COMPARE 拿到
    // 的会是悬垂引用。
    const PkList<int> two = m.values(2);
    PK_COMPARE(two.size(), 1);
    PK_COMPARE(two.at(0), 20);
    PK_VERIFY(m.values(99).isEmpty());

    // 返回类型是 PkList
    static_assert(std::is_same<decltype(m.keys()), PkList<int>>::value,
                  "keys() 必须返回 PkList<K>");
    static_assert(std::is_same<decltype(m.values()), PkList<int>>::value,
                  "values() 必须返回 PkList<V>");

    M empty;
    PK_VERIFY(empty.keys().isEmpty());
    PK_VERIFY(empty.values().isEmpty());
}

// **没有 pkAssocTestUnite**：QMap/QHash 的 unite 核实下来是 0 处调用点，
// 所以 PkAssocContainer 根本不提供它（理由见 PkAssocContainer.h）。
// QSet::unite 有 1 处真实调用点，那条在 test_pkset.cpp 里压。

template <template <typename, typename> class Map>
void pkAssocTestComparison()
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}};
    M b{{2, 20}, {1, 10}};   // 插入顺序不同，内容相同
    M c{{1, 10}};
    M d{{1, 10}, {2, 99}};

    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    PK_VERIFY(a != c);
    PK_VERIFY(a != d);

    // 共享同一份缓冲区时是 O(1) 的真
    M shared(a);
    PK_VERIFY(a == shared);

    const M &alias = a;
    PK_VERIFY(a == alias);

    M e1;
    M e2;
    PK_VERIFY(e1 == e2);
    PK_VERIFY(e1 != a);

    // 比较是 const 路径：不得 detach
    M x{{5, 5}};
    M y(x);
    (void)(x == y);
    (void)(x != y);
    PK_VERIFY(x.PkIsSharedWith(y));
}

// ---------------------------------------------------------------------------
// 4. COW —— 本任务的核心
// ---------------------------------------------------------------------------

template <template <typename, typename> class Map>
void pkAssocTestCowIsolation()
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}, {3, 30}};
    M b(a);

    // 拷贝 = 共享
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a == b);

    // 改 b，a 一个字节都没变
    b.insert(4, 40);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_VERIFY(!a.contains(4));
    PK_VERIFY((pkAssocSorted(a.keys()) == std::vector<int>{1, 2, 3}));
    PK_COMPARE(b.size(), 4);

    // 反方向：再共享一次，这回改 a
    M c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    a[1] = 99;
    PK_VERIFY(!a.PkIsSharedWith(c));
    PK_COMPARE(c.value(1), 10);
    PK_COMPARE(a.value(1), 99);

    // 拷贝赋值同样是共享
    M d;
    d = a;
    PK_VERIFY(a.PkIsSharedWith(d));
    d.clear();
    PK_VERIFY(!a.PkIsSharedWith(d));
    PK_COMPARE(a.size(), 3);

    // 三方共享：一方写只把自己摘出去，另外两方继续共享
    M p{{1, 1}};
    M q(p);
    M r(p);
    PK_COMPARE(p.PkUseCount(), 3L);
    q.insert(2, 2);
    PK_VERIFY(p.PkIsSharedWith(r));
    PK_COMPARE(p.PkUseCount(), 2L);
    PK_COMPARE(p.size(), 1);
    PK_COMPARE(r.size(), 1);
}

template <template <typename, typename> class Map>
void pkAssocTestCopyIsConstantTime()
{
    // 硬要求 3：拷贝必须 O(1)。2286 处 Q_FOREACH 按值拷贝整个容器全指望这一条。
    using M = Map<int, PkAssocCounted>;
    M a;
    for (int i = 0; i < 5; ++i) {
        a.insert(i, PkAssocCounted(i));
    }

    PkAssocCounted::s_copies = 0;
    M b(a);
    PK_COMPARE(PkAssocCounted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 2L);

    M c;
    c = a;
    PK_COMPARE(PkAssocCounted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 3L);

    // 正向对照：共享状态下写一下，**深拷了 5 个值**（计数器本身没失灵）。
    // 总数是 6 = detach 的 5 次 + insert 自己的 1 次：insert 的签名是
    // `insert(const K &, const V &)`（Qt 的形状），值只能拷进节点，没有右值重载。
    PkAssocCounted::s_copies = 0;
    b.insert(9, PkAssocCounted(9));
    PK_COMPARE(PkAssocCounted::s_copies, 6);

    // 摘出去之后再写：只剩 insert 自己那 1 次，没有 detach 的那 5 次
    PkAssocCounted::s_copies = 0;
    b.insert(10, PkAssocCounted(10));
    PK_COMPARE(PkAssocCounted::s_copies, 1);
}

template <template <typename, typename> class Map>
void pkAssocTestConstNeverDetaches()
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}, {3, 30}};
    M b(a);
    const M &ca = a;
    PK_VERIFY(a.PkIsSharedWith(b));

    // 一串 const 方法轮流调，全程共享状态不得变
    for (int i = 0; i < 3; ++i) {
        (void)ca.size();
        (void)ca.count();
        (void)ca.count(1);
        (void)ca.isEmpty();
        (void)ca.empty();
        (void)ca.contains(1);
        (void)ca.value(1);
        (void)ca.value(99, 7);
        (void)ca[1];
        (void)ca[999];          // const operator[] 连缺失的 key 都不该动它
        (void)ca.key(10);
        (void)ca.key(999, -1);
        (void)ca.keys();
        (void)ca.keys(10);
        (void)ca.values();
        (void)ca.values(1);
        (void)ca.constBegin();
        (void)ca.constEnd();
        (void)ca.cbegin();
        (void)ca.cend();
        (void)ca.constFind(1);
        (void)ca.constFind(999);
        (void)ca.begin();       // **const 对象上的 begin()**：走 const 重载，不 detach
        (void)ca.end();
        (void)ca.find(1);       // const 对象上的 find()：同上
        (void)(ca == b);
        (void)(ca != b);

        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 2L);
    }

    PK_COMPARE(a.size(), 3);
    PK_VERIFY(a == b);
}

// 迭代器入口的 detach 时机 —— 实测（真 Qt 5.15.7 与 5.15.13，两份输出一致）：
//
//   拷贝后                  x.isDetached = 0
//   调 x.begin()（非 const）  x.isDetached = 1   ← 非 const begin() 触发 detach
//   调 x2.constBegin()       x2.isDetached = 0   ← constBegin() 不触发
//   const 对象上 begin()     x3.isDetached = 0   ← const 重载不触发
//
// 第三条最容易漏，因为它与第二条**同名**——写成 `a.begin()` 还是 `ca.begin()`
// 决定了走哪个重载，只压前者的话 const 重载偷偷走了 PkMut() 也没人知道。
template <template <typename, typename> class Map>
void pkAssocTestIteratorDetachTiming()
{
    using M = Map<int, int>;

    // ① const 迭代器入口一串下来，全程保持共享
    {
        M a{{1, 10}, {2, 20}};
        M b(a);
        PK_VERIFY(a.PkIsSharedWith(b));
        for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
            (void)it.key();
        }
        for (auto it = a.cbegin(); it != a.cend(); ++it) {
            (void)it.value();
        }
        (void)a.constFind(1);
        (void)a.constFind(999);
        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 2L);
    }

    // ② 非 const begin()：立刻不再共享，另一边内容一个字节不变
    {
        M a{{1, 10}, {2, 20}};
        M b(a);
        PK_VERIFY(a.PkIsSharedWith(b));
        (void)b.begin();
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 1L);
        PK_COMPARE(a.size(), 2);
        PK_COMPARE(a.value(1), 10);
        PK_COMPARE(a.value(2), 20);
        PK_VERIFY(a == b);
    }

    // ③ **const 对象上**的 begin()/end()/find()：仍然共享
    {
        M a{{1, 10}, {2, 20}};
        M b(a);
        const M &cb = b;
        PK_VERIFY(a.PkIsSharedWith(b));
        for (auto it = cb.begin(); it != cb.end(); ++it) {
            (void)it.key();
        }
        (void)cb.find(1);
        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 2L);
    }
}

// ---------------------------------------------------------------------------
// COW 清单：两侧同表 —— {名字, 怎么调, 期望是否 detach}
//
// 与序列容器侧同一套做法（tests/PkSeqTestShared.h 的 PkSeqCowCase）：清单会腐烂，
// 数据驱动让"以后加了方法却忘了登记"变成表里显眼的空缺，也把「该不该 detach」
// 逼成必须显式写出来的一格。
//
// **可写访问器的用例只"调"、不"写"**：拿到可写引用/可写迭代器本身就该 detach。
// 写成 `m[1] = 9` 的话，有人把非 const operator[] 改成 PkConst() + const_cast，
// 测试照样全绿，而那会让共享的两个容器通过引用互相污染。
//
// lambda 一律无捕获 → 隐式转函数指针，整张表才能是同一类型的数组。
// ---------------------------------------------------------------------------

template <template <typename, typename> class Map>
struct PkAssocCowCase
{
    const char *name;
    void (*call)(Map<int, int> &);
    bool expectDetach;
};

template <template <typename, typename> class Map>
void pkAssocRunCowCase(const PkAssocCowCase<Map> &c)
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}, {3, 30}};
    M b(a);
    PK_VERIFY2(a.PkIsSharedWith(b), c.name);
    PK_VERIFY2(a.PkUseCount() == 2L, c.name);

    c.call(b);

    if (c.expectDetach) {
        PK_VERIFY2(!a.PkIsSharedWith(b), c.name);
        PK_VERIFY2(a.PkUseCount() == 1L, c.name);
    } else {
        PK_VERIFY2(a.PkIsSharedWith(b), c.name);
        PK_VERIFY2(a.PkUseCount() == 2L, c.name);
    }

    // 无论哪一侧，另一边的内容都必须一个字节不变
    PK_VERIFY2(a.size() == 3, c.name);
    PK_VERIFY2(a.value(1) == 10, c.name);
    PK_VERIFY2(a.value(2) == 20, c.name);
    PK_VERIFY2(a.value(3) == 30, c.name);
    PK_VERIFY2(!a.contains(99), c.name);
}

template <template <typename, typename> class Map, std::size_t N>
void pkAssocRunCowCases(const PkAssocCowCase<Map> (&cases)[N])
{
    for (std::size_t i = 0; i < N; ++i) {
        pkAssocRunCowCase<Map>(cases[i]);
    }
}

// 每一个非 const 方法逐个验证：共享状态下调用之后两边不再共享，且另一边内容
// 不变。这是查 PkMut() 漏用的唯一办法——漏一个方法就是一个 COW 漏洞。
template <template <typename, typename> class Map>
void pkAssocTestEveryWriterDetaches()
{
    using M = Map<int, int>;

    static const PkAssocCowCase<Map> cases[] = {
        // ---- 写 ----
        {"insert(新 key)", [](M &m) { (void)m.insert(4, 40); }, true},
        {"insert(已有 key)", [](M &m) { (void)m.insert(1, 99); }, true},
        {"remove(命中)", [](M &m) { (void)m.remove(1); }, true},
        {"remove(不命中)", [](M &m) { (void)m.remove(99); }, true},
        {"take(命中)", [](M &m) { (void)m.take(1); }, true},
        {"take(不命中)", [](M &m) { (void)m.take(99); }, true},
        {"clear", [](M &m) { m.clear(); }, true},
        {"erase(it)", [](M &m) { (void)m.erase(m.constBegin()); }, true},

        // ---- 可写访问器：调用本身即视为写 ----
        // 非 const operator[] **看起来像读，其实是写**：不存在的 key 会被插入。
        // 实测 Qt：`非 const operator[](99): size 3 -> 4`。
        {"operator[](已有 key)", [](M &m) { (void)m[1]; }, true},
        {"operator[](缺失 key)", [](M &m) { (void)m[99]; }, true},

        // ---- 可写迭代器：拿到即视为写（实测：调 begin() 后 isDetached 0→1）----
        {"begin()", [](M &m) { (void)m.begin(); }, true},
        {"end()", [](M &m) { (void)m.end(); }, true},
        {"find(命中)", [](M &m) { (void)m.find(1); }, true},
        {"find(不命中)", [](M &m) { (void)m.find(99); }, true},

        // ---- 反面：const 入口一律不 detach ----
        {"constBegin()", [](M &m) { (void)m.constBegin(); }, false},
        {"constEnd()", [](M &m) { (void)m.constEnd(); }, false},
        {"cbegin()", [](M &m) { (void)m.cbegin(); }, false},
        {"cend()", [](M &m) { (void)m.cend(); }, false},
        {"constFind(命中)", [](M &m) { (void)m.constFind(1); }, false},
        {"constFind(不命中)", [](M &m) { (void)m.constFind(99); }, false},
        {"size()", [](M &m) { (void)m.size(); }, false},
        {"count()", [](M &m) { (void)m.count(); }, false},
        {"count(key)", [](M &m) { (void)m.count(1); }, false},
        {"isEmpty()", [](M &m) { (void)m.isEmpty(); }, false},
        {"empty()", [](M &m) { (void)m.empty(); }, false},
        {"contains()", [](M &m) { (void)m.contains(1); }, false},
        {"value(k)", [](M &m) { (void)m.value(1); }, false},
        {"value(k, def)", [](M &m) { (void)m.value(99, 0); }, false},
        {"key(v)", [](M &m) { (void)m.key(10); }, false},
        {"key(v, def)", [](M &m) { (void)m.key(999, -1); }, false},
        {"keys()", [](M &m) { (void)m.keys(); }, false},
        {"keys(v)", [](M &m) { (void)m.keys(10); }, false},
        {"values()", [](M &m) { (void)m.values(); }, false},
        {"values(k)", [](M &m) { (void)m.values(1); }, false},
        // const operator[] 连缺失的 key 都不插入（实测 Qt：size 4 -> 4）
        {"const operator[](缺失 key)",
         [](M &m) {
             const M &cm = m;
             (void)cm[99];
         },
         false},
        {"const 对象上的 begin()",
         [](M &m) {
             const M &cm = m;
             (void)cm.begin();
         },
         false},
        {"const 对象上的 find()",
         [](M &m) {
             const M &cm = m;
             (void)cm.find(1);
         },
         false},
    };

    pkAssocRunCowCases<Map>(cases);
}

// ---------------------------------------------------------------------------
// 5. 赋值与移动
// ---------------------------------------------------------------------------

template <template <typename, typename> class Map>
void pkAssocTestSelfAssignment()
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}};
    // 经引用绕一道：直接写 a = a 会被 -Wself-assign-overloaded 拦下，
    // 而真实调用点里的自赋值本来就是通过别名/引用发生的。
    M &alias = a;
    a = alias;

    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 2);
    PK_COMPARE(a.value(1), 10);

    // 自赋值之后照常可写
    a.insert(3, 30);
    PK_COMPARE(a.size(), 3);
    PK_COMPARE(a.PkUseCount(), 1L);

    // 共享状态下的自赋值：共享关系与内容都不该被破坏
    M b{{5, 50}};
    M c(b);
    M &bAlias = b;
    b = bAlias;
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(b.PkIsSharedWith(c));
    PK_COMPARE(b.size(), 1);
    PK_COMPARE(c.size(), 1);

    // 自赋值之后 COW 仍然生效
    b.insert(6, 60);
    PK_VERIFY(!b.PkIsSharedWith(c));
    PK_COMPARE(c.size(), 1);
}

// 移动语义由 PkArrayData 兜底（移动之后源是「空且完全可用」的容器，Qt 语义），
// 容器层只 = default。这里验证那份兜底真的透过容器层生效了。
template <template <typename, typename> class Map>
void pkAssocTestMoveLeavesSourceUsable()
{
    using M = Map<int, int>;
    M a{{1, 10}, {2, 20}};
    M b(std::move(a));

    PK_COMPARE(b.size(), 2);
    PK_COMPARE(b.value(1), 10);
    PK_COMPARE(b.PkUseCount(), 1L);

    // 源：空容器，且完全可用
    PK_COMPARE(a.size(), 0);
    PK_VERIFY(a.isEmpty());
    // 不断言 moved-from 的 PkUseCount()：源拿到的是 PkArrayData 进程内共享的
    // 空哨兵（这样移动才能 noexcept + 零分配，与 Qt 的 sharedNull 同构），
    // 计数是「1 + 当前活着的 moved-from 个数」，会随别处的测试浮动。
    PK_VERIFY(a.PkUseCount() >= 1L);
    PK_VERIFY(a.constBegin() == a.constEnd());
    a.insert(42, 420);
    PK_COMPARE(a.PkUseCount(), 1L);   // 写入让源从哨兵上 detach 出来，成为独占
    PK_COMPARE(a.size(), 1);
    PK_COMPARE(b.size(), 2);
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 移动赋值同样
    M c{{7, 70}};
    M d{{0, 0}};
    d = std::move(c);
    PK_COMPARE(d.size(), 1);
    PK_COMPARE(d.value(7), 70);
    PK_COMPARE(c.size(), 0);
    PK_VERIFY(c.isEmpty());
    c.insert(5, 50);
    PK_COMPARE(c.size(), 1);
    PK_COMPARE(d.size(), 1);

    // 拷贝构造/拷贝赋值没有因为声明了移动而被 deleted
    static_assert(std::is_copy_constructible<M>::value,
                  "拷贝构造必须存在——声明移动构造会把它 deleted 掉");
    static_assert(std::is_copy_assignable<M>::value, "拷贝赋值必须存在");
    static_assert(std::is_move_constructible<M>::value, "移动构造必须存在");
    static_assert(std::is_move_assignable<M>::value, "移动赋值必须存在");
}
