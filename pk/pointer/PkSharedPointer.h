#ifndef PK_SHARED_POINTER_H
#define PK_SHARED_POINTER_H

// PkSharedPointer<T> / PkWeakPointer<T> —— QSharedPointer<T>/QWeakPointer<T> 的替代品。
//
// 组合 std::shared_ptr/std::weak_ptr，不是继承（理由见
// docs/superpowers/plans/R-04.md §0「设计决定：组合，不是继承」）：公开继承会
// 把 use_count()/owner_before()/aliasing 构造/unique() 一并暴露出去，而这些在
// Qt 的 QSharedPointer 上根本不存在（真 Qt 5.15.13 编译矩阵实测：这几项全部
// FAIL）；判据 A 要改写 isNull 语义、判据 B 要改写布尔语境语义，两项都是
// "覆盖基类行为"，组合表达得干净。
//
// 四条设计判据（全部来自 pk/pointer/probe/qt_semantics_probe.cpp 的真 Qt 输出，
// 详见 §0）：
//   A. 判空按 value 判，不按控制块判——isNull() 是 data()==nullptr，
//      不是 use_count()==0。
//   B. operator bool 是隐式的 safe-bool（operator RestrictedBool），不是
//      explicit operator bool，也不是裸 operator bool。
//   C. PkScopedPointer/PkScopedArrayPointer 既不可拷贝也不可移动。
//      （在 PkScopedPointer.h 里实现，这里只是判据清单的一部分）
//   D. 空指针 + 自定义 deleter 时，Qt 照样调用 deleter——2 参构造/2 参 reset
//      直接转发给 std::shared_ptr 对应构造，不因为判据 A 就把空指针短路掉。

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

template <class T> class PkWeakPointer;

template <class T>
class PkSharedPointer
{
public:
    typedef T Type;
    typedef T *pointer;

    PkSharedPointer() noexcept {}
    PkSharedPointer(std::nullptr_t) noexcept {}
    explicit PkSharedPointer(T *ptr) : m_p(ptr) {}

    // 判据 D：空指针也要建控制块、也要调 deleter，直接转发给 std::shared_ptr。
    template <class Deleter>
    PkSharedPointer(T *ptr, Deleter d) : m_p(ptr, d) {}

    // 跨类型（派生→基类，探针 P16）；X* 不能转换到 T* 时这个重载直接从重载集里消失。
    template <class X, class = typename std::enable_if<std::is_convertible<X *, T *>::value>::type>
    PkSharedPointer(const PkSharedPointer<X> &other) : m_p(other.sharedImpl()) {}

    // 从弱引用提升；kis_pointer_utils.h:63 那处模板用的就是这个构造。
    template <class X, class = typename std::enable_if<std::is_convertible<X *, T *>::value>::type>
    explicit PkSharedPointer(const PkWeakPointer<X> &weak) { *this = weak.toStrongRef(); }

    // 判据 A：空 = value 为空，**不是** use_count()==0。
    T *data() const noexcept { return m_p.get(); }
    T *get() const noexcept { return m_p.get(); }
    bool isNull() const noexcept { return m_p.get() == nullptr; }

    // 判据 B：safe-bool，不是 explicit operator bool。用指向成员的指针做返回
    // 类型：`if (p)` / `bool b = p;` 能过，`p == 1` 编不过（探针编译矩阵实测）。
    typedef std::shared_ptr<T> PkSharedPointer::*RestrictedBool;
    operator RestrictedBool() const noexcept { return isNull() ? nullptr : &PkSharedPointer::m_p; }
    bool operator!() const noexcept { return isNull(); }

    T &operator*() const { return *m_p; }
    T *operator->() const noexcept { return m_p.get(); }

    void reset() { m_p.reset(); }
    void reset(T *ptr) { m_p.reset(ptr); }
    template <class Deleter> void reset(T *ptr, Deleter d) { m_p.reset(ptr, d); }
    void clear() { m_p.reset(); }

    template <class X> PkSharedPointer<X> staticCast() const
    { return PkSharedPointer<X>::fromShared(std::static_pointer_cast<X>(m_p)); }
    template <class X> PkSharedPointer<X> dynamicCast() const
    { return PkSharedPointer<X>::fromShared(std::dynamic_pointer_cast<X>(m_p)); }

    template <class... Args> static PkSharedPointer<T> create(Args &&...args)
    { return fromShared(std::make_shared<T>(std::forward<Args>(args)...)); }

private:
    // 给同族类型（PkWeakPointer）与自由函数用的内部通道；不是 Qt API 面的一部分。
    //
    // **必须落在 private: 之后**——评审 Important：这两个成员原先写在 public
    // 区（private: 标签之前），是事实上的公开成员，任何调用点都能写
    // `p.sharedImpl().use_count()`，正是「设计决定：组合，不是继承」那段
    // 专门要挡住的符号，抵消了选组合的全部理由，也违反判据①「一项不多」。
    // 类里已有的两条 friend 声明对全部实例化互相生效，`PkWeakPointer` 的
    // 构造、跨类型构造、dynamicCast/staticCast/create/toStrongRef 都不受影响。
    const std::shared_ptr<T> &sharedImpl() const noexcept { return m_p; }
    static PkSharedPointer<T> fromShared(std::shared_ptr<T> s) noexcept
    { PkSharedPointer<T> r; r.m_p = std::move(s); return r; }

