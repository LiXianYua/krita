#include "PkMapTest.h"

#include "../PkMap.h"

#include "PkAssocTestShared.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// PkTestBinder<PkMapTest> 特化由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发）。显式特化必须在 qExec<PkMapTest> 实例化前对本 TU 可见，所以像 moc 的
// `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkMapTest.inc"

namespace {

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntMap = PkMap<int, int>;
using StrMap = PkMap<int, std::string>;

// ---- 契约的编译期部分（签名形状，不是行为）----

static_assert(std::is_same<decltype(std::declval<const IntMap &>().size()), int>::value,
              "size() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntMap &>().count()), int>::value,
              "count() 必须返回 int");

// 非 const operator[] 返回**可写引用**（它是写操作：缺失的 key 会被插入）；
// const 版**按值**返回（Qt 的签名是 `const T operator[](const Key &) const`，
// 缺失的 key 没有可指的对象）。
static_assert(std::is_same<decltype(std::declval<IntMap &>()[0]), int &>::value,
              "非 const operator[] 必须返回 V&");
// 用「不是引用」而不是「恰好是 const int」来断言：非类类型的 prvalue 会被剥掉
// cv 限定，`decltype` 在这一格各家实现的口径不完全一致，断言成 const int 会变成
// 一条只在某些编译器上成立的假约束。真正要钉住的是**按值**。
static_assert(!std::is_reference<decltype(std::declval<const IntMap &>()[0])>::value,
              "const operator[] 必须按值返回（缺失的 key 没有可指的对象）");

// value()/take()/key() 按值返回（缺失时要能返回 V()/K()，返回引用就没有可指的对象）
static_assert(std::is_same<decltype(std::declval<const IntMap &>().value(0)), int>::value,
              "value() 必须按值返回");
static_assert(std::is_same<decltype(std::declval<IntMap &>().take(0)), int>::value,
              "take() 必须按值返回");
static_assert(std::is_same<decltype(std::declval<const IntMap &>().key(0)), int>::value,
              "key() 必须按值返回");
// remove() 返回删掉几个（QMap/QHash 是 int；QSet 才是 bool，别抄混）
static_assert(std::is_same<decltype(std::declval<IntMap &>().remove(0)), int>::value,
              "remove() 必须返回 int（删掉的个数）");
// insert() 返回 iterator（Qt 语义）
static_assert(std::is_same<decltype(std::declval<IntMap &>().insert(0, 0)),
                           IntMap::iterator>::value,
              "insert() 必须返回 iterator");

// keys()/values() 返回 PkList
static_assert(std::is_same<decltype(std::declval<const StrMap &>().keys()), PkList<int>>::value,
              "keys() 必须返回 PkList<K>");
static_assert(std::is_same<decltype(std::declval<const StrMap &>().values()),
                           PkList<std::string>>::value,
              "values() 必须返回 PkList<V>");

// **迭代器解引用得 value，不是 pair** —— Qt 与 STL 关联容器最根本的差异。
static_assert(std::is_same<decltype(*std::declval<IntMap::iterator &>()), int &>::value,
              "iterator 的 *it 必须是 V&");
static_assert(std::is_same<decltype(*std::declval<IntMap::const_iterator &>()),
                           const int &>::value,
              "const_iterator 的 *it 必须是 const V&");
static_assert(std::is_convertible<IntMap::iterator, IntMap::const_iterator>::value,
              "iterator 必须能隐式转成 const_iterator");
static_assert(!std::is_convertible<IntMap::const_iterator, IntMap::iterator>::value,
              "const_iterator 不得能转回 iterator");

static_assert(std::is_copy_constructible<IntMap>::value, "拷贝构造必须存在");
static_assert(std::is_copy_assignable<IntMap>::value, "拷贝赋值必须存在");
static_assert(std::is_move_constructible<IntMap>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntMap>::value, "移动赋值必须存在");

// **明确不做的那批**（实测 0 调用点）：给了就是凭空多一项，违反判据①。
// 探测惯用法把"以后别长出来"钉在编译期，而不只是写在注释里。
template <typename M, typename = void>
struct PkHasInsertMulti : std::false_type {
};
template <typename M>
struct PkHasInsertMulti<
    M, std::void_t<decltype(std::declval<M &>().insertMulti(std::declval<int>(),
                                                            std::declval<int>()))>>
    : std::true_type {
};

template <typename M, typename = void>
struct PkHasUniqueKeys : std::false_type {
};
template <typename M>
struct PkHasUniqueKeys<M, std::void_t<decltype(std::declval<const M &>().uniqueKeys())>>
    : std::true_type {
};

template <typename M, typename = void>
struct PkHasEqualRange : std::false_type {
};
template <typename M>
struct PkHasEqualRange<
    M, std::void_t<decltype(std::declval<const M &>().equal_range(std::declval<int>()))>>
    : std::true_type {
};

static_assert(!PkHasInsertMulti<IntMap>::value, "insertMulti 实测 0 调用点，不得实现");
static_assert(!PkHasUniqueKeys<IntMap>::value, "uniqueKeys 实测 0 调用点，不得实现");
static_assert(!PkHasEqualRange<IntMap>::value, "equal_range 实测 0 调用点，不得实现");

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：逐条在 PkMap 上实例化 tests/PkAssocTestShared.h 里的用例
// ---------------------------------------------------------------------------

void PkMapTest::lookupAndDefaults() { pkAssocTestLookupAndDefaults<PkMap>(); }
void PkMapTest::subscript() { pkAssocTestSubscript<PkMap>(); }
void PkMapTest::insertTakeRemove() { pkAssocTestInsertTakeRemove<PkMap>(); }
void PkMapTest::iteratorShape() { pkAssocTestIteratorShape<PkMap>(); }
void PkMapTest::iteratorTraversal() { pkAssocTestIteratorTraversal<PkMap>(); }
void PkMapTest::iteratorConversion() { pkAssocTestIteratorConversion<PkMap>(); }
void PkMapTest::findAndErase() { pkAssocTestFindAndErase<PkMap>(); }
void PkMapTest::keysAndValues() { pkAssocTestKeysAndValues<PkMap>(); }
void PkMapTest::comparison() { pkAssocTestComparison<PkMap>(); }
void PkMapTest::cowIsolation() { pkAssocTestCowIsolation<PkMap>(); }
void PkMapTest::copyIsConstantTime() { pkAssocTestCopyIsConstantTime<PkMap>(); }
void PkMapTest::constNeverDetaches() { pkAssocTestConstNeverDetaches<PkMap>(); }
void PkMapTest::iteratorDetachTiming() { pkAssocTestIteratorDetachTiming<PkMap>(); }
void PkMapTest::everyWriterDetaches() { pkAssocTestEveryWriterDetaches<PkMap>(); }
void PkMapTest::selfAssignment() { pkAssocTestSelfAssignment<PkMap>(); }
void PkMapTest::moveLeavesSourceUsable() { pkAssocTestMoveLeavesSourceUsable<PkMap>(); }

// ---------------------------------------------------------------------------
// PkMap 专有
// ---------------------------------------------------------------------------

// 实测 3：`QMap keys 顺序 = 1,2,3` —— 按 key **升序**，与插入顺序无关。
void PkMapTest::orderedByKey()
{
    PkMap<int, std::string> m;
    m.insert(3, "c");
    m.insert(1, "a");
    m.insert(2, "b");

    // keys() 直接就是升序（不排序，这正是要断言的）
    const PkList<int> keys = m.keys();
    PK_COMPARE(keys.size(), 3);
    PK_COMPARE(keys.at(0), 1);
    PK_COMPARE(keys.at(1), 2);
    PK_COMPARE(keys.at(2), 3);

    // values() 跟着 key 的顺序走
    const PkList<std::string> values = m.values();
    PK_COMPARE(values.size(), 3);
    PK_VERIFY(values.at(0) == "a");
    PK_VERIFY(values.at(1) == "b");
    PK_VERIFY(values.at(2) == "c");

    // 迭代顺序同样是 key 升序
    std::vector<int> seen;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        seen.push_back(it.key());
    }
    PK_VERIFY((seen == std::vector<int>{1, 2, 3}));

