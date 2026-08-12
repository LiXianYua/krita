#include "PkArrayDataTest.h"

#include "../PkArrayData.h"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

// PkTestBinder<PkArrayDataTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkArrayDataTest> 实例化前对本
// TU 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkArrayDataTest.inc"

namespace {

// 带拷贝计数器的元素类型：用来证明 "use_count()==1 时 detach 零成本" 这条
// 不是靠断言 use_count 自我印证的，而是真的一个元素都没拷。
// 移动显式 default 并标 noexcept —— 否则 vector 扩容会退化成逐元素拷贝，
// 计数器就分不清 "detach 拷的" 与 "扩容拷的"。
struct Counted
{
    int v = 0;

    Counted() = default;
    explicit Counted(int x) : v(x) {}
    Counted(const Counted &o) : v(o.v) { ++s_copies; }
    Counted &operator=(const Counted &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    Counted(Counted &&) noexcept = default;
    Counted &operator=(Counted &&) noexcept = default;
    ~Counted() = default;

    bool operator==(const Counted &o) const { return v == o.v; }

    static int s_copies;
};

int Counted::s_copies = 0;

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntVecData = PkArrayData<std::vector<int>>;
using CountedVecData = PkArrayData<std::vector<Counted>>;

CountedVecData makeCounted(int n)
{
    std::vector<Counted> init;
    init.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        init.emplace_back(i);
    }
    return CountedVecData(std::move(init));
}

// ---- 契约的编译期部分（签名形状，不是行为）----

// explicit PkArrayData(C init)：不得存在 C → PkArrayData<C> 的隐式转换。
static_assert(!std::is_convertible<std::vector<int>, IntVecData>::value,
              "PkArrayData(C) 必须是 explicit");
static_assert(std::is_constructible<IntVecData, std::vector<int>>::value,
              "PkArrayData(C) 必须能显式构造");

// 读路径与观测器都是 noexcept；写路径不是（要分配）。
static_assert(noexcept(std::declval<const IntVecData &>().PkConst()),
              "PkConst() 必须 noexcept");
static_assert(noexcept(std::declval<const IntVecData &>().PkUseCount()),
              "PkUseCount() 必须 noexcept");
static_assert(noexcept(std::declval<const IntVecData &>().PkIsSharedWith(
                  std::declval<const IntVecData &>())),
              "PkIsSharedWith() 必须 noexcept");

// PkConst() 返回 const 引用、PkMut() 返回可写引用、PkUseCount() 返回 long。
static_assert(std::is_same<decltype(std::declval<const IntVecData &>().PkConst()),
                           const std::vector<int> &>::value,
              "PkConst() 必须返回 const C&");
static_assert(std::is_same<decltype(std::declval<IntVecData &>().PkMut()),
                           std::vector<int> &>::value,
              "PkMut() 必须返回 C&");
static_assert(std::is_same<decltype(std::declval<const IntVecData &>().PkUseCount()),
                           long>::value,
              "PkUseCount() 必须返回 long");

// PkMut() 是唯一写入口：const 对象上不得有可写通路。
static_assert(!std::is_same<decltype(std::declval<const IntVecData &>().PkConst()),
                            std::vector<int> &>::value,
              "const 对象不得拿到可写引用");

// ---- 五个特殊成员都还在（这组断言守的是一个真陷阱）----
//
// 用户一旦声明移动构造，隐式的**拷贝**构造与拷贝赋值就会被定义为 deleted
// （[class.copy.ctor]/8）。而「拷贝 O(1)」是 2286 处 Q_FOREACH 的命根子。
// 漏写 `= default` 的报错会出现在遥远的调用点上（"use of deleted function"），
// 在这里钉死，让它在地基自己的编译期就现形。
static_assert(std::is_copy_constructible<IntVecData>::value,
              "拷贝构造必须存在——声明移动构造会把它 deleted 掉");
static_assert(std::is_copy_assignable<IntVecData>::value,
              "拷贝赋值必须存在——同上");
static_assert(std::is_nothrow_copy_constructible<IntVecData>::value,
              "拷贝构造必须 noexcept（只拷 shared_ptr）");
static_assert(std::is_nothrow_copy_assignable<IntVecData>::value,
              "拷贝赋值必须 noexcept（只拷 shared_ptr）");
static_assert(std::is_move_constructible<IntVecData>::value, "移动构造必须存在");
static_assert(std::is_move_assignable<IntVecData>::value, "移动赋值必须存在");
static_assert(std::is_nothrow_destructible<IntVecData>::value, "析构必须 noexcept");

// PkSwap 是 noexcept 的零分配交换：Task 2–6 的 swap() 靠它，别退回 std::swap。
static_assert(noexcept(std::declval<IntVecData &>().PkSwap(std::declval<IntVecData &>())),
              "PkSwap() 必须 noexcept");

} // namespace

