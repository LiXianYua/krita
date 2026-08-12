#include "PkHashTest.h"

#include "../PkHash.h"
#include "../PkMap.h"
#include "../PkStringHash.h"

#include "PkAssocTestShared.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// PkTestBinder<PkHashTest> 特化由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发）。显式特化必须在 qExec<PkHashTest> 实例化前对本 TU 可见，所以像 moc 的
// `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkHashTest.inc"

namespace {

using IntHash = PkHash<int, int>;

// ---- 契约的编译期部分（签名形状，不是行为）----

static_assert(std::is_same<decltype(std::declval<const IntHash &>().size()), int>::value,
              "size() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<IntHash &>()[0]), int &>::value,
              "非 const operator[] 必须返回 V&");
static_assert(!std::is_reference<decltype(std::declval<const IntHash &>()[0])>::value,
              "const operator[] 必须按值返回");
static_assert(std::is_same<decltype(std::declval<IntHash &>().remove(0)), int>::value,
              "remove() 必须返回 int（删掉的个数）");
static_assert(std::is_same<decltype(std::declval<IntHash &>().insert(0, 0)),
                           IntHash::iterator>::value,
              "insert() 必须返回 iterator");

// **迭代器解引用得 value，不是 pair**
static_assert(std::is_same<decltype(*std::declval<IntHash::iterator &>()), int &>::value,
              "iterator 的 *it 必须是 V&");
static_assert(std::is_same<decltype(*std::declval<IntHash::const_iterator &>()),
                           const int &>::value,
              "const_iterator 的 *it 必须是 const V&");
static_assert(std::is_convertible<IntHash::iterator, IntHash::const_iterator>::value,
              "iterator 必须能隐式转成 const_iterator");

static_assert(std::is_copy_constructible<IntHash>::value, "拷贝构造必须存在");
static_assert(std::is_copy_assignable<IntHash>::value, "拷贝赋值必须存在");
static_assert(std::is_move_constructible<IntHash>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntHash>::value, "移动赋值必须存在");

// QHash **没有** lowerBound/upperBound（那是 QMap 靠有序性给的）。
// 探测惯用法把"以后别长出来"钉在编译期。
template <typename M, typename = void>
struct PkHasLowerBound : std::false_type {
};
template <typename M>
struct PkHasLowerBound<
    M, std::void_t<decltype(std::declval<M &>().lowerBound(std::declval<int>()))>>
    : std::true_type {
};

static_assert(!PkHasLowerBound<IntHash>::value, "QHash 没有 lowerBound，PkHash 不得提供");
// 反面对照：探测惯用法本身没写坏 —— QMap 确实有。
static_assert(PkHasLowerBound<PkMap<int, int>>::value,
              "QMap 有 lowerBound：探测惯用法若这里为假，说明它整个失灵了");

// ---------------------------------------------------------------------------
// 自定义 qHash 的测试类型 —— **本任务最容易在集成时才炸的一条**。
//
// Krita 全仓有 18 处形如 `uint qHash(const KoID &id)` 的自定义重载，散落在
// Krita 自己的头文件里，S 线替换调用点时**原样保留**。PkHash/PkSet 必须能找到
// 它们，靠的是 PkHasher 里 `qHash(k)` 这个依赖调用在**实例化点的 ADL**。
//
// 这里把类型与它的 qHash 一起放进**匿名命名空间**，正是为了证明走的是 ADL：
// 匿名命名空间里的 qHash 在 PkHasher 的模板定义点（PkHashFunctions.h）根本
// 不可见，普通查找找不到它；能找到只可能是因为 PkKeyed 的关联命名空间被纳入了。
// ---------------------------------------------------------------------------
struct PkKeyed
{
    int a = 0;
    int b = 0;

