// R-04 探针：问真 Qt 智能指针的语义，不推断。
// 编译运行方式见同目录 run_probe.sh；原始输出贴进 plan。
#include <QSharedPointer>
#include <QScopedPointer>
#include <QWeakPointer>
#include <QScopedArrayPointer>
#include <QHash>
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

#define LINE(fmt, ...) std::printf(fmt "\n", ##__VA_ARGS__)

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

    LINE("DONE live=%d", g_live);
    return 0;
}