// ---------------------------------------------------------------------------
// 1. 默认构造
// ---------------------------------------------------------------------------

void PkArrayDataTest::defaultConstructedUseCountIsOne()
{
    IntVecData a;
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkConst().empty());

    // 两个默认构造的实例各自持有一份缓冲区，不是共享同一个空单例
    IntVecData b;
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY(a.PkIsSharedWith(a));
}

void PkArrayDataTest::explicitInitTakesOwnership()
{
    std::vector<int> init{4, 5, 6};
    IntVecData a(std::move(init));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.PkConst().size(), std::size_t(3));
    PK_COMPARE(a.PkConst()[0], 4);
    PK_COMPARE(a.PkConst()[2], 6);
}

// ---------------------------------------------------------------------------
// 2. 拷贝 = 共享
// ---------------------------------------------------------------------------

void PkArrayDataTest::copyConstructShares()
{
    IntVecData a(std::vector<int>{7, 8});
    PK_COMPARE(a.PkUseCount(), 1L);

    IntVecData b(a);
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_VERIFY(b.PkIsSharedWith(a));
    PK_VERIFY((b.PkConst() == std::vector<int>{7, 8}));

    {
        IntVecData c(b);
        PK_COMPARE(a.PkUseCount(), 3L);
        PK_VERIFY(c.PkIsSharedWith(a));
    }
    // c 析构后计数回落
    PK_COMPARE(a.PkUseCount(), 2L);
}

void PkArrayDataTest::copyAssignShares()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::vector<int>{9});
    PK_VERIFY(!a.PkIsSharedWith(b));

    b = a;
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));
}

void PkArrayDataTest::copyDoesNotCopyElements()
{
    // 硬要求 3：拷贝构造/赋值必须 O(1)。2286 处 Q_FOREACH 全指望这一条。
    CountedVecData a = makeCounted(5);
    Counted::s_copies = 0;

    CountedVecData b(a);
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 2L);

    CountedVecData c;
    c = a;
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 3L);
}

// ---------------------------------------------------------------------------
// 3. COW 核心：写一边，另一边内容不变
// ---------------------------------------------------------------------------

void PkArrayDataTest::mutDetachesAndLeavesOtherIntact()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    b.PkMut().push_back(4);

    // 分裂了
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY(!b.PkIsSharedWith(a));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(b.PkUseCount(), 1L);

    // 另一边内容一个字节都没动
    PK_COMPARE(a.PkConst().size(), std::size_t(3));
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3}));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3, 4}));

    // 反方向同样成立：现在两边已经各自独占，改 a 不影响 b
    a.PkMut()[0] = 99;
    PK_COMPARE(a.PkConst()[0], 99);
    PK_COMPARE(b.PkConst()[0], 1);

    // 再共享一次，改 a 这一侧
    IntVecData c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    a.PkMut()[1] = 77;
    PK_VERIFY(!a.PkIsSharedWith(c));
    PK_COMPARE(c.PkConst()[1], 2);
    PK_COMPARE(a.PkConst()[1], 77);
}

