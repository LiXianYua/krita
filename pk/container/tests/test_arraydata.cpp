#include "PkArrayDataTest.h"

#include "../PkArrayData.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

// PkTestBinder<PkArrayDataTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkArrayDataTest> 实例化前对本
// TU 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkArrayDataTest.inc"

// ---------------------------------------------------------------------------
// 堆分配探针
//
// 替换全局 operator new/delete 来数分配次数。这是**程序级**替换（一个 TU 里
// 定义就对整个可执行文件生效），所以测量一律用「窗口前后取差」，不要指望
// 计数器只统计被测代码。
//
// 为什么需要它：`docs/Qt替代品选型.md` §5 点名的唯一性能不确定项就是分配次数
// （QArrayData 把引用计数与数据放同一块分配里 = 1 次 malloc，shared_ptr 默认 2 次）。
// 只数元素拷贝的话，「实现换成三次移动」这种把 O(1) 换成 3 次分配的退化查不出来。
//
// 未替换 over-aligned 版本（operator new(size_t, align_val_t)）：本测试涉及的
// 类型对齐都不超过 max_align_t，走不到那条路径；替换一半会导致 new/delete 配对
// 不上，比漏数更危险。
// ---------------------------------------------------------------------------

static long g_pkAllocCount = 0;   // 常量初始化，先于任何动态初始化，无需担心初始化顺序

void *operator new(std::size_t n)
{
    ++g_pkAllocCount;
    void *p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void *operator new[](std::size_t n)
{
    ++g_pkAllocCount;
    void *p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

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

// 只服务 swapDoesNotAllocate 的探针元素类型。
//
// 关键性质：`PkArrayData<std::vector<PkSwapOnlyProbe>>` 在整个测试进程里
// **从不被移动**，所以它的共享空哨兵始终是「冷」的（从未初始化）。这让
// 「PkSwap 有没有偷偷走移动路径」重新变得可观测：真正的 PkSwap 只换指针、
// 0 次分配；任何退回移动的实现（std::swap、三次移动）都会第一次用到哨兵，
// 触发一次 make_shared —— 计数窗口里就会看到 1。
//
// **不要在别处移动这个类型**（也不要给它加显式实例化）：哨兵一旦被预热，
// 这条断言就退化成恒真。移动改用哨兵之后，单看分配次数已经分不出
// 「1 次指针交换」与「3 次移动」，这是唯一还有判别力的角度。
struct PkSwapOnlyProbe
{
    int v = 0;
};

using SwapProbeData = PkArrayData<std::vector<PkSwapOnlyProbe>>;

CountedVecData makeCounted(int n)
{
    std::vector<Counted> init;
    init.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        init.emplace_back(i);
    }
    return CountedVecData(std::move(init));
}

// 让 PkArrayData<C> 的共享空哨兵在计数窗口之前完成首次初始化。
// 哨兵是函数局部 static，第一次使用时 make_shared 一次；不预热的话
// 「移动零分配」的断言会在第一次跑到时莫名其妙多出 1。
template <typename Data>
void warmUpSentinel()
{
    Data warm;
    Data taken(std::move(warm));
    (void)taken;
}

// stdout 改成行缓冲。默认在管道下是全缓冲，进程崩溃时缓冲区丢失、
// stdout 一片空白，排障时连"崩在哪个测试"都看不到（评审的 M5 变异就是这样）。
// 只影响本可执行文件，不动 pk/test。
const int g_stdoutLineBuffered = []() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    return 0;
}();

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

// PkMut() 是唯一写入口：const 对象上不得存在任何 PkMut() 通路。
// 用 SFINAE 探测「const 对象上能否调 PkMut()」——直接比较返回类型是恒真的
// （const T& 与 T& 本来就永远不同型），查不出「有人加了个 const 重载」。
template <typename T, typename = void>
struct PkHasConstMut : std::false_type {};

template <typename T>
struct PkHasConstMut<T, decltype(void(std::declval<const T &>().PkMut()))> : std::true_type {};

// 探测器自身的正向对照：有 const PkMut() 的类型必须被认出来，
// 否则上面那条断言又变成恒真的。
struct PkConstMutProbe
{
    int &PkMut() const;
};

static_assert(PkHasConstMut<PkConstMutProbe>::value, "探测器失灵：const PkMut() 没被认出来");
static_assert(!PkHasConstMut<IntVecData>::value, "const 对象上不得存在 PkMut() 通路");

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
static_assert(std::is_nothrow_destructible<IntVecData>::value, "析构必须 noexcept");