    std::shared_ptr<T> m_p;
    template <class X> friend class PkSharedPointer;
    template <class X> friend class PkWeakPointer;
};

template <class T>
class PkWeakPointer
{
public:
    PkWeakPointer() noexcept {}
    PkWeakPointer(std::nullptr_t) noexcept {}
    template <class X, class = typename std::enable_if<std::is_convertible<X *, T *>::value>::type>
    PkWeakPointer(const PkSharedPointer<X> &s) noexcept
        : m_weak(s.sharedImpl()), m_value(s.data()) {}

    // 判据 A 的第二面：Qt 的 QWeakPointer::isNull() 是
    // `d == nullptr || d->strongref == 0 || value == nullptr`（qsharedpointer_impl.h:562）。
    // 控制块过期（strongref==0）与「控制块还在但 value 是空的那一格」都要算 null，
    // 后者靠 m_value 单独记（std::weak_ptr 本身没有暴露它包的裸指针）。
    bool isNull() const noexcept { return m_weak.expired() || m_value == nullptr; }

    PkSharedPointer<T> toStrongRef() const
    { return PkSharedPointer<T>::fromShared(m_weak.lock()); }

    // Qt 的 QWeakPointer::operator-> 直接给 value，不先提升（Qt5 已标 deprecated，
    // 但 Krita 有 2 处在用，行为必须逐字对齐）。
    T *operator->() const noexcept { return m_value; }

    typedef T *PkWeakPointer::*RestrictedBool;
    operator RestrictedBool() const noexcept { return isNull() ? nullptr : &PkWeakPointer::m_value; }
    bool operator!() const noexcept { return isNull(); }

private:
    std::weak_ptr<T> m_weak;
    T *m_value = nullptr;
};

// ---- 自由函数：cast / 比较 ----
// 用量：qSharedPointerCast 18 处、qSharedPointerDynamicCast 30 处、
// 比较运算符全仓 8 处、qHash 2 处（`QHash<KoPatternSP, QString>`）。

template <class X, class T> PkSharedPointer<X> pkSharedPointerCast(const PkSharedPointer<T> &p)
{ return p.template staticCast<X>(); }
template <class X, class T> PkSharedPointer<X> pkSharedPointerDynamicCast(const PkSharedPointer<T> &p)
{ return p.template dynamicCast<X>(); }

template <class T, class X>
bool operator==(const PkSharedPointer<T> &a, const PkSharedPointer<X> &b) noexcept
{ return a.data() == b.data(); }
template <class T, class X>
bool operator!=(const PkSharedPointer<T> &a, const PkSharedPointer<X> &b) noexcept
{ return a.data() != b.data(); }
template <class T, class X> bool operator==(const PkSharedPointer<T> &a, X *b) noexcept
{ return a.data() == b; }
template <class T, class X> bool operator!=(const PkSharedPointer<T> &a, X *b) noexcept
{ return a.data() != b; }
template <class T, class X> bool operator==(X *a, const PkSharedPointer<T> &b) noexcept
{ return a == b.data(); }
template <class T, class X> bool operator!=(X *a, const PkSharedPointer<T> &b) noexcept
{ return a != b.data(); }
template <class T> bool operator==(const PkSharedPointer<T> &a, std::nullptr_t) noexcept
{ return a.isNull(); }
template <class T> bool operator!=(const PkSharedPointer<T> &a, std::nullptr_t) noexcept
{ return !a.isNull(); }

// **有意不实现**：operator==(const PkWeakPointer<T>&, const PkSharedPointer<X>&)。
// 探针 P12 证明 Qt 有这个重载（weak==shared:1），但那只证明 Qt 有它，不证明
// Krita 用它——用量表 `.superpowers/sdd/R-04/usage-table.md` §2.3 的
// QWeakPointer 一节没有 operator== 这一行，保留范围用量是 0。判据①「一项
// 不多」优先于计划 Step 4 示例代码：这里是计划的示例代码写多了，用量表对。
// 登记进「有意不实现」清单，Task 4 收（该 README 由 Task 4 交付）。

// ---- Step 9：接 pk/container 的哈希 ----
//
// pk/container/PkHashFunctions.h（只读，一个字节没改）靠**非限定名字 qHash(k) +
// ADL** 求哈希：PkHasher<K>::operator() 在 PkSharedPointer<T> 的实例化点找
// qHash 时，ADL 把 PkSharedPointer 所在的命名空间（全局）纳入关联集合，
// 于是这里给出的重载会被找到——不需要改 pk/container 一个字节。
//
// 名字是 **qHash 不是 pkHash**：pk/container 的 hasher 硬编码调用
// `qHash(k)`（无命名空间前缀），起别的名字它找不到。这一处签名与最初计划稿
// 里的 `pkHash` 不同，是按 pk/container 实测机制定的，见任务报告。
//
// 哈希值取 data() 的指针哈希，与 Qt 的 qHash(const QSharedPointer<T>&)
// 同源——探针 P13 证明"拷贝的哈希相等"，指针哈希天然满足这条（同一 data()）。
template <class T>
inline unsigned int qHash(const PkSharedPointer<T> &p, unsigned int seed = 0) noexcept
{
    return qHash(p.data(), seed);
}

#endif // PK_SHARED_POINTER_H
