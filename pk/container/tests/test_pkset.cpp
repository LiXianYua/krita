#include "PkSetTest.h"

#include "../PkSet.h"
#include "../PkStringHash.h"

#include "PkTest.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// PkTestBinder<PkSetTest> 特化由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发）。显式特化必须在 qExec<PkSetTest> 实例化前对本 TU 可见，所以像 moc 的
// `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkSetTest.inc"

namespace {

using IntSet = PkSet<int>;

// ---- 契约的编译期部分（签名形状，不是行为）----

static_assert(std::is_same<decltype(std::declval<const IntSet &>().size()), int>::value,
              "size() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntSet &>().count()), int>::value,
              "count() 必须返回 int");
// **QSet::remove 返回 bool**（删没删掉），QMap/QHash::remove 才返回 int（几个）。
// 这两个极易抄混，钉在编译期。
static_assert(std::is_same<decltype(std::declval<IntSet &>().remove(0)), bool>::value,
              "QSet::remove() 必须返回 bool（不是 int）");
static_assert(std::is_same<decltype(std::declval<IntSet &>().insert(0)), IntSet::iterator>::value,
              "insert() 必须返回 iterator");
static_assert(std::is_same<decltype(std::declval<const IntSet &>().values()), PkList<int>>::value,
              "values() 必须返回 PkList<T>");
static_assert(std::is_same<decltype(std::declval<const IntSet &>().toList()), PkList<int>>::value,
              "toList() 必须返回 PkList<T>");

// QSet 的元素不可写：*it 是 const T&，iterator 与 const_iterator 同类型。
static_assert(std::is_same<decltype(*std::declval<IntSet::const_iterator &>()),
                           const int &>::value,
              "QSet 的 *it 必须是 const T&（元素改了会破坏哈希不变量）");
static_assert(std::is_same<IntSet::iterator, IntSet::const_iterator>::value,
              "QSet 的 iterator 与 const_iterator 是同一个类型");

static_assert(std::is_copy_constructible<IntSet>::value, "拷贝构造必须存在");
static_assert(std::is_copy_assignable<IntSet>::value, "拷贝赋值必须存在");
static_assert(std::is_move_constructible<IntSet>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntSet>::value, "移动赋值必须存在");

// **明确不做的那批**（核实 0 调用点）：toSet / fromSet / 以及除 `|` `|=` 之外的
// 集合运算符。给了就是凭空多一项，违反判据①。探测惯用法把"以后别长出来"钉在
// 编译期，而不只是写在注释里。
template <typename S, typename = void>
struct PkHasToSet : std::false_type {
};
template <typename S>
struct PkHasToSet<S, std::void_t<decltype(std::declval<const S &>().toSet())>>
    : std::true_type {
};

template <typename S, typename = void>
struct PkHasAndEq : std::false_type {
};
template <typename S>
struct PkHasAndEq<S, std::void_t<decltype(std::declval<S &>() &= std::declval<const S &>())>>
    : std::true_type {
};

template <typename S, typename = void>
struct PkHasMinusEq : std::false_type {
};
template <typename S>
struct PkHasMinusEq<S, std::void_t<decltype(std::declval<S &>() -= std::declval<const S &>())>>
    : std::true_type {
};

template <typename S, typename = void>
struct PkHasOrEq : std::false_type {
};
template <typename S>
struct PkHasOrEq<S, std::void_t<decltype(std::declval<S &>() |= std::declval<const S &>())>>
    : std::true_type {
};

static_assert(!PkHasToSet<IntSet>::value, "toSet 实测 0 调用点，不得实现");
static_assert(!PkHasAndEq<IntSet>::value, "QSet 的 operator&= 核实 0 调用点，不得实现");
static_assert(!PkHasMinusEq<IntSet>::value, "QSet 的 operator-= 核实 0 调用点，不得实现");
// 反面对照：探测惯用法本身没写坏 —— operator|= 有 4 处真实调用点，确实提供了。
static_assert(PkHasOrEq<IntSet>::value,
              "operator|= 有 4 处调用点：探测惯用法若这里为假，说明它整个失灵了");

// ---------------------------------------------------------------------------
// 自定义 qHash 的测试类型。与 test_pkhash.cpp 同因：类型与它的 qHash 一起放进
// **匿名命名空间**，正是为了证明走的是 ADL —— 匿名命名空间里的 qHash 在
// PkHasher 的模板定义点（PkHashFunctions.h）根本不可见。
//
// Krita 侧的真实样例：libs/image/brushengine/kis_paintop_lod_limitations.h:14
//   `inline uint qHash(const KoID &id) { return qHash(id.id()); }`
// 而同一个文件里 :34-35 就是 `QSet<KoID> limitations; QSet<KoID> blockers;`
// ——PkSet 必须能找到那条重载，否则那个文件当场编不过。
// ---------------------------------------------------------------------------
struct PkTagged
{
    int group = 0;
    int id = 0;

