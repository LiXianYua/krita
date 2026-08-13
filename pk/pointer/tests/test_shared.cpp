#include "cases/shared_case.h"
#include "../PkSharedPointer.h"
#include "../../container/PkHash.h"

// PkTestBinder<PkSharedPointerCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样
// 直接 #include 进本 TU（显式特化必须在 qExec<T> 实例化前对本 TU 可见，理由与
// pk/geometry/tests/test_point.cpp 相同）。
#include "pk_binder_shared_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部来自 pk/pointer/probe/qt_semantics_probe.cpp 的真 Qt 5.15.13 输出
// （docs/superpowers/plans/R-04.md §0），逐条对应探针的某一行——不是推的。
// ---------------------------------------------------------------------------

// 类型名带 Shared 前缀、留在全局命名空间（不放进匿名命名空间）：
//
// 三个测试 .cpp 各自定义一份 SharedBase/SharedDerived/SharedUnrelated，它们的构造/析构是隐式
// inline（vague/COMDAT 链接）。若三份都叫 SharedBase 且都在全局作用域，链接器会把
// 它们的构造/析构函数当成同一个符号折叠成一份——折叠后哪个 TU 的 g_sharedLive 被
// 真正自增，取决于链接顺序，与调用它的 TU 无关（实测复现：早期版本 g_sharedLive
// 与 SharedBase 都叫这个名字时，test_scoped.cpp 里 `new SharedBase(8)` 之后 g_sharedLive 读到
// 0，因为 SharedBase::SharedBase(int) 被折叠成了 test_shared.cpp 的版本）。
//
// 改类型名唯一化即可解决，**不能改放进匿名命名空间**：usableAsHashKey() 要
// 靠 pk/container 的 qHash(k)+ADL 机制找到 PkSharedPointer.h 里的
// `qHash(const T*, seed)` 重载，那条链路要求 T 的关联命名空间包含全局命名空间
// ——匿名命名空间是它自己的命名空间，不会把全局命名空间带进 ADL 的关联集合，
// 放进去会让 usableAsHashKey() 编不过（已实测复现并改回）。
static int g_sharedLive = 0;
struct SharedBase {
    int v;
    explicit SharedBase(int x = 7) : v(x) { ++g_sharedLive; }
    virtual ~SharedBase() { --g_sharedLive; }
};
struct SharedDerived : SharedBase { explicit SharedDerived(int x = 9) : SharedBase(x) {} };
struct SharedUnrelated : SharedBase {};

static int g_nonVirtualBaseDestroyed = 0;
static int g_nonVirtualDerivedDestroyed = 0;
struct SharedNonVirtualBase {
    ~SharedNonVirtualBase() { ++g_nonVirtualBaseDestroyed; }
};
struct SharedNonVirtualDerived : SharedNonVirtualBase {
    ~SharedNonVirtualDerived() { ++g_nonVirtualDerivedDestroyed; }
};
struct SharedGenericDeleter {
    template <class X>
    void operator()(X *p) const { delete p; }
};

void PkSharedPointerCase::init() { g_sharedLive = 0; }

// 探针 P1：qt default: isNull=1 data=(nil) bool=0
void PkSharedPointerCase::defaultCtorIsNull()
{
    PkSharedPointer<SharedBase> p;
    PK_VERIFY(p.isNull());
    PK_COMPARE(p.data(), static_cast<SharedBase *>(nullptr));
    PK_VERIFY(!p);
    PK_VERIFY(!bool(p));
}

// 探针 P2：qt from-null: isNull=1，且由它产生的弱引用也是 null。
// 判据 A：std::shared_ptr((T*)nullptr) 的 use_count 是 1，isNull 绝不能按控制块判。
void PkSharedPointerCase::ctorFromNullRawPointerIsNull()
{
    PkSharedPointer<SharedBase> p(static_cast<SharedBase *>(nullptr));
    PK_VERIFY(p.isNull());
    PK_VERIFY(!p);
    PkWeakPointer<SharedBase> w = p;
    PK_VERIFY(w.isNull());
    PK_VERIFY(w.toStrongRef().isNull());
}

// 探针 P3：after copy dtor live=1；after clear() live=0
void PkSharedPointerCase::refcountControlsDestruction()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    PK_COMPARE(g_sharedLive, 1);
    { PkSharedPointer<SharedBase> b = a; PK_COMPARE(g_sharedLive, 1); }
    PK_COMPARE(g_sharedLive, 1);
    a.clear();
    PK_COMPARE(g_sharedLive, 0);
    PK_VERIFY(a.isNull());
}

// 探针 P4：reset(new) live=1 v=2；reset() live=0
void PkSharedPointerCase::resetReplacesAndReleases()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    a.reset(new SharedBase(2));
    PK_COMPARE(g_sharedLive, 1);
    PK_COMPARE(a->v, 2);
    a.reset();
    PK_COMPARE(g_sharedLive, 0);
    PK_VERIFY(a.isNull());
}

