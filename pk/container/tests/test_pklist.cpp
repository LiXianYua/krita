#include "PkListTest.h"

#include "../PkList.h"

#include "PkSeqTestShared.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

// PkTestBinder<PkListTest> 特化由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发）。显式特化必须在 qExec<PkListTest> 实例化前对本 TU 可见，所以像 moc 的
// `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkListTest.inc"

namespace {

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntList = PkList<int>;

// ---- 契约的编译期部分（签名形状，不是行为）----

static_assert(std::is_same<decltype(std::declval<const IntList &>().size()), int>::value,
              "size() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntList &>().count()), int>::value,
              "count() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntList &>().at(0)), const int &>::value,
              "at() 必须返回 const T&");
static_assert(std::is_same<decltype(std::declval<IntList &>()[0]), int &>::value,
              "非 const operator[] 必须返回 T&");
static_assert(std::is_same<decltype(std::declval<const IntList &>().value(0)), int>::value,
              "value() 必须按值返回");

// Qt 的 removeAll 返回删掉几个、removeOne 返回删没删掉、takeAt 按值返回。
static_assert(std::is_same<decltype(std::declval<IntList &>().removeAll(0)), int>::value,
              "removeAll() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<IntList &>().removeOne(0)), bool>::value,
              "removeOne() 必须返回 bool");
static_assert(std::is_same<decltype(std::declval<IntList &>().takeAt(0)), int>::value,
              "takeAt() 必须按值返回 T");

// **Qt5 的 QList 没有 remove(int)**（只有 removeAt；remove 是 QVector 的）。
// 于是 `QList::remove(...)` 在 Qt 下根本编不过 —— 调用点里不可能存在这种写法，
// 我们给出它就是凭空多一项零调用点的 API，违反线级 spec 判据①。
//
// 这条断言不是注释：它用探测惯用法把"PkList 不许长出 remove(int)"钉在编译期。
// 光删掉实现不够 —— 以后谁把它挪回共同基类 PkArrayContainer，这里会立刻报错。
template <typename L, typename = void>
struct PkHasRemoveInt : std::false_type {
};
template <typename L>
struct PkHasRemoveInt<L, std::void_t<decltype(std::declval<L &>().remove(0))>>
    : std::true_type {
};

static_assert(!PkHasRemoveInt<PkList<int>>::value,
              "QList 没有 remove(int)：PkList 不得提供它（只有 removeAt）");
static_assert(!PkHasRemoveInt<PkList<std::string>>::value,
              "QList 没有 remove(int)：PkList 不得提供它（只有 removeAt）");
// 反面对照：探测惯用法本身没写坏 —— QVector 确实有 remove(int)。
static_assert(PkHasRemoveInt<PkVector<int>>::value,
              "QVector 有 remove(int)：探测惯用法若这里为假，说明它整个失灵了");

static_assert(std::is_copy_constructible<IntList>::value, "拷贝构造必须存在");
static_assert(std::is_copy_assignable<IntList>::value, "拷贝赋值必须存在");
static_assert(std::is_move_constructible<IntList>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntList>::value, "移动赋值必须存在");
static_assert(noexcept(std::declval<IntList &>().swap(std::declval<IntList &>())),
              "swap() 必须 noexcept");

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：逐条在 PkList 上实例化 tests/PkSeqTestShared.h 里的用例
// ---------------------------------------------------------------------------

void PkListTest::sizeIsInt() { pkSeqTestSizeIsInt<PkList>(); }
void PkListTest::sizeAndEmptiness() { pkSeqTestSizeAndEmptiness<PkList>(); }
void PkListTest::elementAccess() { pkSeqTestElementAccess<PkList>(); }
void PkListTest::valueOutOfRange() { pkSeqTestValueOutOfRange<PkList>(); }
void PkListTest::appendAndPrepend() { pkSeqTestAppendAndPrepend<PkList>(); }
void PkListTest::insertAndRemove() { pkSeqTestInsertAndRemove<PkList>(); }
void PkListTest::erase() { pkSeqTestErase<PkList>(); }
void PkListTest::search() { pkSeqTestSearch<PkList>(); }
void PkListTest::iterators() { pkSeqTestIterators<PkList>(); }
void PkListTest::constIteratorsDoNotDetach() { pkSeqTestConstIteratorsDoNotDetach<PkList>(); }
void PkListTest::comparison() { pkSeqTestComparison<PkList>(); }
void PkListTest::streamOperators() { pkSeqTestStreamOperators<PkList>(); }
void PkListTest::cowIsolation() { pkSeqTestCowIsolation<PkList>(); }
void PkListTest::copyIsConstantTime() { pkSeqTestCopyIsConstantTime<PkList>(); }
void PkListTest::constNeverDetaches() { pkSeqTestConstNeverDetaches<PkList>(); }
void PkListTest::everyWriterDetaches() { pkSeqTestEveryWriterDetaches<PkList>(); }
void PkListTest::reserveDetachRules() { pkSeqTestReserveDetachRules<PkList>(); }
void PkListTest::swap() { pkSeqTestSwap<PkList>(); }
void PkListTest::selfAssignment() { pkSeqTestSelfAssignment<PkList>(); }
void PkListTest::moveLeavesSourceUsable() { pkSeqTestMoveLeavesSourceUsable<PkList>(); }
void PkListTest::initializerListAndDefaults() { pkSeqTestInitializerListAndDefaults<PkList>(); }

// ---------------------------------------------------------------------------
// PkList 专有
// ---------------------------------------------------------------------------

void PkListTest::removeAtAllOne()
{
    PkList<int> l{1, 2, 3, 2, 1};

    l.removeAt(2);
    PK_VERIFY((l == PkList<int>{1, 2, 2, 1}));

    // removeAll 返回删掉的个数
    PK_COMPARE(l.removeAll(2), 2);
    PK_VERIFY((l == PkList<int>{1, 1}));
    PK_COMPARE(l.removeAll(9), 0);
    PK_VERIFY((l == PkList<int>{1, 1}));
    PK_COMPARE(l.removeAll(1), 2);
    PK_VERIFY(l.isEmpty());

    // removeOne 只删第一个，返回删没删掉
    PkList<int> m{1, 2, 1, 2};
    PK_VERIFY(m.removeOne(2));
    PK_VERIFY((m == PkList<int>{1, 1, 2}));
    PK_VERIFY(!m.removeOne(9));
    PK_VERIFY((m == PkList<int>{1, 1, 2}));

    // 参数指向自身元素时不得悬垂（Qt 先取一份副本，我们照做）
    PkList<int> alias{5, 6, 5};
    PK_COMPARE(alias.removeAll(alias.at(0)), 2);
    PK_VERIFY((alias == PkList<int>{6}));

    PkList<int> alias2{7, 8, 7};
    PK_VERIFY(alias2.removeOne(alias2.at(2)));
    PK_VERIFY((alias2 == PkList<int>{8, 7}));
}

void PkListTest::removeFirstLast()
{
    PkList<int> l{1, 2, 3};

    l.removeFirst();
    PK_VERIFY((l == PkList<int>{2, 3}));
    l.removeLast();
    PK_VERIFY((l == PkList<int>{2}));
    l.removeFirst();
    PK_VERIFY(l.isEmpty());

    // 清空之后照常可用
    l.append(9);
    PK_VERIFY((l == PkList<int>{9}));
}

void PkListTest::takeAtFirstLast()
{
    PkList<int> l{1, 2, 3, 4};

    PK_COMPARE(l.takeAt(1), 2);
    PK_VERIFY((l == PkList<int>{1, 3, 4}));

    PK_COMPARE(l.takeFirst(), 1);
    PK_VERIFY((l == PkList<int>{3, 4}));

    PK_COMPARE(l.takeLast(), 4);
    PK_VERIFY((l == PkList<int>{3}));

    PK_COMPARE(l.takeAt(0), 3);
    PK_VERIFY(l.isEmpty());

    // 非平凡元素类型：取走的是内容，不是悬垂引用
    PkList<std::string> s{std::string("a"), std::string("b")};
    const std::string taken = s.takeFirst();
    PK_VERIFY(taken == "a");
    PK_COMPARE(s.size(), 1);
    PK_VERIFY(s.at(0) == "b");
}

void PkListTest::popBackFront()
{
    PkList<int> l{1, 2, 3};

    l.pop_back();
    PK_VERIFY((l == PkList<int>{1, 2}));
    l.pop_front();
    PK_VERIFY((l == PkList<int>{2}));
    l.pop_back();
    PK_VERIFY(l.isEmpty());
}

void PkListTest::moveElement()
{
    PkList<int> l{1, 2, 3, 4};

    // 往后搬：先摘出来，再插到目标下标
    l.move(0, 2);
    PK_VERIFY((l == PkList<int>{2, 3, 1, 4}));

    // 往前搬
    PkList<int> m{1, 2, 3, 4};
    m.move(3, 0);
    PK_VERIFY((m == PkList<int>{4, 1, 2, 3}));
}

void PkListTest::moveElementBoundaries()
{
    // from == to：内容不变
    PkList<int> same{1, 2, 3};
    same.move(1, 1);
    PK_VERIFY((same == PkList<int>{1, 2, 3}));
    same.move(0, 0);
    PK_VERIFY((same == PkList<int>{1, 2, 3}));

    // 相邻
    PkList<int> adj{1, 2, 3, 4};
    adj.move(1, 2);
    PK_VERIFY((adj == PkList<int>{1, 3, 2, 4}));
    PkList<int> adj2{1, 2, 3, 4};
    adj2.move(2, 1);
    PK_VERIFY((adj2 == PkList<int>{1, 3, 2, 4}));

    // 首尾对调（两步）
    PkList<int> ends{1, 2, 3, 4};
    ends.move(0, ends.size() - 1);
    PK_VERIFY((ends == PkList<int>{2, 3, 4, 1}));
    ends.move(ends.size() - 2, 0);
    PK_VERIFY((ends == PkList<int>{4, 2, 3, 1}));

    // 单元素列表
    PkList<int> one{5};
    one.move(0, 0);
    PK_VERIFY((one == PkList<int>{5}));

    // 两元素对调
    PkList<int> two{1, 2};
    two.move(0, 1);
    PK_VERIFY((two == PkList<int>{2, 1}));
    two.move(1, 0);
    PK_VERIFY((two == PkList<int>{1, 2}));
}

void PkListTest::toVector()
{
    PkList<int> l{1, 2, 3};
    PkVector<int> v = l.toVector();

    PK_COMPARE(v.size(), 3);
    PK_VERIFY((v == PkVector<int>{1, 2, 3}));

    // 是一份独立的容器：改 vector 不影响 list
    v.append(4);
    PK_VERIFY((l == PkList<int>{1, 2, 3}));
    PK_COMPARE(v.size(), 4);

    PkList<int> empty;
    PK_VERIFY(empty.toVector().isEmpty());

    // 往返
    PK_VERIFY((l.toVector().toList() == l));

    // toVector() 是 const 路径：不得 detach
    PkList<int> a{1, 2};
    PkList<int> b(a);
    (void)a.toVector();
    PK_VERIFY(a.PkIsSharedWith(b));
}

void PkListTest::listWritersDetach()
{
    using L = PkList<int>;

    static const PkSeqCowCase<PkList> cases[] = {
        {"removeAt", [](L &s) { s.removeAt(0); }, true},
        {"removeAll(命中)", [](L &s) { (void)s.removeAll(2); }, true},
        {"removeOne(命中)", [](L &s) { (void)s.removeOne(2); }, true},
        // ---- 「删不到东西」的两格：依据是真 Qt 5.15.7 的探针实测 ----
        //
        //   QList removeOne(不命中)  返回=0  元素拷贝=0  isDetached=0   ← 不 detach
        //   QList removeOne(命中)    返回=1  元素拷贝=2  isDetached=1   ← detach
        //   QList removeAll(不命中)  返回=0  元素拷贝=0  isDetached=0   ← 不 detach
        //   QList removeAll(命中)    返回=1  元素拷贝=3  isDetached=1   ← detach
        //
        // **Qt 的两个方法在删不到东西时都不 detach**，所以两格都归 false。
        // 这一格之前是空着的（当时两条路径行为不自洽、没有实测能裁决）；
        // 现在有探针输出了，removeAll 也已按它改成「先 const 确认存在性」。
        {"removeAll(不命中)", [](L &s) { (void)s.removeAll(99); }, false},
        {"removeOne(不命中)", [](L &s) { (void)s.removeOne(99); }, false},
        {"removeFirst", [](L &s) { s.removeFirst(); }, true},
        {"removeLast", [](L &s) { s.removeLast(); }, true},
        {"takeAt", [](L &s) { (void)s.takeAt(1); }, true},
        {"takeFirst", [](L &s) { (void)s.takeFirst(); }, true},
        {"takeLast", [](L &s) { (void)s.takeLast(); }, true},
        {"pop_back", [](L &s) { s.pop_back(); }, true},
        {"pop_front", [](L &s) { s.pop_front(); }, true},
        {"move(from != to)", [](L &s) { s.move(0, 2); }, true},
        // move(i, i) **必须 detach**：实测真 Qt 5.15.7 的 QList::move(i, i)
        // 共享态下元素拷贝 2 次、isDetached 0→1。它不是 no-op。
        {"move(from == to)", [](L &s) { s.move(1, 1); }, true},
    };

    pkSeqRunCowCases<PkList>(cases);
}

void PkListTest::removeMissDoesNotDetach()
{
    // 真 Qt 5.15.7 探针实测（带元素拷贝计数器）——本函数四条断言的全部依据：
    //
    //   QList removeOne(不命中)  返回=0  元素拷贝=0  isDetached=0
    //   QList removeOne(命中)    返回=1  元素拷贝=2  isDetached=1
    //   QList removeAll(不命中)  返回=0  元素拷贝=0  isDetached=0
    //   QList removeAll(命中)    返回=1  元素拷贝=3  isDetached=1
    //
    // 上面 listWritersDetach 的表压的是共享状态；这里补表压不到的两样：
    // **返回值**与**元素拷贝次数**。只看 PkIsSharedWith 证明不了「没走深拷贝
    // 这条路」——实现要是先 PkMut() 再判断，共享状态确实会变，但元素也白拷了。

    // ① 共享态 removeAll(不命中)：返回 0、仍然共享、元素零拷贝
    {
        PkList<PkSeqCounted> a;
        for (int i = 0; i < 3; ++i) {
            a.append(PkSeqCounted(i));
        }
        PkList<PkSeqCounted> b(a);
        PK_VERIFY(a.PkIsSharedWith(b));

        PkSeqCounted::s_copies = 0;
        PK_COMPARE(b.removeAll(PkSeqCounted(99)), 0);
        PK_COMPARE(PkSeqCounted::s_copies, 0);
        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 3);
        PK_COMPARE(b.size(), 3);
    }

