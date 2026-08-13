#include "cases/weak_case.h"
#include "../PkSharedPointer.h"

#include "pk_binder_weak_case.inc"

// 匿名命名空间 + 唯一类型名（WeakBase，而非裸 Base）：防止本文件与
// test_shared.cpp/test_scoped.cpp 各自的同名 struct 的 vague-linkage 构造/
// 析构函数被链接器跨 TU 折叠成一份（实测复现过：折叠后哪个 TU 的活体计数被
// 真正自增，取决于链接顺序，与调用它的 TU 无关，细节见 test_shared.cpp 同一
// 处更长的注释）。本文件不需要 ADL 找 pk/container 的 qHash（那是
// test_shared.cpp 的 usableAsHashKey() 才需要的），所以可以放心用匿名命名
// 空间；test_shared.cpp 因为要保留 ADL 可见性，改用的是"只唯一化类型名、
// 不进匿名命名空间"那条路。
namespace {
int g_weakLive = 0;
struct WeakBase {
    int v;
    explicit WeakBase(int x = 7) : v(x) { ++g_weakLive; }
    virtual ~WeakBase() { --g_weakLive; }
};
} // namespace

void PkWeakPointerCase::init() { g_weakLive = 0; }

// 探针 P5：default weak isNull=1；alive isNull=0；expired isNull=1 且
// toStrongRef 为 null。
void PkWeakPointerCase::weakLifecycle()
{
    PkWeakPointer<WeakBase> w;
    PK_VERIFY(w.isNull());
    PK_VERIFY(w.toStrongRef().isNull());
    {
        PkSharedPointer<WeakBase> a(new WeakBase(3));
        w = a;
        PK_VERIFY(!w.isNull());
        PK_VERIFY(!w.toStrongRef().isNull());
        PK_COMPARE(w.toStrongRef()->v, 3);
        PK_COMPARE(w->v, 3);              // Qt 的 operator-> 不先提升，直接给 value
    }
    PK_VERIFY(w.isNull());
    PK_VERIFY(w.toStrongRef().isNull());
    PK_COMPARE(g_weakLive, 0);
}

// 判据 A 的第二面：控制块还在、value 是空的那一格。
void PkWeakPointerCase::weakOfNullValuedSharedIsNull()
{
    static int calls = 0;
    struct H { static void del(WeakBase *p) { ++calls; delete p; } };
    PkSharedPointer<WeakBase> p(static_cast<WeakBase *>(nullptr), &H::del);
    PkWeakPointer<WeakBase> w = p;
    PK_VERIFY(w.isNull());               // 强引用还活着，但 value 是 nullptr
    PK_VERIFY(w.toStrongRef().isNull());
}

// kis_pointer_utils.h:63 那处模板：从 weak 提升成 strong。
void PkWeakPointerCase::promoteFromWeak()
{
    PkSharedPointer<WeakBase> a(new WeakBase(4));
    PkWeakPointer<WeakBase> w = a;
    PkSharedPointer<WeakBase> strong(w);
    PK_VERIFY(!strong.isNull());
    PK_COMPARE(strong->v, 4);
}

int run_weak_tests()
{
    PkWeakPointerCase tc;
    const char *argv[] = {"test_pkpointer"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
