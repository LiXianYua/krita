// R-04 探针：问真 Qt 智能指针的语义，不推断；P18 同时与 Pk 对照。
// 编译运行方式见同目录 run_probe.sh；初始原始输出贴进 plan，修订证据在 Task 4 报告。
#include <QSharedPointer>
#include <QScopedPointer>
#include <QWeakPointer>
#include <QScopedArrayPointer>
#include <QHash>
#include "../PkSharedPointer.h"
#include <cstdio>
#include <memory>
#include <type_traits>

static int g_live = 0;
struct Base {
    int v;
    explicit Base(int x = 7) : v(x) { ++g_live; }
    virtual ~Base() { --g_live; }
};
struct Derived : Base {
    explicit Derived(int x = 9) : Base(x) {}
};
struct Unrelated : Base {};
struct NoDefault {
    int v;
    explicit NoDefault(int a, int b) : v(a * 100 + b) {}
};

static int g_deleterCalls = 0;
static void fnDeleter(Base *p) { ++g_deleterCalls; delete p; }

static int g_deleteBaseDestroyed = 0;
static int g_deleteDerivedDestroyed = 0;
struct DeleteBase {
protected:
    ~DeleteBase() { ++g_deleteBaseDestroyed; }
};
struct DeleteDerived : DeleteBase {
    ~DeleteDerived() { ++g_deleteDerivedDestroyed; }
};
struct TypePreservingDeleter {
    template <class X>
    void operator()(X *p) const { delete p; }
};

static int g_resetDeleterSawBase = 0;
static int g_resetDeleterSawDerived = 0;
struct SafeResetTypeDeleter {
    void operator()(DeleteBase *p) const
    {
        ++g_resetDeleterSawBase;
        // Every call below supplies a real DeleteDerived. Downcast before delete so the
        // non-virtual base is never used as the static type of a delete expression.
        delete static_cast<DeleteDerived *>(p);
    }
    void operator()(DeleteDerived *p) const
    {
        ++g_resetDeleterSawDerived;
        delete p;
    }
};

template <class Pointer>
static auto hasDerivedDefaultResetTemplate(int) -> decltype(
    static_cast<void (Pointer::*)(DeleteDerived *)>(
        &Pointer::template reset<DeleteDerived>),
    std::true_type());
template <class>
static std::false_type hasDerivedDefaultResetTemplate(...);

template <class Pointer>
static auto hasFixedDefaultReset(int) -> decltype(
    static_cast<void (Pointer::*)(DeleteBase *)>(&Pointer::reset),
    std::true_type());
template <class>
static std::false_type hasFixedDefaultReset(...);

struct NoReset {
};
struct WrongFixedResetShape {
    void reset(void *) {}
};
struct WrongTemplatedReset {
    template <class X>
    void reset(X *) {}
};

template <class Pointer>
using HasFixedDefaultReset = std::integral_constant<bool,
    decltype(hasFixedDefaultReset<Pointer>(0))::value &&
    !decltype(hasDerivedDefaultResetTemplate<Pointer>(0))::value>;

static_assert(!decltype(hasFixedDefaultReset<NoReset>(0))::value,
              "no reset cannot satisfy the positive exact-signature check");
static_assert(!decltype(hasDerivedDefaultResetTemplate<NoReset>(0))::value,
              "no reset also has no derived member-template reset");
static_assert(!HasFixedDefaultReset<NoReset>::value,
              "a type without reset must not be classified as fixed-T*");
static_assert(!decltype(hasFixedDefaultReset<WrongFixedResetShape>(0))::value,
              "reset(void*) cannot satisfy the positive exact-signature check");
static_assert(!decltype(hasDerivedDefaultResetTemplate<WrongFixedResetShape>(0))::value,
              "reset(void*) is not a derived member-template reset");
static_assert(!HasFixedDefaultReset<WrongFixedResetShape>::value,
              "reset(void*) must not be classified as fixed-T*");
static_assert(decltype(hasFixedDefaultReset<WrongTemplatedReset>(0))::value,
              "template deduction alone can satisfy the positive signature check");
static_assert(decltype(hasDerivedDefaultResetTemplate<WrongTemplatedReset>(0))::value,
              "the negative check must expose the derived member-template reset");
static_assert(!HasFixedDefaultReset<WrongTemplatedReset>::value,
              "reset<X>(X*) must not be classified as fixed-T*");

static_assert(
    decltype(hasFixedDefaultReset<QSharedPointer<DeleteBase>>(0))::value,
    "QSharedPointer::reset must have the exact void (Pointer::*)(T*) shape");
static_assert(
    decltype(hasFixedDefaultReset<PkSharedPointer<DeleteBase>>(0))::value,
    "PkSharedPointer::reset must match Qt's exact one-argument reset shape");