void PkArrayDataTest::detachDropsShareOnlyForCaller()
{
    IntVecData a(std::vector<int>{1});
    IntVecData b(a);
    IntVecData c(a);
    PK_COMPARE(a.PkUseCount(), 3L);

    b.PkDetach();

    PK_VERIFY(!b.PkIsSharedWith(a));
    PK_VERIFY(!b.PkIsSharedWith(c));
    // 剩下两个仍然共享同一份
    PK_VERIFY(a.PkIsSharedWith(c));
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_COMPARE(c.PkUseCount(), 2L);
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1}));
}

// ---------------------------------------------------------------------------
// 4. 独占时零拷贝（带正向对照，防止计数器本身失灵让断言空过）
// ---------------------------------------------------------------------------

void PkArrayDataTest::detachOnUnsharedDoesNotCopy()
{
    CountedVecData a = makeCounted(3);
    PK_COMPARE(a.PkUseCount(), 1L);

    Counted::s_copies = 0;
    a.PkDetach();
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 1L);

    // PkMut() 走的是同一条 PkDetach，也必须零成本
    (void)a.PkMut();
    PK_COMPARE(Counted::s_copies, 0);

    // 连续多次同样零成本
    for (int i = 0; i < 5; ++i) {
        a.PkDetach();
        (void)a.PkMut();
    }
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(a.PkConst().size(), std::size_t(3));
}

void PkArrayDataTest::detachOnSharedDoesCopy()
{
    // 上一条的正向对照：计数器确实数得到拷贝，共享时 detach 真的深拷了。
    CountedVecData a = makeCounted(3);
    CountedVecData b(a);
    PK_COMPARE(a.PkUseCount(), 2L);

    Counted::s_copies = 0;
    b.PkDetach();
    PK_COMPARE(Counted::s_copies, 3);
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 分裂之后再 detach 就不拷了
    Counted::s_copies = 0;
    b.PkDetach();
    PK_COMPARE(Counted::s_copies, 0);
}

// ---------------------------------------------------------------------------
// 5. PkConst() 绝不 detach（167 处 constBegin + 190 处 constEnd 靠这条保持 O(1)）
// ---------------------------------------------------------------------------

void PkArrayDataTest::constNeverDetaches()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(a);
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);

    const IntVecData &ca = a;
    const std::vector<int> &r = ca.PkConst();
    PK_COMPARE(r.size(), std::size_t(3));

    // 调用前后共享状态原样保持
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);

    for (int i = 0; i < 10; ++i) {
        (void)a.PkConst();
        (void)b.PkConst();
        (void)ca.PkConst().size();
    }
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);

    // 引用在整个过程里仍指向同一块缓冲区（没有被悄悄换掉）
    PK_VERIFY(&r == &a.PkConst());

    // 独占时读也不该改变任何东西
    CountedVecData c = makeCounted(4);
    Counted::s_copies = 0;
    for (int i = 0; i < 10; ++i) {
        (void)c.PkConst().size();
    }
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(c.PkUseCount(), 1L);
}

// ---------------------------------------------------------------------------
// 6. 自赋值
// ---------------------------------------------------------------------------

void PkArrayDataTest::selfAssignmentIsSafe()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    // 经引用绕一道：直接写 a = a 会被 -Wself-assign-overloaded 拦下，
    // 而真实调用点里的自赋值本来就是通过别名/引用发生的。
    IntVecData &alias = a;
    a = alias;

    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3}));

    // 自赋值之后照常可写、可共享
    a.PkMut().push_back(4);
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3, 4}));
    PK_COMPARE(a.PkUseCount(), 1L);
}

void PkArrayDataTest::selfAssignmentWhileSharedIsSafe()
{
    IntVecData a(std::vector<int>{5, 6});
    IntVecData b(a);
    PK_COMPARE(a.PkUseCount(), 2L);

    IntVecData &alias = a;
    a = alias;

    // 共享关系与计数都不该被自赋值破坏
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_VERIFY((a.PkConst() == std::vector<int>{5, 6}));
    PK_VERIFY((b.PkConst() == std::vector<int>{5, 6}));

    // 自赋值之后 COW 仍然生效
    a.PkMut().push_back(7);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY((b.PkConst() == std::vector<int>{5, 6}));
}