    // ② 共享态 removeAll(命中)：返回删除个数、不再共享、另一边一个字节不变
    {
        PkList<int> a{1, 2, 2, 3};
        PkList<int> b(a);
        PK_VERIFY(a.PkIsSharedWith(b));

        PK_COMPARE(b.removeAll(2), 2);
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_VERIFY((a == PkList<int>{1, 2, 2, 3}));
        PK_VERIFY((b == PkList<int>{1, 3}));
    }

    // ③ 共享态 removeOne(不命中)：返回 false、仍然共享、元素零拷贝
    {
        PkList<PkSeqCounted> a;
        for (int i = 0; i < 3; ++i) {
            a.append(PkSeqCounted(i));
        }
        PkList<PkSeqCounted> b(a);
        PK_VERIFY(a.PkIsSharedWith(b));

        PkSeqCounted::s_copies = 0;
        PK_VERIFY(!b.removeOne(PkSeqCounted(99)));
        PK_COMPARE(PkSeqCounted::s_copies, 0);
        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.size(), 3);
    }

    // ④ 共享态 removeOne(命中)：返回 true、不再共享、另一边一个字节不变
    {
        PkList<int> a{1, 2, 2, 3};
        PkList<int> b(a);
        PK_VERIFY(a.PkIsSharedWith(b));

        PK_VERIFY(b.removeOne(2));
        PK_VERIFY(!a.PkIsSharedWith(b));
        PK_VERIFY((a == PkList<int>{1, 2, 2, 3}));
        PK_VERIFY((b == PkList<int>{1, 2, 3}));   // 只删第一个
    }

    // ⑤ 独占态：不命中仍然返回 0/false，内容不变（别把「不 detach」写成「不做事」）
    {
        PkList<int> solo{1, 2, 3};
        PK_COMPARE(solo.removeAll(99), 0);
        PK_VERIFY(!solo.removeOne(99));
        PK_VERIFY((solo == PkList<int>{1, 2, 3}));
        PK_COMPARE(solo.PkUseCount(), 1L);

        // 独占态命中照常删
        PK_COMPARE(solo.removeAll(2), 1);
        PK_VERIFY((solo == PkList<int>{1, 3}));
    }

    // ⑥ 空列表上不命中：不崩、返回 0/false、不 detach
    {
        PkList<int> e;
        PkList<int> f(e);
        PK_COMPARE(f.removeAll(1), 0);
        PK_VERIFY(!f.removeOne(1));
        PK_VERIFY(e.PkIsSharedWith(f));
        PK_VERIFY(f.isEmpty());
    }
}

PK_TEST_MAIN(PkListTest)
