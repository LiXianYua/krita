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

    pkSeqCheckDetaches<PkList>("removeAt", [](L &s) { s.removeAt(0); });
    pkSeqCheckDetaches<PkList>("removeAll", [](L &s) { s.removeAll(2); });
    pkSeqCheckDetaches<PkList>("removeAll(不命中)", [](L &s) { s.removeAll(99); });
    pkSeqCheckDetaches<PkList>("removeOne", [](L &s) { s.removeOne(2); });
    pkSeqCheckDetaches<PkList>("removeFirst", [](L &s) { s.removeFirst(); });
    pkSeqCheckDetaches<PkList>("removeLast", [](L &s) { s.removeLast(); });
    pkSeqCheckDetaches<PkList>("takeAt", [](L &s) { (void)s.takeAt(1); });
    pkSeqCheckDetaches<PkList>("takeFirst", [](L &s) { (void)s.takeFirst(); });
    pkSeqCheckDetaches<PkList>("takeLast", [](L &s) { (void)s.takeLast(); });
    pkSeqCheckDetaches<PkList>("pop_back", [](L &s) { s.pop_back(); });
    pkSeqCheckDetaches<PkList>("pop_front", [](L &s) { s.pop_front(); });
    pkSeqCheckDetaches<PkList>("move", [](L &s) { s.move(0, 2); });
    // from == to 也要 detach：非 const 方法一律经 PkMut()，不留例外
    pkSeqCheckDetaches<PkList>("move(from==to)", [](L &s) { s.move(1, 1); });
}

PK_TEST_MAIN(PkListTest)