// ---- 移动必须 noexcept（I1 的编译期机器证据）----
//
// Qt 的 QVector/QList 这两个 trait 都是 true。移动一旦不是 noexcept，
// std::vector<PkVector<T>> 扩容会走 move_if_noexcept 退回拷贝路径。
// 这两条同时也钉住了"别把共享空哨兵改回每次新分配一个空 C"。
static_assert(std::is_nothrow_move_constructible<IntVecData>::value,
              "移动构造必须 noexcept（共享空哨兵，零分配）");
static_assert(std::is_nothrow_move_assignable<IntVecData>::value,
              "移动赋值必须 noexcept（共享空哨兵，零分配）");
static_assert(std::is_nothrow_move_constructible<CountedVecData>::value,
              "非平凡元素类型也必须 noexcept 移动");
static_assert(std::is_nothrow_move_assignable<CountedVecData>::value,
              "非平凡元素类型也必须 noexcept 移动");

// PkSwap 是 noexcept 的零分配交换：Task 2–6 的 swap() 靠它，别退回 std::swap。
static_assert(noexcept(std::declval<IntVecData &>().PkSwap(std::declval<IntVecData &>())),
              "PkSwap() 必须 noexcept");

} // namespace

// ---------------------------------------------------------------------------
// 1. 默认构造 / explicit C 构造
// ---------------------------------------------------------------------------

void PkArrayDataTest::defaultConstructedUseCountIsOne()
{
    IntVecData a;
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkConst().empty());

    // 两个默认构造的实例各自持有一份缓冲区，不是共享同一个空单例
    // （默认构造**不**走哨兵——哨兵只给 moved-from 用）
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

void PkArrayDataTest::explicitInitDoesNotCopyElements()
{
    // 名字承诺的是「接管所有权」，就得真的一个元素都不拷。
    // 把实现从 make_shared<C>(std::move(init)) 改成 make_shared<C>(init)
    // 只有这条断言看得见——只查 size 与元素值的话，拷贝版本一样全绿。
    std::vector<Counted> init;
    init.reserve(4);
    for (int i = 0; i < 4; ++i) {
        init.emplace_back(i);
    }

    Counted::s_copies = 0;
    CountedVecData a(std::move(init));
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(a.PkConst().size(), std::size_t(4));
    PK_COMPARE(a.PkConst()[3].v, 3);
    PK_COMPARE(a.PkUseCount(), 1L);

    // 正向对照：传左值时按值形参会拷一次，证明计数器在这条路径上确实有效
    std::vector<Counted> lvalue;
    lvalue.reserve(2);
    lvalue.emplace_back(7);
    lvalue.emplace_back(8);
    Counted::s_copies = 0;
    CountedVecData b(lvalue);
    PK_COMPARE(Counted::s_copies, 2);
    PK_COMPARE(b.PkConst().size(), std::size_t(2));
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
// 7. 移动：源是「空且完全可用」的容器，走共享空哨兵
//
// 注意：**不要断言 moved-from 的 PkUseCount()**。它拿到的是进程内共享的空哨兵，
// 计数是「1 + 当前活着的 moved-from 个数」，随别的测试/别的作用域浮动。
// 要断言的是真实语义：空、可读、可写、写后能 detach、互不影响。
// ---------------------------------------------------------------------------

void PkArrayDataTest::moveSmokeSourceObserversDoNotCrash()
{
    // 移动组里最小的一条：移动之后源的观测器不得解空指针。
    // 放在本组最前面，是为了让"移动语义回归成隐式 steal"这类事故有个落点——
    // 那种回归会直接段错误，有这条至少能看出崩在移动组的第一步。
    IntVecData a(std::vector<int>{1});
    IntVecData b(std::move(a));

    PK_VERIFY(a.PkUseCount() >= 1L);   // 不崩即达标，具体数值不做断言
    PK_VERIFY(a.PkConst().empty());
    PK_VERIFY(!b.PkConst().empty());
}

void PkArrayDataTest::moveConstructLeavesSourceEmptyAndUsable()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::move(a));

    // 目标拿到全部内容，且独占
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));

    // 源：空容器，四个访问器全都能调（这一条正是哨兵方案要守住的——
    // 隐式移动会让 d 变成 nullptr，下面每一行都会解空指针）
    PK_VERIFY(a.PkConst().empty());
    PK_COMPARE(a.PkConst().size(), std::size_t(0));
    PK_VERIFY(a.PkUseCount() >= 1L);
    a.PkDetach();                     // 从哨兵上分裂出来，不得崩
    PK_COMPARE(a.PkUseCount(), 1L);   // detach 之后才是独占的
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
    PK_VERIFY(a.PkConst().empty());
    PK_VERIFY(a.PkUseCount() >= 1L);
    a.PkDetach();
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.PkMut().empty());
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 不先 detach、直接写也必须安全（PkMut 自己会 detach）
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
    // 源已经脱离这段共享
    PK_VERIFY(!a.PkIsSharedWith(x));
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY(a.PkConst().empty());
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
    PK_VERIFY(p.PkConst().empty());
    PK_VERIFY(!p.PkIsSharedWith(y));
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

    // 自移动必须是 no-op：内容与计数原样（没有把自己的数据搬走再塞个哨兵进来）
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
}