// ---------------------------------------------------------------------------
// 7. 移动：源变成「空且完全可用」的容器（Qt 语义），不是空 shared_ptr
// ---------------------------------------------------------------------------

void PkArrayDataTest::moveConstructLeavesSourceEmptyAndUsable()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::move(a));

    // 目标拿到全部内容，且独占
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));

    // 源：空容器 + 独占，四个访问器全都能调（这一条正是本次修改的目标——
    // 隐式移动会让 d 变成 nullptr，下面每一行都会解空指针）
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkConst().empty());
    PK_COMPARE(a.PkConst().size(), std::size_t(0));
    a.PkDetach();                     // 独占时零成本，且不得崩
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkMut().empty());     // 可写通路也是活的
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 源作为一个正常的空容器继续用：能写、能共享、COW 照常
    a.PkMut().push_back(42);
    PK_VERIFY((a.PkConst() == std::vector<int>{42}));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));
    IntVecData c(a);
    PK_COMPARE(a.PkUseCount(), 2L);
    a.PkMut().push_back(43);
    PK_VERIFY(!a.PkIsSharedWith(c));
    PK_VERIFY((c.PkConst() == std::vector<int>{42}));
}

void PkArrayDataTest::moveAssignLeavesSourceEmptyAndUsable()
{
    IntVecData a(std::vector<int>{1, 2});
    IntVecData b(std::vector<int>{9, 9, 9});

    b = std::move(a);

    // 目标拿到源的内容，旧内容被释放
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2}));

    // 源：空且完全可用，与移动构造同一套保证
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkConst().empty());
    a.PkDetach();
    PK_VERIFY(a.PkMut().empty());
    PK_VERIFY(!a.PkIsSharedWith(b));

    a.PkMut().push_back(7);
    PK_VERIFY((a.PkConst() == std::vector<int>{7}));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2}));

    // 源也可以被整体重新赋值，行为与普通实例无异
    a = IntVecData(std::vector<int>{3});
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY((a.PkConst() == std::vector<int>{3}));
}

void PkArrayDataTest::moveIsConstantTime()
{
    // 移动构造：零元素拷贝
    CountedVecData a = makeCounted(6);
    Counted::s_copies = 0;
    CountedVecData b(std::move(a));
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_COMPARE(b.PkConst().size(), std::size_t(6));
    PK_VERIFY(a.PkConst().empty());

    // 移动赋值：同样零元素拷贝（源、目标都非空，两侧的元素都不该被拷）
    CountedVecData c = makeCounted(4);
    CountedVecData d = makeCounted(5);
    Counted::s_copies = 0;
    d = std::move(c);
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(d.PkConst().size(), std::size_t(4));
    PK_VERIFY(c.PkConst().empty());

    // PkSwap：零分配也零拷贝
    CountedVecData e = makeCounted(3);
    CountedVecData f = makeCounted(9);
    Counted::s_copies = 0;
    e.PkSwap(f);
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(e.PkConst().size(), std::size_t(9));
    PK_COMPARE(f.PkConst().size(), std::size_t(3));
}