static_assert(
    !decltype(hasDerivedDefaultResetTemplate<QSharedPointer<DeleteBase>>(0))::value,
    "QSharedPointer::reset must expose fixed T*, not a reset<X>(X*) template");
static_assert(
    !decltype(hasDerivedDefaultResetTemplate<PkSharedPointer<DeleteBase>>(0))::value,
    "PkSharedPointer::reset must match Qt's fixed-T* API shape");

static int g_nullDeleterCalls = 0;
static int g_nullDeleterSawNull = 0;
struct NullBaseDeleter {
    void operator()(Base *p) const
    {
        ++g_nullDeleterCalls;
        g_nullDeleterSawNull = p == nullptr;
        delete p;
    }
};

#define LINE(fmt, ...) std::printf(fmt "\n", ##__VA_ARGS__)

template <template <class> class SharedPointer>
static void probeDeletionSemantics(const char *label)
{
    g_deleteBaseDestroyed = g_deleteDerivedDestroyed = 0;
    { SharedPointer<DeleteBase> p(new DeleteDerived); }
    LINE("%s ctor-default base=%d derived=%d", label,
         g_deleteBaseDestroyed, g_deleteDerivedDestroyed);

    const bool hasDerivedReset =
        decltype(hasDerivedDefaultResetTemplate<SharedPointer<DeleteBase>>(0))::value;
    const bool hasFixedReset =
        HasFixedDefaultReset<SharedPointer<DeleteBase>>::value;
    LINE("%s reset-default fixed-T-star=%d derived-reset-template=%d", label,
         hasFixedReset, hasDerivedReset);

    g_deleteBaseDestroyed = g_deleteDerivedDestroyed = 0;
    { SharedPointer<DeleteBase> p(new DeleteDerived, TypePreservingDeleter()); }
    LINE("%s ctor-deleter base=%d derived=%d", label,
         g_deleteBaseDestroyed, g_deleteDerivedDestroyed);

    g_deleteBaseDestroyed = g_deleteDerivedDestroyed = 0;
    g_resetDeleterSawBase = g_resetDeleterSawDerived = 0;
    {
        SharedPointer<DeleteBase> p;
        p.reset(new DeleteDerived, SafeResetTypeDeleter());
    }
    LINE("%s reset-deleter static-base=%d static-derived=%d base=%d derived=%d", label,
         g_resetDeleterSawBase, g_resetDeleterSawDerived,
         g_deleteBaseDestroyed, g_deleteDerivedDestroyed);

    g_nullDeleterCalls = g_nullDeleterSawNull = 0;
    { SharedPointer<Base> p(nullptr, NullBaseDeleter()); }
    LINE("%s nullptr-deleter calls=%d sawNull=%d", label,
         g_nullDeleterCalls, g_nullDeleterSawNull);
}