    bool operator==(const PkKeyed &o) const { return a == o.a && b == o.b; }
};

// 故意用一个**很差**但确定的哈希：只看 a，让 {1,1} 与 {1,2} 必然冲突。
// 哈希冲突下 operator== 才是决定相等的那一环，顺带把这条链也压到。
inline unsigned int qHash(const PkKeyed &k)
{
    return static_cast<unsigned int>(k.a);
}

// 计数器：证明容器**真的**调了我们这个重载，而不是某个别的东西。
int g_customHashCalls = 0;

struct PkCounted
{
    int v = 0;
    bool operator==(const PkCounted &o) const { return v == o.v; }
};

inline unsigned int qHash(const PkCounted &k)
{
    ++g_customHashCalls;
    return static_cast<unsigned int>(k.v);
}

// 枚举 key：没有 PkHashFunctions.h 里那条 enum 重载的话，这里会在一堆整型
// 重载之间报歧义。QHash<某枚举, V> 在 Krita 里是常见写法。
enum class PkColor { Red, Green, Blue };

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：逐条在 PkHash 上实例化 tests/PkAssocTestShared.h 里的用例
// ---------------------------------------------------------------------------

void PkHashTest::lookupAndDefaults() { pkAssocTestLookupAndDefaults<PkHash>(); }
void PkHashTest::subscript() { pkAssocTestSubscript<PkHash>(); }
void PkHashTest::insertTakeRemove() { pkAssocTestInsertTakeRemove<PkHash>(); }
void PkHashTest::iteratorShape() { pkAssocTestIteratorShape<PkHash>(); }
void PkHashTest::iteratorTraversal() { pkAssocTestIteratorTraversal<PkHash>(); }
void PkHashTest::iteratorConversion() { pkAssocTestIteratorConversion<PkHash>(); }
void PkHashTest::findAndErase() { pkAssocTestFindAndErase<PkHash>(); }
void PkHashTest::keysAndValues() { pkAssocTestKeysAndValues<PkHash>(); }
void PkHashTest::comparison() { pkAssocTestComparison<PkHash>(); }
void PkHashTest::cowIsolation() { pkAssocTestCowIsolation<PkHash>(); }
void PkHashTest::copyIsConstantTime() { pkAssocTestCopyIsConstantTime<PkHash>(); }
void PkHashTest::constNeverDetaches() { pkAssocTestConstNeverDetaches<PkHash>(); }
void PkHashTest::iteratorDetachTiming() { pkAssocTestIteratorDetachTiming<PkHash>(); }
void PkHashTest::everyWriterDetaches() { pkAssocTestEveryWriterDetaches<PkHash>(); }
void PkHashTest::selfAssignment() { pkAssocTestSelfAssignment<PkHash>(); }
void PkHashTest::moveLeavesSourceUsable() { pkAssocTestMoveLeavesSourceUsable<PkHash>(); }

// ---------------------------------------------------------------------------
// PkHash 专有
// ---------------------------------------------------------------------------

// 内建类型的 qHash 重载：Qt 本来就提供这些，调用点指望它们存在。
// 这里主要是**编译期**的断言（每种 key 类型都要能实例化出一个 PkHash），
// 顺带压几条运行期不变量。
void PkHashTest::builtinQHash()
{
    // 整型家族
    PkHash<int, int> i;
    i.insert(-7, 1);
    PK_VERIFY(i.contains(-7));

    PkHash<unsigned int, int> u;
    u.insert(7u, 1);
    PK_VERIFY(u.contains(7u));

    PkHash<long, int> l;
    l.insert(-7L, 1);
    PK_VERIFY(l.contains(-7L));

    PkHash<unsigned long long, int> ull;
    ull.insert(0xffffffffffULL, 1);
    PK_VERIFY(ull.contains(0xffffffffffULL));

    PkHash<short, int> s;
    s.insert(static_cast<short>(-3), 1);
    PK_VERIFY(s.contains(static_cast<short>(-3)));

    PkHash<bool, int> b;
    b.insert(true, 1);
    b.insert(false, 2);
    PK_COMPARE(b.size(), 2);
    PK_COMPARE(b.value(true), 1);
    PK_COMPARE(b.value(false), 2);

    PkHash<char, int> c;
    c.insert('x', 1);
    PK_VERIFY(c.contains('x'));

    // 指针
    int one = 1;
    int two = 2;
    PkHash<const int *, int> p;
    p.insert(&one, 10);
    p.insert(&two, 20);
    PK_COMPARE(p.size(), 2);
    PK_COMPARE(p.value(&one), 10);
    PK_COMPARE(p.value(&two), 20);
    PK_VERIFY(!p.contains(nullptr));

    // 浮点：**-0.0 与 +0.0 必须落到同一个桶**，否则 `-0.0 == 0.0` 为真而
    // contains 为假，哈希容器的不变量当场破掉。
    PkHash<double, int> d;
    d.insert(0.0, 1);
    PK_VERIFY(d.contains(-0.0));
    PK_COMPARE(d.value(-0.0), 1);
    d.insert(-0.0, 2);
    PK_COMPARE(d.size(), 1);
    PK_COMPARE(d.value(0.0), 2);
    d.insert(1.5, 3);
    PK_COMPARE(d.size(), 2);
    PK_COMPARE(d.value(1.5), 3);

    PkHash<float, int> f;
    f.insert(0.0f, 1);
    PK_VERIFY(f.contains(-0.0f));

    // 枚举：靠 PkHashFunctions.h 里那条 enum 重载，否则在整型重载之间报歧义
    PkHash<PkColor, int> e;
    e.insert(PkColor::Red, 1);
    e.insert(PkColor::Blue, 3);
    PK_COMPARE(e.size(), 2);
    PK_COMPARE(e.value(PkColor::Red), 1);
    PK_COMPARE(e.value(PkColor::Blue), 3);
    PK_COMPARE(e.value(PkColor::Green), 0);
    PK_VERIFY(!e.contains(PkColor::Green));
}

// 自定义 qHash 经 ADL 命中 —— 这条通不了，S 线替换调用点时那 18 处自定义
// 重载全部作废，而且是到集成时才炸。
void PkHashTest::customQHashViaAdl()
{
    PkHash<PkKeyed, std::string> h;

    const PkKeyed k11{1, 1};
    const PkKeyed k12{1, 2};   // 与 k11 哈希相同（我们的 qHash 只看 a），但不相等
    const PkKeyed k20{2, 0};

    h.insert(k11, "one-one");
    h.insert(k12, "one-two");
    h.insert(k20, "two-zero");

    // 存得进、取得出
    PK_COMPARE(h.size(), 3);
    PK_VERIFY(h.value(k11) == "one-one");
    PK_VERIFY(h.value(k12) == "one-two");
    PK_VERIFY(h.value(k20) == "two-zero");

    // 哈希冲突下靠 operator== 区分，两个 key 没有被并成一个
    PK_VERIFY(h.contains(k11));
    PK_VERIFY(h.contains(k12));
    PK_COMPARE(h.count(k11), 1);

    // 覆盖与删除照常
    h.insert(k11, "rewritten");
    PK_COMPARE(h.size(), 3);
    PK_VERIFY(h.value(k11) == "rewritten");
    PK_COMPARE(h.remove(k12), 1);
    PK_COMPARE(h.size(), 2);
    PK_VERIFY(!h.contains(k12));

    // 找不到的 key 返回 V()
    PK_VERIFY(h.value(PkKeyed{9, 9}) == std::string());

    // **我们的重载真的被调到了**，不是某个别的东西在算哈希
    g_customHashCalls = 0;
    PkHash<PkCounted, int> counted;
    counted.insert(PkCounted{1}, 10);
    counted.insert(PkCounted{2}, 20);
    (void)counted.contains(PkCounted{1});
    (void)counted.value(PkCounted{2});
    PK_VERIFY(g_customHashCalls >= 4);
    PK_COMPARE(counted.value(PkCounted{1}), 10);

    // COW 在自定义 key 类型上照常
    PkHash<PkKeyed, std::string> shared(h);
    PK_VERIFY(h.PkIsSharedWith(shared));
    shared.insert(PkKeyed{7, 7}, "new");
    PK_VERIFY(!h.PkIsSharedWith(shared));
    PK_COMPARE(h.size(), 2);
}

// PkHash<PkString, V>：重载写在 pk/container/PkStringHash.h，**没有改 pk/string/**。
void PkHashTest::pkStringKey()
{
    PkHash<PkString, int> h;

    h.insert(PkString("alpha"), 1);
    h.insert(PkString("beta"), 2);
    PK_COMPARE(h.size(), 2);
    PK_COMPARE(h.value(PkString("alpha")), 1);
    PK_COMPARE(h.value(PkString("beta")), 2);
    PK_COMPARE(h.value(PkString("missing")), 0);
    PK_VERIFY(h.contains(PkString("alpha")));
    PK_VERIFY(!h.contains(PkString("missing")));

    // 相等的两个 PkString（不同实例）必须落到同一项 —— 哈希只看内容
    const PkString a("alpha");
    const PkString sameContent = PkString("alph") + PkString("a");
    PK_VERIFY(a == sameContent);
    PK_COMPARE(qHash(a), qHash(sameContent));
    PK_COMPARE(h.value(sameContent), 1);

    // 覆盖，不是多值
    h.insert(sameContent, 99);
    PK_COMPARE(h.size(), 2);
    PK_COMPARE(h.value(a), 99);

    // 空串也是合法 key
    h.insert(PkString(), 7);
    PK_COMPARE(h.size(), 3);
    PK_COMPARE(h.value(PkString()), 7);

    PK_COMPARE(h.remove(PkString("beta")), 1);
    PK_COMPARE(h.size(), 2);
}

// 试接目标 libs/image/tests/kis_fill_interval_map_test.cpp 的真实用法：
// `QHash<int, QMap<int, POD>>` 嵌套 + 对 QHash::iterator 用 operator-> 拿到里层
// QMap 再调它的方法 + `it->field` 直达 POD 的成员。
//
// **调研时第一次用 std::map 裸包做垫片，就是在这个形状上编译失败的**
// （std::map 的 `*it` 是 pair，`it->insert(...)` 会解析成 pair 的成员）。
void PkHashTest::nestedHashOfMap()
{
    using RowMap = PkMap<int, PkAssocPod>;
    PkHash<int, RowMap> rows;

    // 外层：非 const operator[] 对缺失的 key 插入一个默认构造的里层 PkMap，
    // 返回可写引用 —— 这正是调用点建表的写法。
    rows[10].insert(3, PkAssocPod{3, 7});
    rows[10].insert(1, PkAssocPod{1, 2});
    rows[20].insert(5, PkAssocPod{5, 9});

    PK_COMPARE(rows.size(), 2);
    PK_COMPARE(rows.value(10).size(), 2);

    // **对 PkHash::iterator 用 operator-> 拿到里层 PkMap 再调它的方法**
    auto rowIt = rows.find(10);
    PK_VERIFY(rowIt != rows.end());
    rowIt->insert(2, PkAssocPod{2, 4});
    PK_COMPARE(rowIt->size(), 3);
    PK_COMPARE(rows.value(10).size(), 3);
    PK_VERIFY(rowIt->contains(2));

    // 里层迭代器：`it->field` 直达 POD 的成员（不是 pair 的 first/second）
    auto inner = rowIt->find(3);
    PK_VERIFY(inner != rowIt->end());
    PK_COMPARE(inner->begin, 3);
    PK_COMPARE(inner->end, 7);
    PK_COMPARE(inner.key(), 3);
    PK_COMPARE((*inner).begin, 3);

    // 经 operator-> 写回去
    inner->end = 77;
    PK_COMPARE(rows.value(10).value(3).end, 77);

    // 迭代器相等比较（试接目标里的 `QCOMPARE(range.beginIt, range.endIt)`）
    const RowMap &row10 = rows.value(10);
    PK_COMPARE(row10.constBegin(), row10.constBegin());
    PK_VERIFY(row10.constBegin() != row10.constEnd());

    // 里层的有序性：PkMap 按 key 升序
    std::vector<int> innerKeys;
    for (auto it = row10.constBegin(); it != row10.constEnd(); ++it) {
        innerKeys.push_back(it.key());
    }
    PK_VERIFY((innerKeys == std::vector<int>{1, 2, 3}));

    // 里层 PkMap 作为 value 时 COW 照常：拷外层不深拷里层
    PkHash<int, RowMap> copy(rows);
    PK_VERIFY(rows.PkIsSharedWith(copy));
    copy[10].insert(9, PkAssocPod{9, 9});
    PK_VERIFY(!rows.PkIsSharedWith(copy));
    PK_COMPARE(rows.value(10).size(), 3);
    PK_COMPARE(copy.value(10).size(), 4);

    // const 路径上取里层再遍历
    const PkHash<int, RowMap> &cRows = rows;
    auto cRowIt = cRows.constFind(20);
    PK_VERIFY(cRowIt != cRows.constEnd());
    PK_COMPARE(cRowIt->size(), 1);
    PK_COMPARE(cRowIt->value(5).end, 9);
}

// QHash 没有 lowerBound/upperBound —— 断言全在文件头的 static_assert 里，
// 这个槽只是给它一个会被 harness 执行到的名字（否则那些断言的意图无处可查）。
void PkHashTest::noOrderedApi()
{
    // 模板实参里的逗号会被预处理器当成宏参数分隔符，所以整体再套一层括号。
    PK_VERIFY((!PkHasLowerBound<IntHash>::value));
    PK_VERIFY((PkHasLowerBound<PkMap<int, int>>::value));
}

PK_TEST_MAIN(PkHashTest)