void PkArrayDataTest::moveCarriesSharingToTarget()
{
    // 源原本与第三方 x 共享 —— 移动之后这份共享关系必须跟着目标走，
    // 而不是留在被掏空的源身上，也不是凭空断掉。
    IntVecData x(std::vector<int>{8});
    IntVecData a(x);
    PK_COMPARE(x.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(x));

    IntVecData b(std::move(a));

    PK_VERIFY(b.PkIsSharedWith(x));
    PK_VERIFY(x.PkIsSharedWith(b));
    PK_COMPARE(x.PkUseCount(), 2L);
    PK_COMPARE(b.PkUseCount(), 2L);
    // 源已经脱离这段共享，自己独占一个空的
    PK_VERIFY(!a.PkIsSharedWith(x));
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 1L);
    // 第三方内容不受影响
    PK_VERIFY((x.PkConst() == std::vector<int>{8}));

    // 移动赋值路径同样：共享跟着目标走
    IntVecData y(std::vector<int>{5});
    IntVecData p(y);
    PK_COMPARE(y.PkUseCount(), 2L);
    IntVecData q;
    q = std::move(p);
    PK_COMPARE(q.PkUseCount(), 2L);
    PK_VERIFY(q.PkIsSharedWith(y));
    PK_COMPARE(p.PkUseCount(), 1L);
    PK_VERIFY(p.PkConst().empty());
    PK_VERIFY((y.PkConst() == std::vector<int>{5}));

    // 共享着的目标被写 → 照常 detach，第三方不受污染
    q.PkMut().push_back(6);
    PK_VERIFY(!q.PkIsSharedWith(y));
    PK_VERIFY((y.PkConst() == std::vector<int>{5}));
}

void PkArrayDataTest::selfMoveAssignmentIsSafe()
{
    // 经引用绕一道：直接写 a = std::move(a) 会被 -Wself-move 拦下。
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData &alias = a;
    a = std::move(alias);

    // 自移动必须是 no-op：内容与计数原样（没有把自己的数据搬走再塞个空的进来）
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3}));

    // 自移动之后照常可写
    a.PkMut().push_back(4);
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3, 4}));

    // 共享状态下的自移动：共享关系与内容都不该被破坏
    IntVecData b(std::vector<int>{5, 6});
    IntVecData c(b);
    IntVecData &bAlias = b;
    b = std::move(bAlias);
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(b.PkIsSharedWith(c));
    PK_VERIFY((b.PkConst() == std::vector<int>{5, 6}));
    PK_VERIFY((c.PkConst() == std::vector<int>{5, 6}));
}

void PkArrayDataTest::movedFromSourceIsIndependentOfTarget()
{
    // 源与目标绝不能共享同一块缓冲区——否则「源是空容器」会变成
    // 「源和目标是同一个容器」，往源里写就污染了目标。
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::move(a));
    PK_VERIFY(!a.PkIsSharedWith(b));

    a.PkMut().push_back(99);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));
    b.PkMut().push_back(4);
    PK_VERIFY((a.PkConst() == std::vector<int>{99}));

    // 两个各自被移动走的源之间也必须互相独立（不是共用某个全局空哨兵）
    IntVecData p(std::vector<int>{1});
    IntVecData q(std::vector<int>{2});
    IntVecData pMoved(std::move(p));
    IntVecData qMoved(std::move(q));
    PK_VERIFY(!p.PkIsSharedWith(q));
    PK_COMPARE(p.PkUseCount(), 1L);
    PK_COMPARE(q.PkUseCount(), 1L);
    p.PkMut().push_back(7);
    PK_VERIFY(q.PkConst().empty());
}

void PkArrayDataTest::swapExchangesBuffers()
{
    IntVecData a(std::vector<int>{1, 2});
    IntVecData b(std::vector<int>{3});
    IntVecData aShare(a);   // a 的共享伙伴，交换后应当跟着缓冲区走到 b 那边

    a.PkSwap(b);

    PK_VERIFY((a.PkConst() == std::vector<int>{3}));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2}));
    PK_VERIFY(b.PkIsSharedWith(aShare));
    PK_VERIFY(!a.PkIsSharedWith(aShare));
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_COMPARE(a.PkUseCount(), 1L);

    // 自交换安全
    IntVecData &aAlias = a;
    a.PkSwap(aAlias);
    PK_VERIFY((a.PkConst() == std::vector<int>{3}));
    PK_COMPARE(a.PkUseCount(), 1L);
}

PK_TEST_MAIN(PkArrayDataTest)