int main()
{
    LINE("== P1 default-ctor ==");
    {
        QSharedPointer<Base> q;
        std::shared_ptr<Base> s;
        LINE("qt   default: isNull=%d data=%p bool=%d", (int)q.isNull(), (void*)q.data(), (int)(bool)q);
        LINE("std  default: get=%p bool=%d use_count=%ld", (void*)s.get(), (int)(bool)s, (long)s.use_count());
    }

    LINE("== P2 ctor from NULL raw pointer (控制块分配与否) ==");
    {
        QSharedPointer<Base> q(static_cast<Base*>(nullptr));
        std::shared_ptr<Base> s(static_cast<Base*>(nullptr));
        QWeakPointer<Base> qw = q.toWeakRef();
        std::weak_ptr<Base> sw = s;
        LINE("qt   from-null: isNull=%d data=%p bool=%d  weak.toStrongRef().isNull=%d weak.isNull=%d",
             (int)q.isNull(), (void*)q.data(), (int)(bool)q,
             (int)qw.toStrongRef().isNull(), (int)qw.isNull());
        LINE("std  from-null: get=%p bool=%d use_count=%ld weak.expired=%d weak.use_count=%ld",
             (void*)s.get(), (int)(bool)s, (long)s.use_count(), (int)sw.expired(), (long)sw.use_count());
    }

    LINE("== P3 引用计数可观测性：析构时机 ==");
    {
        g_live = 0;
        QSharedPointer<Base> a(new Base(1));
        LINE("qt   after ctor        live=%d", g_live);
        {
            QSharedPointer<Base> b = a;
            LINE("qt   after copy        live=%d", g_live);
        }
        LINE("qt   after copy dtor   live=%d", g_live);
        a.clear();
        LINE("qt   after clear()     live=%d isNull=%d", g_live, (int)a.isNull());
        a.reset();
        LINE("qt   after reset()     live=%d isNull=%d", g_live, (int)a.isNull());
    }

    LINE("== P4 reset(T*) 与 clear() ==");
    {
        g_live = 0;
        QSharedPointer<Base> a(new Base(1));
        a.reset(new Base(2));
        LINE("qt   reset(new) live=%d v=%d", g_live, a->v);
        a.reset();
        LINE("qt   reset()    live=%d isNull=%d", g_live, (int)a.isNull());
    }

    LINE("== P5 QWeakPointer：过期后 ==");
    {
        g_live = 0;
        QWeakPointer<Base> w;
        LINE("qt   default weak: isNull=%d toStrongRef.isNull=%d", (int)w.isNull(), (int)w.toStrongRef().isNull());
        {
            QSharedPointer<Base> a(new Base(3));
            w = a.toWeakRef();
            LINE("qt   alive  weak: isNull=%d toStrongRef.isNull=%d v=%d", (int)w.isNull(),
                 (int)w.toStrongRef().isNull(), w.toStrongRef()->v);
        }
        LINE("qt   expired weak: isNull=%d toStrongRef.isNull=%d live=%d", (int)w.isNull(),
             (int)w.toStrongRef().isNull(), g_live);
        std::weak_ptr<Base> sw;
        {
            std::shared_ptr<Base> a(new Base(3));
            sw = a;
        }
        LINE("std  expired weak: expired=%d lock==null=%d", (int)sw.expired(), (int)(sw.lock() == nullptr));
    }

    LINE("== P6 casts ==");
    {
        QSharedPointer<Base> b(new Derived(11));
        QSharedPointer<Derived> d = b.dynamicCast<Derived>();
        QSharedPointer<Unrelated> u = b.dynamicCast<Unrelated>();
        QSharedPointer<Derived> ds = b.staticCast<Derived>();
        QSharedPointer<const Base> cb = b;
        QSharedPointer<Base> nc = qSharedPointerConstCast<Base>(cb);
        LINE("qt   dynamicCast ok isNull=%d v=%d | to-unrelated isNull=%d | staticCast isNull=%d | constCast isNull=%d",
             (int)d.isNull(), d->v, (int)u.isNull(), (int)ds.isNull(), (int)nc.isNull());
        QSharedPointer<Base> nullb;
        LINE("qt   dynamicCast on null: isNull=%d", (int)nullb.dynamicCast<Derived>().isNull());
        LINE("qt   free qSharedPointerDynamicCast isNull=%d", (int)qSharedPointerDynamicCast<Derived>(b).isNull());
    }

    LINE("== P7 create() ==");
    {
        auto p = QSharedPointer<NoDefault>::create(3, 4);
        LINE("qt   create(3,4) v=%d", p->v);
        auto z = QSharedPointer<Base>::create();
        LINE("qt   create() default v=%d", z->v);
    }

    LINE("== P8 自定义 deleter（2 参 ctor / 2 参 reset）==");
    {
        g_deleterCalls = 0; g_live = 0;
        {
            QSharedPointer<Base> p(new Base(5), fnDeleter);
            LINE("qt   with deleter live=%d", g_live);
        }
        LINE("qt   after scope deleterCalls=%d live=%d", g_deleterCalls, g_live);
        g_deleterCalls = 0;
        QSharedPointer<Base> p2;
        p2.reset(new Base(6), fnDeleter);
        p2.reset();
        LINE("qt   reset(ptr,deleter) then reset() deleterCalls=%d", g_deleterCalls);
    }

    LINE("== P9 QScopedPointer ==");
    {
        g_live = 0;
        QScopedPointer<Base> sp(new Base(8));
        LINE("qt   scoped: isNull=%d bool=%d v=%d live=%d", (int)sp.isNull(), (int)(bool)sp, sp->v, g_live);
        Base *taken = sp.take();
        LINE("qt   after take(): isNull=%d data=%p live=%d", (int)sp.isNull(), (void*)sp.data(), g_live);
        sp.reset(taken);
        LINE("qt   after reset(taken): isNull=%d v=%d live=%d", (int)sp.isNull(), sp->v, g_live);
        sp.reset();
        LINE("qt   after reset(): isNull=%d live=%d", (int)sp.isNull(), g_live);
        QScopedPointer<Base> def;
        LINE("qt   scoped default: isNull=%d bool=%d data=%p", (int)def.isNull(), (int)(bool)def, (void*)def.data());
    }

    LINE("== P10 QScopedPointer 的移动/拷贝能力 ==");
    LINE("qt   QScopedPointer<Base>: copy_ctor=%d move_ctor=%d copy_assign=%d move_assign=%d",
         (int)std::is_copy_constructible<QScopedPointer<Base>>::value,
         (int)std::is_move_constructible<QScopedPointer<Base>>::value,
         (int)std::is_copy_assignable<QScopedPointer<Base>>::value,
         (int)std::is_move_assignable<QScopedPointer<Base>>::value);
    LINE("std  std::unique_ptr<Base>: copy_ctor=%d move_ctor=%d copy_assign=%d move_assign=%d",
         (int)std::is_copy_constructible<std::unique_ptr<Base>>::value,
         (int)std::is_move_constructible<std::unique_ptr<Base>>::value,
         (int)std::is_copy_assignable<std::unique_ptr<Base>>::value,
         (int)std::is_move_assignable<std::unique_ptr<Base>>::value);
    LINE("qt   QSharedPointer<Base>: copy=%d move=%d | std::shared_ptr copy=%d move=%d",
         (int)std::is_copy_constructible<QSharedPointer<Base>>::value,
         (int)std::is_move_constructible<QSharedPointer<Base>>::value,
         (int)std::is_copy_constructible<std::shared_ptr<Base>>::value,
         (int)std::is_move_constructible<std::shared_ptr<Base>>::value);

    LINE("== P11 operator bool 是否 explicit ==");
    LINE("qt   QSharedPointer->bool implicit=%d | QScopedPointer->bool implicit=%d | QWeakPointer->bool implicit=%d",
         (int)std::is_convertible<QSharedPointer<Base>, bool>::value,
         (int)std::is_convertible<QScopedPointer<Base>, bool>::value,
         (int)std::is_convertible<QWeakPointer<Base>, bool>::value);
    LINE("std  shared_ptr->bool implicit=%d | unique_ptr->bool implicit=%d",
         (int)std::is_convertible<std::shared_ptr<Base>, bool>::value,
         (int)std::is_convertible<std::unique_ptr<Base>, bool>::value);

    LINE("== P12 比较运算符可用性（编译期）==");
    {
        QSharedPointer<Base> a(new Base(1));
        QSharedPointer<Base> b = a;
        Base *raw = a.data();
        LINE("qt   a==b:%d a!=b:%d a==raw:%d a!=raw:%d a==nullptr:%d",
             (int)(a == b), (int)(a != b), (int)(a == raw), (int)(a != raw), (int)(a == nullptr));
        QSharedPointer<Derived> d(new Derived(2));
        QSharedPointer<Base> db = d;
        LINE("qt   cross-type db==d:%d", (int)(db == d));
        QWeakPointer<Base> w = a.toWeakRef();
        LINE("qt   weak==shared:%d", (int)(w == a));
    }

    LINE("== P13 qHash / QHash key ==");
    {
        QSharedPointer<Base> a(new Base(1));
        QHash<QSharedPointer<Base>, int> h;
        h.insert(a, 42);
        LINE("qt   QHash<QSharedPointer,int> value=%d size=%d", h.value(a), h.size());
        LINE("qt   qHash(a)==qHash(copy): %d", (int)(qHash(a) == qHash(QSharedPointer<Base>(a))));
    }

    LINE("== P14 QScopedArrayPointer ==");
    {
        g_live = 0;
        QScopedArrayPointer<Base> arr(new Base[3]);
        LINE("qt   array: isNull=%d live=%d [1].v=%d", (int)arr.isNull(), g_live, arr[1].v);
        arr.reset();
        LINE("qt   after reset live=%d isNull=%d", g_live, (int)arr.isNull());
    }

    LINE("== P15 self-assign / self-reset ==");
    {
        g_live = 0;
        QSharedPointer<Base> a(new Base(1));
        a = a;
        LINE("qt   self-assign: live=%d isNull=%d v=%d", g_live, (int)a.isNull(), a->v);
    }

    LINE("== P16 派生→基类 隐式转换 ==");
    LINE("qt   QSharedPointer<Derived>→<Base> convertible=%d | <Base>→<Derived>=%d",
         (int)std::is_convertible<QSharedPointer<Derived>, QSharedPointer<Base>>::value,
         (int)std::is_convertible<QSharedPointer<Base>, QSharedPointer<Derived>>::value);
    LINE("qt   QSharedPointer<Base>→<const Base> convertible=%d",
         (int)std::is_convertible<QSharedPointer<Base>, QSharedPointer<const Base>>::value);

    LINE("== P17 QSharedPointer 有无 aliasing ctor / get() / use_count() ==");
    LINE("(编译矩阵见 compile_matrix 部分)");

    LINE("== P18 构造与 reset 的删除类型 + nullptr deleter ==");
    probeDeletionSemantics<QSharedPointer>("qt  ");
    probeDeletionSemantics<PkSharedPointer>("pk  ");

    LINE("DONE live=%d", g_live);
    return 0;
}