    bool operator==(const PkTagged &o) const { return group == o.group && id == o.id; }
};

// 故意只看 group：同 group 不同 id 必然哈希冲突，把 operator== 那一环也压到。
inline unsigned int qHash(const PkTagged &t)
{
    return static_cast<unsigned int>(t.group);
}

int g_setHashCalls = 0;

struct PkSetCounted
{
    int v = 0;
    bool operator==(const PkSetCounted &o) const { return v == o.v; }
};

inline unsigned int qHash(const PkSetCounted &k)
{
    ++g_setHashCalls;
    return static_cast<unsigned int>(k.v);
}

// 带拷贝计数器的元素类型：证明"拷贝容器 = O(1)"不是靠 PkUseCount 自我印证的。
struct PkSetElem
{
    int v = 0;

    PkSetElem() = default;
    explicit PkSetElem(int x) : v(x) {}
    PkSetElem(const PkSetElem &o) : v(o.v) { ++s_copies; }
    PkSetElem &operator=(const PkSetElem &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    PkSetElem(PkSetElem &&) noexcept = default;
    PkSetElem &operator=(PkSetElem &&) noexcept = default;
    ~PkSetElem() = default;

    bool operator==(const PkSetElem &o) const { return v == o.v; }

    inline static int s_copies = 0;
};

inline unsigned int qHash(const PkSetElem &e)
{
    return static_cast<unsigned int>(e.v);
}

// PkSet 无序：一律排序后比集合。
std::vector<int> sortedOf(const PkSet<int> &s)
{
    std::vector<int> out;
    for (auto it = s.constBegin(); it != s.constEnd(); ++it) {
        out.push_back(*it);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ---- COW 清单：{名字, 怎么调, 期望是否 detach} ----
//
// 与序列侧 PkSeqCowCase / 关联侧 PkAssocCowCase 同一套做法：清单会腐烂，
// 数据驱动让"以后加了方法却忘了登记"变成表里显眼的空缺。
struct PkSetCowCase
{
    const char *name;
    void (*call)(PkSet<int> &);
    bool expectDetach;
};

} // namespace

// ---------------------------------------------------------------------------

// 实测 13：`QSet 去重: insert(1) 两次后 size=1`
void PkSetTest::insertAndContains()
{
    PkSet<int> s;
    PK_VERIFY(s.isEmpty());
    PK_COMPARE(s.size(), 0);
    PK_COMPARE(s.count(), 0);

    s.insert(1);
    PK_COMPARE(s.size(), 1);
    PK_VERIFY(s.contains(1));
    PK_VERIFY(!s.isEmpty());

    // 去重：同一个值插两次，size 仍然是 1
    s.insert(1);
    PK_COMPARE(s.size(), 1);

    s.insert(2);
    s.insert(3);
    PK_COMPARE(s.size(), 3);
    PK_VERIFY((sortedOf(s) == std::vector<int>{1, 2, 3}));

    PK_VERIFY(!s.contains(99));

    // insert 返回指向该元素的迭代器（重复插入时指向已有的那个）
    auto it = s.insert(2);
    PK_COMPARE(*it, 2);
    PK_COMPARE(s.size(), 3);

    // 初始化列表构造同样去重
    PkSet<int> lit{1, 1, 2, 2, 3};
    PK_COMPARE(lit.size(), 3);
    PK_VERIFY((sortedOf(lit) == std::vector<int>{1, 2, 3}));

    PkSet<int> empty;
    PK_VERIFY(empty.isEmpty());
    PK_VERIFY(!empty.contains(0));
}

void PkSetTest::removeAndClear()
{
    PkSet<int> s{1, 2, 3};

    // QSet::remove 返回**删没删掉**（bool）
    PK_VERIFY(s.remove(2));
    PK_COMPARE(s.size(), 2);
    PK_VERIFY(!s.contains(2));

    PK_VERIFY(!s.remove(99));
    PK_COMPARE(s.size(), 2);

    s.clear();
    PK_VERIFY(s.isEmpty());
    PK_COMPARE(s.size(), 0);

    // clear 之后照常可用
    s.insert(7);
    PK_VERIFY((sortedOf(s) == std::vector<int>{7}));

    // 空集上删任何东西都返回 false
    PkSet<int> empty;
    PK_VERIFY(!empty.remove(0));
}

void PkSetTest::valuesAndToList()
{
    PkSet<int> s{3, 1, 2};

    const PkList<int> v = s.values();
    PK_COMPARE(v.size(), 3);
    // 无序：排序后比集合（QSet 的迭代顺序在 Qt 里本就未定义）
    PkList<int> sortedV = v;
    std::sort(sortedV.begin(), sortedV.end());
    PK_VERIFY((sortedV == PkList<int>{1, 2, 3}));

    // toList() 与 values() 结果相同（toList 是 Qt 的老名字）
    PkList<int> sortedL = s.toList();
    std::sort(sortedL.begin(), sortedL.end());
    PK_VERIFY((sortedL == sortedV));

    PkSet<int> empty;
    PK_VERIFY(empty.values().isEmpty());
    PK_VERIFY(empty.toList().isEmpty());

    // 是 const 路径：不得 detach
    PkSet<int> a{1, 2};
    PkSet<int> b(a);
    (void)a.values();
    (void)a.toList();
    PK_VERIFY(a.PkIsSharedWith(b));
}

// unite(1 处) / intersect(1 处) / subtract(1 处) / operator|=(4 处) / operator|(1 处)
// —— 五项都有真实调用点，逐个压。
void PkSetTest::uniteIntersectSubtract()
{
    // unite：libs/image/kis_layer_utils.cpp:1384 frames.unite(...)
    PkSet<int> a{1, 2};
    PkSet<int> b{2, 3};
    PkSet<int> &uniteRef = a.unite(b);
    PK_VERIFY(&uniteRef == &a);
    PK_VERIFY((sortedOf(a) == std::vector<int>{1, 2, 3}));
    PK_VERIFY((sortedOf(b) == std::vector<int>{2, 3}));   // 源不变

    // intersect：libs/image/kis_layer_utils.cpp:2567 allKeyframeTimes().intersect(times)
    PkSet<int> c{1, 2, 3, 4};
    PkSet<int> d{2, 4, 9};
    c.intersect(d);
    PK_VERIFY((sortedOf(c) == std::vector<int>{2, 4}));
    PK_VERIFY((sortedOf(d) == std::vector<int>{2, 4, 9}));

    // 交空集 → 空
    PkSet<int> e{1, 2};
    PkSet<int> emptySet;
    e.intersect(emptySet);
    PK_VERIFY(e.isEmpty());

    // subtract：libs/ui/widgets/kis_color_label_button.cpp:222
    //           viableColorLabels.subtract(labels)
    PkSet<int> f{1, 2, 3};
    PkSet<int> g{2, 9};
    f.subtract(g);
    PK_VERIFY((sortedOf(f) == std::vector<int>{1, 3}));
    PK_VERIFY((sortedOf(g) == std::vector<int>{2, 9}));

    // 自运算：并/交是 no-op，差得到空集（Qt 同样）
    PkSet<int> h{1, 2};
    PkSet<int> &hAlias = h;
    h.unite(hAlias);
    PK_COMPARE(h.size(), 2);
    h.intersect(hAlias);
    PK_COMPARE(h.size(), 2);
    h.subtract(hAlias);
    PK_VERIFY(h.isEmpty());

    // operator|=（4 处调用点的形状）
    PkSet<int> lhs{1};
    PkSet<int> rhs{2, 3};
    PkSet<int> &orRef = (lhs |= rhs);
    PK_VERIFY(&orRef == &lhs);
    PK_VERIFY((sortedOf(lhs) == std::vector<int>{1, 2, 3}));

    // operator|（libs/image/kis_layer_utils.cpp:150 的形状：两侧都是右值）
    PkSet<int> unioned = PkSet<int>{1, 2} | PkSet<int>{2, 5};
    PK_VERIFY((sortedOf(unioned) == std::vector<int>{1, 2, 5}));
    // 两个具名集合，源都不变
    PkSet<int> p{1};
    PkSet<int> q{2};
    PkSet<int> r = p | q;
    PK_VERIFY((sortedOf(r) == std::vector<int>{1, 2}));
    PK_VERIFY((sortedOf(p) == std::vector<int>{1}));
    PK_VERIFY((sortedOf(q) == std::vector<int>{2}));
}

void PkSetTest::comparison()
{
    PkSet<int> a{1, 2, 3};
    PkSet<int> b{3, 2, 1};   // 插入顺序不同，内容相同
    PkSet<int> c{1, 2};
    PkSet<int> d{1, 2, 9};

    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    PK_VERIFY(a != c);
    PK_VERIFY(a != d);

    // 共享同一份缓冲区时是 O(1) 的真
    PkSet<int> shared(a);
    PK_VERIFY(a == shared);

    const PkSet<int> &alias = a;
    PK_VERIFY(a == alias);

    PkSet<int> e1;
    PkSet<int> e2;
    PK_VERIFY(e1 == e2);
    PK_VERIFY(e1 != a);

    // 比较是 const 路径：不得 detach
    PkSet<int> x{5};
    PkSet<int> y(x);
    (void)(x == y);
    (void)(x != y);
    PK_VERIFY(x.PkIsSharedWith(y));
}

void PkSetTest::iterators()
{
    PkSet<int> s{1, 2, 3};

    // 遍历（顺序不断言）
    std::vector<int> seen;
    for (auto it = s.constBegin(); it != s.constEnd(); ++it) {
        seen.push_back(*it);
    }
    std::sort(seen.begin(), seen.end());
    PK_VERIFY((seen == std::vector<int>{1, 2, 3}));

    // range-for
    int sum = 0;
    for (int v : s) {
        sum += v;
    }
    PK_COMPARE(sum, 6);

    // begin()/end() 与 constBegin()/constEnd() 指的是同一处
    PK_VERIFY(s.begin() == s.constBegin());
    PK_VERIFY(s.end() == s.constEnd());

    // 空集
    PkSet<int> empty;
    PK_VERIFY(empty.begin() == empty.end());
    PK_VERIFY(empty.constBegin() == empty.constEnd());
}

void PkSetTest::cowIsolation()
{
    PkSet<int> a{1, 2, 3};
    PkSet<int> b(a);

    // 拷贝 = 共享
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a == b);

    // 改 b，a 一个字节都没变
    b.insert(4);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_VERIFY(!a.contains(4));
    PK_VERIFY((sortedOf(a) == std::vector<int>{1, 2, 3}));
    PK_COMPARE(b.size(), 4);

    // 反方向
    PkSet<int> c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    a.remove(1);
    PK_VERIFY(!a.PkIsSharedWith(c));
    PK_VERIFY(c.contains(1));
    PK_VERIFY(!a.contains(1));

    // 拷贝赋值同样是共享
    PkSet<int> d;
    d = a;
    PK_VERIFY(a.PkIsSharedWith(d));
    d.clear();
    PK_VERIFY(!a.PkIsSharedWith(d));
    PK_COMPARE(a.size(), 2);

    // 三方共享：一方写只把自己摘出去，另外两方继续共享
    PkSet<int> p{1};
    PkSet<int> q(p);
    PkSet<int> r(p);
    PK_COMPARE(p.PkUseCount(), 3L);
    q.insert(2);
    PK_VERIFY(p.PkIsSharedWith(r));
    PK_COMPARE(p.PkUseCount(), 2L);
    PK_COMPARE(p.size(), 1);
    PK_COMPARE(r.size(), 1);
}

void PkSetTest::copyIsConstantTime()
{
    // 硬要求 3：拷贝必须 O(1)。2286 处 Q_FOREACH 按值拷贝整个容器全指望这一条。
    PkSet<PkSetElem> a;
    for (int i = 0; i < 5; ++i) {
        a.insert(PkSetElem(i));
    }

    PkSetElem::s_copies = 0;
    PkSet<PkSetElem> b(a);
    PK_COMPARE(PkSetElem::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 2L);

    PkSet<PkSetElem> c;
    c = a;
    PK_COMPARE(PkSetElem::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 3L);

    // 正向对照：共享状态下写一下，**深拷了 5 个元素**（计数器本身没失灵）。
    // 总数是 6 = detach 的 5 次 + insert 自己的 1 次：insert 的签名是
    // `insert(const T &)`（Qt 的形状），元素只能拷进节点，没有右值重载。
    PkSetElem::s_copies = 0;
    b.insert(PkSetElem(9));
    PK_COMPARE(PkSetElem::s_copies, 6);

    // 摘出去之后再写：只剩 insert 自己那 1 次，没有 detach 的那 5 次
    PkSetElem::s_copies = 0;
    b.insert(PkSetElem(10));
    PK_COMPARE(PkSetElem::s_copies, 1);
}

void PkSetTest::constNeverDetaches()
{
    PkSet<int> a{1, 2, 3};
    PkSet<int> b(a);
    const PkSet<int> &ca = a;
    PK_VERIFY(a.PkIsSharedWith(b));

    // 一串 const 方法轮流调，全程共享状态不得变。
    // **PkSet 的 begin()/end() 也在这张表里**：QSet 的元素不可写，拿不到可写
    // 迭代器，所以它们与 PkMap/PkHash 的非 const begin() 不同，不该 detach。
    for (int i = 0; i < 3; ++i) {
        (void)ca.size();
        (void)ca.count();
        (void)ca.isEmpty();
        (void)ca.contains(1);
        (void)ca.values();
        (void)ca.toList();
        (void)ca.begin();
        (void)ca.end();
        (void)ca.constBegin();
        (void)ca.constEnd();
        (void)(ca == b);
        (void)(ca != b);
        (void)a.begin();       // 非 const 对象上的 begin()：同样不该 detach
        (void)a.constBegin();

        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 2L);
    }

