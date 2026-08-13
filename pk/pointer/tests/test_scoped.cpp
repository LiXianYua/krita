#include "cases/scoped_case.h"
#include "../PkScopedPointer.h"

#include "pk_binder_scoped_case.inc"

// 匿名命名空间 + 唯一类型名（ScopedBase，而非裸 Base）：理由与
// test_weak.cpp 同一处注释相同——防跨 TU 的 vague-linkage 折叠，本文件同样
// 不需要 ADL 找 pk/container 的 qHash，可以放心用匿名命名空间。
namespace {
int g_scopedLive = 0;
struct ScopedBase {
    int v;
    explicit ScopedBase(int x = 7) : v(x) { ++g_scopedLive; }
    virtual ~ScopedBase() { --g_scopedLive; }
};
} // namespace

void PkScopedPointerCase::init() { g_scopedLive = 0; }

// 探针 P9：take() 之后 isNull=1 而 live 仍为 1（所有权转出，对象没死）。
void PkScopedPointerCase::scopedTakeTransfersOwnership()
{
    PkScopedPointer<ScopedBase> sp(new ScopedBase(8));
    PK_VERIFY(!sp.isNull());
    PK_COMPARE(sp->v, 8);
    PK_COMPARE(g_scopedLive, 1);
    ScopedBase *taken = sp.take();
    PK_VERIFY(sp.isNull());
    PK_COMPARE(sp.data(), static_cast<ScopedBase *>(nullptr));
    PK_COMPARE(g_scopedLive, 1);
    sp.reset(taken);
    PK_COMPARE(sp->v, 8);
    sp.reset();
    PK_COMPARE(g_scopedLive, 0);
}

// 探针 P14：QScopedArrayPointer 用 delete[] 释放，三个元素全析构。
void PkScopedPointerCase::scopedArrayDeletesAll()
{
    PkScopedArrayPointer<ScopedBase> arr(new ScopedBase[3]);
    PK_COMPARE(g_scopedLive, 3);
    PK_COMPARE(arr[1].v, 7);
    PK_VERIFY(arr.data() != nullptr);
    arr.reset();
    PK_COMPARE(g_scopedLive, 0);
}

void PkScopedPointerCase::scopedComparisons()
{
    PkScopedPointer<ScopedBase> a(new ScopedBase(1));
    PkScopedPointer<ScopedBase> b;
    PK_VERIFY(!(a == b));
    PK_VERIFY(a != b);
    PK_VERIFY(b == nullptr);
    PK_VERIFY(a != nullptr);
}

int run_scoped_tests()
{
    PkScopedPointerCase tc;
    const char *argv[] = {"test_pkpointer"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