void PkArrayDataTest::movedFromContainersDoNotShareSentinelOnWrite()
{
    // 哨兵是全进程共享的一份空 C。这条测的是它**不会被写污染**：
    // 任何持有哨兵的容器 use_count() 都 ≥ 2（哨兵自己长期持一份），
    // 所以第一次写入必然 detach 出私有副本。
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::vector<int>{4, 5});
    IntVecData aTaken(std::move(a));
    IntVecData bTaken(std::move(b));

    // 两个 moved-from 此刻确实指向同一个哨兵——这是机制本身，不是缺陷
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_VERIFY(a.PkConst().empty());
    PK_VERIFY(b.PkConst().empty());
    PK_VERIFY(a.PkUseCount() >= 2L);   // 哨兵自己那一份 + 至少 a、b

    // 各自写入 → 各自 detach → 互不影响，且都不再是空
    a.PkMut().push_back(11);
    b.PkMut().push_back(22);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY(!a.PkConst().empty());
    PK_VERIFY(!b.PkConst().empty());
    PK_VERIFY((a.PkConst() == std::vector<int>{11}));
    PK_VERIFY((b.PkConst() == std::vector<int>{22}));

    // 关键：哨兵本身没被那两次写入改掉——新的 moved-from 拿到的仍是干净的空
    IntVecData c(std::vector<int>{9});
    IntVecData cTaken(std::move(c));
    PK_VERIFY(c.PkConst().empty());
    PK_COMPARE(c.PkConst().size(), std::size_t(0));

    // 目标全都不受影响
    PK_VERIFY((aTaken.PkConst() == std::vector<int>{1, 2, 3}));
    PK_VERIFY((bTaken.PkConst() == std::vector<int>{4, 5}));
    PK_VERIFY((cTaken.PkConst() == std::vector<int>{9}));
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

// ---------------------------------------------------------------------------
// 8. 堆分配探针
//
// 每条都是「窗口前后取差」，且**先把差值存进局部量再交给 PK_COMPARE**——
// PK_COMPARE 失败路径要拼 std::string，自己会分配，写在同一条语句里会污染读数。
// ---------------------------------------------------------------------------

void PkArrayDataTest::copyDoesNotAllocate()
{
    IntVecData a(std::vector<int>{1, 2, 3});

    const long beforeCtor = g_pkAllocCount;
    IntVecData b(a);
    const long ctorAllocs = g_pkAllocCount - beforeCtor;
    PK_COMPARE(ctorAllocs, 0L);

    IntVecData c;
    const long beforeAssign = g_pkAllocCount;
    c = a;
    const long assignAllocs = g_pkAllocCount - beforeAssign;
    PK_COMPARE(assignAllocs, 0L);

    PK_COMPARE(a.PkUseCount(), 3L);
}

void PkArrayDataTest::moveDoesNotAllocate()
{
    // 哨兵首次使用时会 make_shared 一次；不预热的话这一次会被算进窗口。
    warmUpSentinel<IntVecData>();

    IntVecData a(std::vector<int>{1, 2, 3});
    const long beforeCtor = g_pkAllocCount;
    IntVecData b(std::move(a));
    const long ctorAllocs = g_pkAllocCount - beforeCtor;
    PK_COMPARE(ctorAllocs, 0L);

    IntVecData c(std::vector<int>{7});
    IntVecData d(std::vector<int>{8, 9});
    const long beforeAssign = g_pkAllocCount;
    d = std::move(c);
    const long assignAllocs = g_pkAllocCount - beforeAssign;
    PK_COMPARE(assignAllocs, 0L);

    // 内容仍然对，别让"零分配"是靠什么都没做换来的
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));
    PK_VERIFY((d.PkConst() == std::vector<int>{7}));
    PK_VERIFY(a.PkConst().empty());
    PK_VERIFY(c.PkConst().empty());
}