    // 负数与逆序插入照样按 operator< 排
    PkMap<int, int> neg;
    neg.insert(5, 0);
    neg.insert(-5, 0);
    neg.insert(0, 0);
    const PkList<int> negKeys = neg.keys();
    PK_COMPARE(negKeys.at(0), -5);
    PK_COMPARE(negKeys.at(1), 0);
    PK_COMPARE(negKeys.at(2), 5);

    // 非整型 key：std::string 按 operator< 排
    PkMap<std::string, int> s;
    s.insert("beta", 2);
    s.insert("alpha", 1);
    const PkList<std::string> sKeys = s.keys();
    PK_VERIFY(sKeys.at(0) == "alpha");
    PK_VERIFY(sKeys.at(1) == "beta");
}

// 实测 10：表里是 {1, 3} 时
//   `lowerBound(2).key=3  upperBound(2).key=3`
//   `lowerBound(0)==begin: true   upperBound(9)==end: true`
void PkMapTest::lowerAndUpperBound()
{
    // 与实测同一张表：{1, 3}（实测里 2 已被 erase 掉）
    PkMap<int, std::string> m;
    m.insert(1, "a");
    m.insert(3, "c");

    // 落在空档里的 2：lowerBound 与 upperBound 都停在 3
    PK_COMPARE(m.lowerBound(2).key(), 3);
    PK_COMPARE(m.upperBound(2).key(), 3);

    // 小于全部：lowerBound == begin
    PK_VERIFY(m.constBegin() == m.lowerBound(0));
    PK_VERIFY(m.constBegin() == m.upperBound(0));

    // 大于全部：upperBound == end
    PK_VERIFY(m.upperBound(9) == m.constEnd());
    PK_VERIFY(m.lowerBound(9) == m.constEnd());

    // 正好命中：lowerBound 停在它自己，upperBound 跳到下一个
    PK_COMPARE(m.lowerBound(1).key(), 1);
    PK_COMPARE(m.upperBound(1).key(), 3);
    PK_COMPARE(m.lowerBound(3).key(), 3);
    PK_VERIFY(m.upperBound(3) == m.constEnd());

    // const 重载给的是 const_iterator，且**不 detach**
    const PkMap<int, std::string> &cm = m;
    static_assert(std::is_same<decltype(cm.lowerBound(0)),
                               PkMap<int, std::string>::const_iterator>::value,
                  "const lowerBound() 必须返回 const_iterator");
    PkMap<int, std::string> shared(m);
    PK_VERIFY(m.PkIsSharedWith(shared));
    const PkMap<int, std::string> &cShared = shared;
    (void)cShared.lowerBound(2);
    (void)cShared.upperBound(2);
    PK_VERIFY(m.PkIsSharedWith(shared));

    // 非 const 重载给的是可写 iterator，按实测规则**要 detach**
    (void)shared.lowerBound(2);
    PK_VERIFY(!m.PkIsSharedWith(shared));
    PK_COMPARE(m.size(), 2);

    // 空容器上两个都等于 end()
    PkMap<int, std::string> empty;
    PK_VERIFY(empty.lowerBound(0) == empty.end());
    PK_VERIFY(empty.upperBound(0) == empty.end());
}

