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
// 7. 移动之后源对象仍可析构、可再赋值
// ---------------------------------------------------------------------------

void PkArrayDataTest::moveConstructLeavesSourceUsable()
{
    IntVecData a(std::vector<int>{1, 2, 3});
    IntVecData b(std::move(a));

    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));

    // 被移动走的 a：契约只承诺"可析构的有效状态"。PkUseCount()/PkIsSharedWith()
    // 是 shared_ptr 上的良定义查询，对空指针也能安全调用；PkConst()/PkMut()
    // 不在承诺之内（见 PkArrayData.h 顶部关于移动后状态的说明）。
    PK_COMPARE(a.PkUseCount(), 0L);
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 可以再赋值，赋完就完全正常
    a = b;
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_VERIFY((a.PkConst() == std::vector<int>{1, 2, 3}));
    a.PkMut().push_back(4);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2, 3}));

    // 移动是 O(1)：一个元素都不拷
    CountedVecData c = makeCounted(6);
    Counted::s_copies = 0;
    CountedVecData d(std::move(c));
    PK_COMPARE(Counted::s_copies, 0);
    PK_COMPARE(d.PkUseCount(), 1L);
}

void PkArrayDataTest::moveAssignLeavesSourceUsable()
{
    IntVecData a(std::vector<int>{1, 2});
    IntVecData b(std::vector<int>{9, 9, 9});

    b = std::move(a);
    PK_COMPARE(b.PkUseCount(), 1L);
    PK_VERIFY((b.PkConst() == std::vector<int>{1, 2}));

    PK_COMPARE(a.PkUseCount(), 0L);
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 源可以被重新赋成一个全新的值，并作为独立实例继续用
    a = IntVecData(std::vector<int>{3});
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY((a.PkConst() == std::vector<int>{3}));
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 移动赋值不释放共享给别人的那一份：源被移走后，其余持有者不受影响
    IntVecData x(std::vector<int>{8});
    IntVecData y(x);
    PK_COMPARE(x.PkUseCount(), 2L);
    IntVecData z;
    z = std::move(x);
    PK_COMPARE(z.PkUseCount(), 2L);
    PK_VERIFY(z.PkIsSharedWith(y));
    PK_VERIFY((y.PkConst() == std::vector<int>{8}));
}

PK_TEST_MAIN(PkArrayDataTest)