void PkArrayDataTest::swapDoesNotAllocate()
{
    // 冷哨兵探针：SwapProbeData 在本进程里从不被移动，所以它的共享空哨兵
    // 从未初始化。真正的 PkSwap 只换指针 → 0 次分配；任何退回移动的实现
    // （std::swap、三次移动）都会在这里第一次用到哨兵、触发一次 make_shared。
    // 这是移动改用哨兵之后，唯一还能把两者区分开的角度——单看「有没有多分配」
    // 已经分不出来了，因为移动本身也不分配了。
    //
    // 本函数**必须**是全进程第一个也是唯一一个碰 SwapProbeData 的地方。
    SwapProbeData p(std::vector<PkSwapOnlyProbe>{PkSwapOnlyProbe{1}});
    SwapProbeData q(std::vector<PkSwapOnlyProbe>{PkSwapOnlyProbe{2}});

    const long beforeCold = g_pkAllocCount;
    p.PkSwap(q);
    const long coldAllocs = g_pkAllocCount - beforeCold;
    PK_COMPARE(coldAllocs, 0L);
    PK_COMPARE(p.PkConst()[0].v, 2);
    PK_COMPARE(q.PkConst()[0].v, 1);

    // 常规路径（哨兵早已预热）也断言 0 次
    IntVecData a(std::vector<int>{1, 2});
    IntVecData b(std::vector<int>{3});

    const long before = g_pkAllocCount;
    a.PkSwap(b);
    const long allocs = g_pkAllocCount - before;
    PK_COMPARE(allocs, 0L);

    PK_VERIFY((a.PkConst() == std::vector<int>{3}));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2}));
}

void PkArrayDataTest::detachAllocationCounts()
{
    // 独占：0 次
    IntVecData a(std::vector<int>{1, 2, 3});
    const long beforeExclusive = g_pkAllocCount;
    a.PkDetach();
    const long exclusiveAllocs = g_pkAllocCount - beforeExclusive;
    PK_COMPARE(exclusiveAllocs, 0L);

    // 共享 + 非空：**2 次**（正向对照，防计数器本身失灵）。
    //
    // 这 2 次是 docs/Qt替代品选型.md §5 点名的那笔账：
    //   1) make_shared<C> —— 控制块与 C 对象**融在同一块**分配里（这一半 Qt 也要）
    //   2) std::vector 的拷贝构造为元素缓冲区单独分配
    // Qt 的 QArrayData 把引用计数、头部与元素数据放进**同一块** malloc，
    // 所以同一次 detach 在 Qt 那边是 1 次。这是替代品相对 Qt 的已知差距，
    // 数值写死在这里，将来若把内层换成自定义的融合分配器，这条会立刻失败提醒改判。
    IntVecData shared(std::vector<int>{1, 2, 3});
    IntVecData sharer(shared);
    const long beforeShared = g_pkAllocCount;
    sharer.PkDetach();
    const long sharedAllocs = g_pkAllocCount - beforeShared;
    PK_COMPARE(sharedAllocs, 2L);
    PK_VERIFY(!sharer.PkIsSharedWith(shared));

    // 共享 + 空容器：1 次（只有 make_shared，空 vector 不分配元素缓冲）
    IntVecData empty;
    IntVecData emptySharer(empty);
    const long beforeEmpty = g_pkAllocCount;
    emptySharer.PkDetach();
    const long emptyAllocs = g_pkAllocCount - beforeEmpty;
    PK_COMPARE(emptyAllocs, 1L);

    // 默认构造：1 次（make_shared）
    const long beforeDefault = g_pkAllocCount;
    {
        IntVecData fresh;
        (void)fresh;
    }
    const long defaultAllocs = g_pkAllocCount - beforeDefault;
    PK_COMPARE(defaultAllocs, 1L);
}

PK_TEST_MAIN(PkArrayDataTest)
