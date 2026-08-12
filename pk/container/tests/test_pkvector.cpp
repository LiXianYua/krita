#include "PkVectorTest.h"

#include "../PkVector.h"

#include "PkSeqTestShared.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

// PkTestBinder<PkVectorTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkVectorTest> 实例化前对本 TU
// 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkVectorTest.inc"

namespace {

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntVec = PkVector<int>;

// ---- 契约的编译期部分（签名形状，不是行为）----

// size()/count() 返回 int 不是 size_t：Qt5 口径，v.size() - 1 在空容器上必须
// 是 -1 而不是回绕成天文数字。
static_assert(std::is_same<decltype(std::declval<const IntVec &>().size()), int>::value,
              "size() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntVec &>().count()), int>::value,
              "count() 必须返回 int");
static_assert(std::is_same<decltype(std::declval<const IntVec &>().capacity()), int>::value,
              "capacity() 必须返回 int");

// 读路径返回 const 引用/指针，写路径返回可写引用/指针。
static_assert(std::is_same<decltype(std::declval<const IntVec &>().at(0)), const int &>::value,
              "at() 必须返回 const T&");
static_assert(std::is_same<decltype(std::declval<IntVec &>()[0]), int &>::value,
              "非 const operator[] 必须返回 T&");
static_assert(std::is_same<decltype(std::declval<const IntVec &>()[0]), const int &>::value,
              "const operator[] 必须返回 const T&");
static_assert(std::is_same<decltype(std::declval<const IntVec &>().constData()), const int *>::value,
              "constData() 必须返回 const T*");
static_assert(std::is_same<decltype(std::declval<IntVec &>().data()), int *>::value,
              "非 const data() 必须返回 T*");

// value() 按值返回（越界时要能返回 T()，返回引用就没有可指的对象）。
static_assert(std::is_same<decltype(std::declval<const IntVec &>().value(0)), int>::value,
              "value() 必须按值返回");

// 拷贝必须存在且 O(1)——2286 处 Q_FOREACH 的命根子；声明了移动构造会把隐式
// 拷贝 deleted 掉，这条断言把那个陷阱钉在编译期。
static_assert(std::is_copy_constructible<IntVec>::value, "拷贝构造必须存在");
static_assert(std::is_copy_assignable<IntVec>::value, "拷贝赋值必须存在");
static_assert(std::is_move_constructible<IntVec>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntVec>::value, "移动赋值必须存在");
static_assert(noexcept(std::declval<IntVec &>().swap(std::declval<IntVec &>())),
              "swap() 必须 noexcept");

// PkVector(int) 必须是 explicit：PkVector<int> v = 5; 不该编过。
static_assert(!std::is_convertible<int, IntVec>::value, "PkVector(int) 必须是 explicit");
static_assert(std::is_constructible<IntVec, int>::value, "PkVector(int) 必须能显式构造");

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：逐条在 PkVector 上实例化 tests/PkSeqTestShared.h 里的用例
// ---------------------------------------------------------------------------

void PkVectorTest::sizeIsInt() { pkSeqTestSizeIsInt<PkVector>(); }
void PkVectorTest::sizeAndEmptiness() { pkSeqTestSizeAndEmptiness<PkVector>(); }
void PkVectorTest::elementAccess() { pkSeqTestElementAccess<PkVector>(); }
void PkVectorTest::valueOutOfRange() { pkSeqTestValueOutOfRange<PkVector>(); }
void PkVectorTest::appendAndPrepend() { pkSeqTestAppendAndPrepend<PkVector>(); }
void PkVectorTest::insertAndRemove() { pkSeqTestInsertAndRemove<PkVector>(); }
void PkVectorTest::erase() { pkSeqTestErase<PkVector>(); }
void PkVectorTest::search() { pkSeqTestSearch<PkVector>(); }
void PkVectorTest::iterators() { pkSeqTestIterators<PkVector>(); }
void PkVectorTest::constIteratorsDoNotDetach() { pkSeqTestConstIteratorsDoNotDetach<PkVector>(); }
void PkVectorTest::comparison() { pkSeqTestComparison<PkVector>(); }
void PkVectorTest::streamOperators() { pkSeqTestStreamOperators<PkVector>(); }
void PkVectorTest::cowIsolation() { pkSeqTestCowIsolation<PkVector>(); }
void PkVectorTest::copyIsConstantTime() { pkSeqTestCopyIsConstantTime<PkVector>(); }
void PkVectorTest::constNeverDetaches() { pkSeqTestConstNeverDetaches<PkVector>(); }
void PkVectorTest::everyWriterDetaches() { pkSeqTestEveryWriterDetaches<PkVector>(); }
void PkVectorTest::reserveDetachRules() { pkSeqTestReserveDetachRules<PkVector>(); }
void PkVectorTest::swap() { pkSeqTestSwap<PkVector>(); }
void PkVectorTest::selfAssignment() { pkSeqTestSelfAssignment<PkVector>(); }
void PkVectorTest::moveLeavesSourceUsable() { pkSeqTestMoveLeavesSourceUsable<PkVector>(); }
void PkVectorTest::initializerListAndDefaults() { pkSeqTestInitializerListAndDefaults<PkVector>(); }

// ---------------------------------------------------------------------------
// PkVector 专有
// ---------------------------------------------------------------------------

void PkVectorTest::sizedConstructors()
{
    // QVector<T>(int size)：size 个默认构造的元素
    PkVector<int> v(3);
    PK_COMPARE(v.size(), 3);
    PK_COMPARE(v.at(0), 0);
    PK_COMPARE(v.at(2), 0);
    PK_COMPARE(v.PkUseCount(), 1L);

    PkVector<int> zero(0);
    PK_COMPARE(zero.size(), 0);
    PK_VERIFY(zero.isEmpty());

    // QVector<T>(int size, const T &t)
    PkVector<int> filled(4, 7);
    PK_COMPARE(filled.size(), 4);
    PK_VERIFY((filled == PkVector<int>{7, 7, 7, 7}));

    PkVector<std::string> strings(2, std::string("x"));
    PK_COMPARE(strings.size(), 2);
    PK_VERIFY(strings.at(1) == "x");
}

// remove(int) / remove(int, int) 只有 QVector 有 —— QList 没有（Qt5 里
// `QList::remove(...)` 根本编不过，所以调用点里不可能存在）。上面的
// PkListHasNoRemoveInt 断言把"PkList 不许有它"钉在编译期，这里压 PkVector 的语义。
void PkVectorTest::removeByIndex()
{
    PkVector<int> v{0, 1, 9, 2, 3, 4};

    v.remove(2);
    PK_VERIFY((v == PkVector<int>{0, 1, 2, 3, 4}));

    v.remove(1, 2);
    PK_VERIFY((v == PkVector<int>{0, 3, 4}));

    v.remove(0, 0);   // n == 0 是 no-op
    PK_VERIFY((v == PkVector<int>{0, 3, 4}));

    // 删到只剩空
    v.remove(0, v.size());
    PK_VERIFY(v.isEmpty());
    PK_COMPARE(v.size(), 0);

    // 删空之后照常可用
    v.append(7);
    PK_VERIFY((v == PkVector<int>{7}));

    // 末尾一个
    PkVector<int> t{1, 2, 3};
    t.remove(t.size() - 1);
    PK_VERIFY((t == PkVector<int>{1, 2}));
}

void PkVectorTest::resize()
{
    PkVector<int> v{1, 2, 3};

    // 缩小：尾部被截掉
    v.resize(2);
    PK_VERIFY((v == PkVector<int>{1, 2}));

    // 放大：新元素是 T()
    v.resize(4);
    PK_COMPARE(v.size(), 4);
    PK_VERIFY((v == PkVector<int>{1, 2, 0, 0}));

    v.resize(0);
    PK_VERIFY(v.isEmpty());
    PK_COMPARE(v.size(), 0);

    // resize 之后照常可用
    v.append(9);
    PK_VERIFY((v == PkVector<int>{9}));
}

void PkVectorTest::fill()
{
    PkVector<int> v{1, 2, 3};

    // fill(t)：size 不变，全部改写成 t，返回自身引用
    PkVector<int> &ref = v.fill(7);
    PK_VERIFY((v == PkVector<int>{7, 7, 7}));
    PK_VERIFY(&ref == &v);

    // fill(t, size)：先 resize 再填
    v.fill(5, 5);
    PK_COMPARE(v.size(), 5);
    PK_VERIFY((v == PkVector<int>{5, 5, 5, 5, 5}));

    v.fill(1, 2);
    PK_VERIFY((v == PkVector<int>{1, 1}));

    v.fill(3, 0);
    PK_VERIFY(v.isEmpty());

    // 空容器上 fill(t) 不做任何事
    PkVector<int> empty;
    empty.fill(9);
    PK_VERIFY(empty.isEmpty());

    // 用自身的元素填：Qt 先取一份副本，我们照做，否则边填边悬垂
    PkVector<int> alias{4, 1, 1};
    alias.fill(alias.at(0));
    PK_VERIFY((alias == PkVector<int>{4, 4, 4}));
}

void PkVectorTest::capacity()
{
    PkVector<int> v;
    v.reserve(64);
    PK_VERIFY(v.capacity() >= 64);
    PK_COMPARE(v.size(), 0);

    v.append(1);
    PK_VERIFY(v.capacity() >= 1);
    PK_COMPARE(v.size(), 1);

    // capacity() 是 const 路径：不得 detach
    PkVector<int> a{1, 2};
    PkVector<int> b(a);
    (void)a.capacity();
    PK_VERIFY(a.PkIsSharedWith(b));
}

void PkVectorTest::toList()
{
    PkVector<int> v{1, 2, 3};
    PkList<int> l = v.toList();

    PK_COMPARE(l.size(), 3);
    PK_VERIFY((l == PkList<int>{1, 2, 3}));

    // 是一份独立的容器：改 list 不影响 vector
    l.append(4);
    PK_VERIFY((v == PkVector<int>{1, 2, 3}));
    PK_COMPARE(l.size(), 4);

    // 空容器
    PkVector<int> empty;
    PK_VERIFY(empty.toList().isEmpty());

    // 往返：toList().toVector() 还原
    PK_VERIFY((v.toList().toVector() == v));

    // toList() 是 const 路径：不得 detach
    PkVector<int> a{1, 2};
    PkVector<int> b(a);
    (void)a.toList();
    PK_VERIFY(a.PkIsSharedWith(b));
}

void PkVectorTest::vectorWritersDetach()
{
    using V = PkVector<int>;

    static const PkSeqCowCase<PkVector> cases[] = {
        {"resize(bigger)", [](V &s) { s.resize(5); }, true},
        {"resize(smaller)", [](V &s) { s.resize(1); }, true},
        {"resize(same)", [](V &s) { s.resize(3); }, true},
        {"fill(t)", [](V &s) { s.fill(9); }, true},
        {"fill(t, size)", [](V &s) { s.fill(9, 2); }, true},
        {"remove(int)", [](V &s) { s.remove(0); }, true},
        {"remove(int, int)", [](V &s) { s.remove(0, 2); }, true},
        // 反面：capacity() 是 const 方法，绝不能 detach
        {"capacity() const", [](V &s) { (void)s.capacity(); }, false},
    };

    pkSeqRunCowCases<PkVector>(cases);
}

PK_TEST_MAIN(PkVectorTest)