// 实测 9：`erase(find(2)) 返回的迭代器 key=3`。
// 这条只有有序容器能逐字对上——PkHash 无序，共同用例里只能断言"不是被删的那个"。
void PkMapTest::eraseReturnsNextKey()
{
    PkMap<int, std::string> m;
    m.insert(1, "a");
    m.insert(2, "b");
    m.insert(3, "c");

    const auto next = m.erase(m.find(2));
    PK_COMPARE(next.key(), 3);
    PK_VERIFY(next.value() == "c");
    PK_COMPARE(m.size(), 2);
    PK_VERIFY(!m.contains(2));

    // 删最后一个 → 返回 end()
    PK_VERIFY(m.erase(m.find(3)) == m.end());
    PK_COMPARE(m.size(), 1);

    // 删第一个 → 返回原来的第二个（这里表里只剩 {1}，所以是 end()）
    PK_VERIFY(m.erase(m.find(1)) == m.end());
    PK_VERIFY(m.isEmpty());

    // 共享状态下 erase 之后，返回的迭代器仍然指对项（内部按 key 重新定位）
    PkMap<int, std::string> a;
    a.insert(1, "a");
    a.insert(2, "b");
    a.insert(3, "c");
    PkMap<int, std::string> b(a);
    const auto n2 = b.erase(b.constFind(1));
    PK_COMPARE(n2.key(), 2);
    PK_VERIFY(n2.value() == "b");
    PK_COMPARE(a.size(), 3);
    PK_VERIFY(a.contains(1));
}

void PkMapTest::mapWritersDetach()
{
    using M = PkMap<int, int>;

    static const PkAssocCowCase<PkMap> cases[] = {
        // 非 const 版拿到的是可写迭代器 → 按实测规则要 detach
        {"lowerBound()", [](M &m) { (void)m.lowerBound(2); }, true},
        {"upperBound()", [](M &m) { (void)m.upperBound(2); }, true},
        // 反面：const 重载绝不 detach
        {"const lowerBound()",
         [](M &m) {
             const M &cm = m;
             (void)cm.lowerBound(2);
         },
         false},
        {"const upperBound()",
         [](M &m) {
             const M &cm = m;
             (void)cm.upperBound(2);
         },
         false},
    };

    pkAssocRunCowCases<PkMap>(cases);
}

PK_TEST_MAIN(PkMapTest)