// std::shared_ptr preserves the raw pointer's concrete X type in its control
// block. The Pk wrapper must not erase X to the exposed T before construction
// or either reset overload, otherwise a non-virtual base loses the derived
// destructor. The generic deleter also reveals which pointer type it receives.
void PkSharedPointerCase::preservesDynamicDeletionType()
{
    const auto resetCounts = [] {
        g_nonVirtualBaseDestroyed = 0;
        g_nonVirtualDerivedDestroyed = 0;
    };
    const auto verifyBothDestroyed = [] {
        PK_COMPARE(g_nonVirtualBaseDestroyed, 1);
        PK_COMPARE(g_nonVirtualDerivedDestroyed, 1);
    };

    resetCounts();
    { PkSharedPointer<SharedNonVirtualBase> p(new SharedNonVirtualDerived); }
    verifyBothDestroyed();

    resetCounts();
    {
        PkSharedPointer<SharedNonVirtualBase> p;
        p.reset(new SharedNonVirtualDerived);
    }
    verifyBothDestroyed();

    resetCounts();
    {
        PkSharedPointer<SharedNonVirtualBase> p(new SharedNonVirtualDerived,
                                                SharedGenericDeleter());
    }
    verifyBothDestroyed();

    resetCounts();
    {
        PkSharedPointer<SharedNonVirtualBase> p;
        p.reset(new SharedNonVirtualDerived, SharedGenericDeleter());
    }
    verifyBothDestroyed();
}

// 探针 P6：dynamicCast 到无关类型返回 null；对 null 做 dynamicCast 不崩、返回 null
void PkSharedPointerCase::casts()
{
    PkSharedPointer<SharedBase> b(new SharedDerived(11));
    PkSharedPointer<SharedDerived> d = b.dynamicCast<SharedDerived>();
    PK_VERIFY(!d.isNull());
    PK_COMPARE(d->v, 11);
    PK_VERIFY(b.dynamicCast<SharedUnrelated>().isNull());
    PK_VERIFY(!b.staticCast<SharedDerived>().isNull());
    PkSharedPointer<SharedBase> nullb;
    PK_VERIFY(nullb.dynamicCast<SharedDerived>().isNull());
    PK_VERIFY(!pkSharedPointerDynamicCast<SharedDerived>(b).isNull());
    PK_VERIFY(!pkSharedPointerCast<SharedDerived>(b).isNull());
}

// 探针 P7：create(3,4) v=304；create() 走默认参数 v=7
void PkSharedPointerCase::create()
{
    struct NoDefault { int v; NoDefault(int a, int b) : v(a * 100 + b) {} };
    PK_COMPARE(PkSharedPointer<NoDefault>::create(3, 4)->v, 304);
    PK_COMPARE(PkSharedPointer<SharedBase>::create()->v, 7);
}

// 探针 D3：空指针也要调 deleter（qt deleter calls on null = 1）。
// 探针 P8：2 参 reset 之后再 reset()，deleter 恰好调 1 次。
void PkSharedPointerCase::customDeleter()
{
    static int calls = 0;
    struct H { static void del(SharedBase *p) { ++calls; delete p; } };
    calls = 0;
    { PkSharedPointer<SharedBase> p(new SharedBase(5), &H::del); }
    PK_COMPARE(calls, 1);
    PK_COMPARE(g_sharedLive, 0);

    calls = 0;
    { PkSharedPointer<SharedBase> p(static_cast<SharedBase *>(nullptr), &H::del); }
    PK_COMPARE(calls, 1);        // ← 判据 D，别短路掉

    calls = 0;
    PkSharedPointer<SharedBase> q;
    q.reset(new SharedBase(6), &H::del);
    q.reset();
    PK_COMPARE(calls, 1);
}

// 探针 P15：自赋值不释放
void PkSharedPointerCase::selfAssignKeepsAlive()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    PkSharedPointer<SharedBase> &ref = a;
    a = ref;
    PK_COMPARE(g_sharedLive, 1);
    PK_VERIFY(!a.isNull());
    PK_COMPARE(a->v, 1);
}

// 探针 P12：与另一智能指针、与裸指针比较
void PkSharedPointerCase::comparisons()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    PkSharedPointer<SharedBase> b = a;
    SharedBase *raw = a.data();
    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    PK_VERIFY(a == raw);
    PK_VERIFY(!(a != raw));
    PkSharedPointer<SharedDerived> d(new SharedDerived(2));
    PkSharedPointer<SharedBase> db = d;      // 探针 P16：派生→基类隐式转换
    PK_VERIFY(db == d);
}

void PkSharedPointerCase::assignNullptrClears()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    a = nullptr;
    PK_VERIFY(a.isNull());
    PK_COMPARE(g_sharedLive, 0);
}

// Step 9：接 pk/container 的哈希——PkHash<K,V> 靠非限定名字 qHash(k) + ADL 找
// PkSharedPointer.h 里给的那个重载。探针 P13 证明拷贝的哈希相等，这里用指针
// 哈希（data() 相同即哈希相同）间接满足同一条不变量。
void PkSharedPointerCase::usableAsHashKey()
{
    PkSharedPointer<SharedBase> a(new SharedBase(1));
    PkHash<PkSharedPointer<SharedBase>, int> h;
    h.insert(a, 42);
    PK_COMPARE(h.value(a), 42);
    PK_COMPARE(h.size(), 1);
}

int run_shared_tests()
{
    PkSharedPointerCase tc;
    const char *argv[] = {"test_pkpointer"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