    PK_COMPARE(a.size(), 3);
    PK_VERIFY(a == b);
}

void PkSetTest::everyWriterDetaches()
{
    using S = PkSet<int>;

    static const PkSetCowCase cases[] = {
        {"insert(新元素)", [](S &s) { (void)s.insert(9); }, true},
        {"insert(已有元素)", [](S &s) { (void)s.insert(1); }, true},
        {"remove(命中)", [](S &s) { (void)s.remove(1); }, true},
        {"remove(不命中)", [](S &s) { (void)s.remove(99); }, true},
        {"clear", [](S &s) { s.clear(); }, true},
        {"unite", [](S &s) { S o{7}; (void)s.unite(o); }, true},
        {"intersect", [](S &s) { S o{1, 2}; (void)s.intersect(o); }, true},
        {"subtract", [](S &s) { S o{1}; (void)s.subtract(o); }, true},
        {"operator|=", [](S &s) { S o{7}; s |= o; }, true},

        // 反面：const 路径一律不 detach。QSet 的元素不可写，**begin()/end()
        // 也在这一侧**（PkMap/PkHash 的非 const begin() 则相反，要 detach）。
        {"begin()", [](S &s) { (void)s.begin(); }, false},
        {"end()", [](S &s) { (void)s.end(); }, false},
        {"constBegin()", [](S &s) { (void)s.constBegin(); }, false},
        {"constEnd()", [](S &s) { (void)s.constEnd(); }, false},
        {"size()", [](S &s) { (void)s.size(); }, false},
        {"count()", [](S &s) { (void)s.count(); }, false},
        {"isEmpty()", [](S &s) { (void)s.isEmpty(); }, false},
        {"contains()", [](S &s) { (void)s.contains(1); }, false},
        {"values()", [](S &s) { (void)s.values(); }, false},
        {"toList()", [](S &s) { (void)s.toList(); }, false},
        {"operator|(不改自己)", [](S &s) { S o{7}; (void)(s | o); }, false},
    };

    for (const PkSetCowCase &c : cases) {
        PkSet<int> a{1, 2, 3};
        PkSet<int> b(a);
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
        PK_VERIFY2(a.contains(1), c.name);
        PK_VERIFY2(a.contains(2), c.name);
        PK_VERIFY2(a.contains(3), c.name);
        PK_VERIFY2(!a.contains(9), c.name);
    }
}

void PkSetTest::selfAssignment()
{
    PkSet<int> a{1, 2, 3};
    // 经引用绕一道：直接写 a = a 会被 -Wself-assign-overloaded 拦下，
    // 而真实调用点里的自赋值本来就是通过别名/引用发生的。
    PkSet<int> &alias = a;
    a = alias;

    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_VERIFY((sortedOf(a) == std::vector<int>{1, 2, 3}));

    // 自赋值之后照常可写
    a.insert(4);
    PK_COMPARE(a.size(), 4);
    PK_COMPARE(a.PkUseCount(), 1L);

    // 共享状态下的自赋值：共享关系与内容都不该被破坏
    PkSet<int> b{5, 6};
    PkSet<int> c(b);
    PkSet<int> &bAlias = b;
    b = bAlias;
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(b.PkIsSharedWith(c));
    PK_COMPARE(b.size(), 2);

    // 自赋值之后 COW 仍然生效
    b.insert(7);
    PK_VERIFY(!b.PkIsSharedWith(c));
    PK_COMPARE(c.size(), 2);
}

void PkSetTest::moveLeavesSourceUsable()
{
    PkSet<int> a{1, 2};
    PkSet<int> b(std::move(a));

    PK_COMPARE(b.size(), 2);
    PK_VERIFY(b.contains(1));
    PK_COMPARE(b.PkUseCount(), 1L);

    // 源：空容器，且完全可用
    PK_COMPARE(a.size(), 0);
    PK_VERIFY(a.isEmpty());
    // 不断言 moved-from 的 PkUseCount()：源拿到的是 PkArrayData 进程内共享的
    // 空哨兵（这样移动才能 noexcept + 零分配，与 Qt 的 sharedNull 同构），
    // 计数是「1 + 当前活着的 moved-from 个数」，会随别处的测试浮动。
    PK_VERIFY(a.PkUseCount() >= 1L);
    PK_VERIFY(a.constBegin() == a.constEnd());
    a.insert(42);
    PK_COMPARE(a.PkUseCount(), 1L);   // 写入让源从哨兵上 detach 出来，成为独占
    PK_COMPARE(a.size(), 1);
    PK_COMPARE(b.size(), 2);
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 移动赋值同样
    PkSet<int> c{7};
    PkSet<int> d{0};
    d = std::move(c);
    PK_COMPARE(d.size(), 1);
    PK_VERIFY(d.contains(7));
    PK_COMPARE(c.size(), 0);
    c.insert(5);
    PK_COMPARE(c.size(), 1);
    PK_COMPARE(d.size(), 1);

    // 移动是 O(1)
    PkSet<PkSetElem> e;
    e.insert(PkSetElem(1));
    e.insert(PkSetElem(2));
    PkSetElem::s_copies = 0;
    PkSet<PkSetElem> f(std::move(e));
    PK_COMPARE(PkSetElem::s_copies, 0);
    PK_COMPARE(f.size(), 2);
    PK_COMPARE(e.size(), 0);
}

// 自定义 qHash 经 ADL 命中。Krita 的真实样例就是
// kis_paintop_lod_limitations.h：同一个文件里既定义 `uint qHash(const KoID &)`
// 又声明 `QSet<KoID> limitations;`——这条链断了那个文件当场编不过。
void PkSetTest::customQHashViaAdl()
{
    PkSet<PkTagged> s;

    const PkTagged a{1, 1};
    const PkTagged b{1, 2};   // 与 a 哈希相同（我们的 qHash 只看 group），但不相等
    const PkTagged c{2, 0};

    s.insert(a);
    s.insert(b);
    s.insert(c);
    PK_COMPARE(s.size(), 3);

    // 哈希冲突下靠 operator== 区分，两个元素没有被并成一个
    PK_VERIFY(s.contains(a));
    PK_VERIFY(s.contains(b));
    PK_VERIFY(s.contains(c));
    PK_VERIFY(!s.contains(PkTagged{9, 9}));

    // 去重仍然按 operator== 判定
    s.insert(PkTagged{1, 1});
    PK_COMPARE(s.size(), 3);

    PK_VERIFY(s.remove(b));
    PK_COMPARE(s.size(), 2);
    PK_VERIFY(!s.contains(b));

    // 集合运算在自定义类型上照常
    PkSet<PkTagged> other;
    other.insert(PkTagged{3, 3});
    s.unite(other);
    PK_COMPARE(s.size(), 3);
    s.intersect(other);
    PK_COMPARE(s.size(), 1);
    PK_VERIFY(s.contains(PkTagged{3, 3}));

    // **我们的重载真的被调到了**，不是某个别的东西在算哈希
    g_setHashCalls = 0;
    PkSet<PkSetCounted> counted;
    counted.insert(PkSetCounted{1});
    counted.insert(PkSetCounted{2});
    (void)counted.contains(PkSetCounted{1});
    PK_VERIFY(g_setHashCalls >= 3);
    PK_COMPARE(counted.size(), 2);

    // COW 在自定义元素类型上照常
    PkSet<PkTagged> shared(s);
    PK_VERIFY(s.PkIsSharedWith(shared));
    shared.insert(PkTagged{7, 7});
    PK_VERIFY(!s.PkIsSharedWith(shared));
    PK_COMPARE(s.size(), 1);
}

// PkSet<PkString>：重载写在 pk/container/PkStringHash.h，**没有改 pk/string/**。
void PkSetTest::pkStringElement()
{
    PkSet<PkString> s;

    s.insert(PkString("alpha"));
    s.insert(PkString("beta"));
    PK_COMPARE(s.size(), 2);
    PK_VERIFY(s.contains(PkString("alpha")));
    PK_VERIFY(!s.contains(PkString("missing")));

    // 相等的两个 PkString（不同实例）必须去重成一个 —— 哈希只看内容
    const PkString sameContent = PkString("alph") + PkString("a");
    PK_VERIFY(PkString("alpha") == sameContent);
    PK_COMPARE(qHash(PkString("alpha")), qHash(sameContent));
    s.insert(sameContent);
    PK_COMPARE(s.size(), 2);

    // 空串也是合法元素
    s.insert(PkString());
    PK_COMPARE(s.size(), 3);
    PK_VERIFY(s.contains(PkString()));

    PK_VERIFY(s.remove(PkString("beta")));
    PK_COMPARE(s.size(), 2);

    // 集合运算
    PkSet<PkString> other;
    other.insert(PkString("alpha"));
    other.insert(PkString("gamma"));
    s.intersect(other);
    PK_COMPARE(s.size(), 1);
    PK_VERIFY(s.contains(PkString("alpha")));
}

PK_TEST_MAIN(PkSetTest)
