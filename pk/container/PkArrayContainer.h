#pragma once

#include "PkArrayData.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// PkArrayContainer<T, Derived> —— PkVector<T> 与 PkList<T> 的**共同实现**。
//
// 为什么是一份而不是两份：Qt5 的 QVector 与 QList 方法集有重叠也有各自专属
// （QVector 有 resize/fill/capacity/toList，QList 有 takeAt/move/removeAll/…），
// 但重叠的那几十个方法语义完全一致。抄两份的话，任何一次修正都要记得改两个
// 地方——漏改的那一半就是一个静默的 COW 漏洞。
//
// 为什么用 CRTP（把 Derived 当模板参数）而不是普通基类：`append()` 之后要能
// 接着 `<<`，`operator<<`/`operator+=`/`fill` 这些必须返回**派生类**的引用
// （`PkVector<int> &`），而不是基类引用——调用点写的是
// `v << 1 << 2`、`f(v.fill(0))`，返回基类引用会当场编不过。
//
// 内层一律 std::vector<T>：Qt5 的 QList<T> 在 sizeof(T) <= sizeof(void*) 且可
// 移动时本来就是连续数组，大对象才退化成指针数组；我们不复刻那条指针数组路径
// （它是 Qt 为了 QList<QVariant> 之类的老包袱留的），语义上 QList 的公开行为
// 用连续数组全都能给到。
//
// 三条硬要求（照 PkArrayData 的契约消费，不许各写各的）：
//
// 1. **PkMut() 是唯一的写入口。** 下面每个非 const 方法都经它拿内层引用。
//    绕过去直接碰 m_d 就是 COW 漏洞——共享的两个容器会互相污染。唯一的例外是
//    swap()：它交换的是缓冲区指针本身，走 PkArrayData::PkSwap（零分配、
//    noexcept），Qt 的 QVector::swap 同样不 detach。
// 2. **PkConst() 绝不 detach。** const 方法、constBegin/constEnd/cbegin/cend
//    全走它。调用点有 167 处 constBegin + 190 处 constEnd 靠这条保持 O(1)。
// 3. **拷贝构造/赋值必须 O(1)**（只拷 shared_ptr）。调用点有 2286 处
//    Q_FOREACH/foreach 按值拷贝整个容器全指望这一条。
//
// size() 返回 **int** 不是 size_t：Qt5 的口径。调用点大量把它塞进 int 变量、
// 与 int 比较、做减法；返回 size_t 会让 `for (int i = 0; i < v.size(); ++i)`
// 报有符号/无符号比较警告，还会让 `v.size() - 1` 在空容器上回绕成天文数字
// ——回绕成 -1 正是要对齐的行为。
//
// 越界：at()/operator[]/first()/last() 在 Qt 里是 Q_ASSERT，release 下是 UB。
// 「Qt 是 UB」不等于「我们可以随便做」——它等于「对拍证明不了这一格」，所以
// 这些格子由 assert() 兜住（NDEBUG 下与 Q_ASSERT 一样消失）。
// ---------------------------------------------------------------------------

template <typename T, typename Derived>
class PkArrayContainer
{
public:
    using PkInner = std::vector<T>;

    using value_type = T;
    using iterator = typename PkInner::iterator;
    using const_iterator = typename PkInner::const_iterator;
    using reverse_iterator = typename PkInner::reverse_iterator;
    using const_reverse_iterator = typename PkInner::const_reverse_iterator;
    // Qt 的驼峰别名：调用点写 QVector<T>::Iterator 的地方靠它。
    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using reference = T &;
    using const_reference = const T &;
    using pointer = T *;
    using const_pointer = const T *;
    using difference_type = typename PkInner::difference_type;
    // Qt5 的 size_type 就是 int，不是 std::size_t。
    using size_type = int;

    // ---- 容量 ----

    int size() const noexcept { return static_cast<int>(m_d.PkConst().size()); }
    int count() const noexcept { return size(); }
    // count(const T&) 与无参 count() 是两个重载：数某个值出现了几次。
    int count(const T &t) const
    {
        const PkInner &v = m_d.PkConst();
        return static_cast<int>(std::count(v.begin(), v.end(), t));
    }

    bool isEmpty() const noexcept { return m_d.PkConst().empty(); }
    bool empty() const noexcept { return isEmpty(); }

    void reserve(int n)
    {
        m_d.PkMut().reserve(n > 0 ? static_cast<std::size_t>(n) : 0);
    }

    // ---- 元素访问 ----

    const T &at(int i) const
    {
        assert(i >= 0 && i < size());
        return m_d.PkConst()[static_cast<std::size_t>(i)];
    }

    T &operator[](int i)
    {
        assert(i >= 0 && i < size());
        return m_d.PkMut()[static_cast<std::size_t>(i)];
    }

    const T &operator[](int i) const { return at(i); }

    // value() 越界（含负数）返回 T() / def —— 这是 Qt 的行为，不是断言。
    T value(int i) const
    {
        const PkInner &v = m_d.PkConst();
        if (i < 0 || i >= static_cast<int>(v.size())) {
            return T();
        }
        return v[static_cast<std::size_t>(i)];
    }

    T value(int i, const T &defaultValue) const
    {
        const PkInner &v = m_d.PkConst();
        if (i < 0 || i >= static_cast<int>(v.size())) {
            return defaultValue;
        }
        return v[static_cast<std::size_t>(i)];
    }

    T &first()
    {
        assert(!isEmpty());
        return m_d.PkMut().front();
    }
    const T &first() const
    {
        assert(!isEmpty());
        return m_d.PkConst().front();
    }

    T &last()
    {
        assert(!isEmpty());
        return m_d.PkMut().back();
    }
    const T &last() const
    {
        assert(!isEmpty());
        return m_d.PkConst().back();
    }

    T &front() { return first(); }
    const T &front() const { return first(); }
    T &back() { return last(); }
    const T &back() const { return last(); }

    T *data() { return m_d.PkMut().data(); }
    const T *data() const { return m_d.PkConst().data(); }
    const T *constData() const { return m_d.PkConst().data(); }

    // ---- 增 ----
    //
    // 单元素的 push_back/insert 不需要「先取一份副本」：标准要求 vector 的
    // 这两个操作在实参是自身元素时也正确（值先拷出来再扩容）。而 PkMut() 若
    // 真的 detach 了，说明缓冲区是共享的 —— 老缓冲区被另一方持有，实参引用
    // 照样有效。两条合起来，v.append(v.at(0)) 是安全的。

    void append(const T &t) { m_d.PkMut().push_back(t); }
    void append(T &&t) { m_d.PkMut().push_back(std::move(t)); }

    void append(const Derived &other)
    {
        if (other.isEmpty()) {
            return;
        }
        if (this == &other) {
            // 自追加：insert 期间源区间会失效，先取一份快照。
            PkInner &v = m_d.PkMut();
            const PkInner snapshot(v);
            v.insert(v.end(), snapshot.begin(), snapshot.end());
            return;
        }
        // 先绑源再 PkMut()：若两者共享同一缓冲区，PkMut() 只会把**自己**换到
        // 新缓冲区，老的那份仍被 other 持有，src 不会悬垂。
        const PkInner &src = other.m_d.PkConst();
        PkInner &v = m_d.PkMut();
        v.insert(v.end(), src.begin(), src.end());
    }

    void push_back(const T &t) { append(t); }
    void push_back(T &&t) { append(std::move(t)); }

    void prepend(const T &t)
    {
        PkInner &v = m_d.PkMut();
        v.insert(v.begin(), t);
    }

    void push_front(const T &t) { prepend(t); }

    void insert(int i, const T &t)
    {
        assert(i >= 0 && i <= size());
        PkInner &v = m_d.PkMut();
        v.insert(v.begin() + i, t);
    }

    // 迭代器版：返回指向新插入元素的迭代器。
    // 偏移**必须按调用时的缓冲区算**——PkMut() 可能 detach，之后 before 就指向
    // 老缓冲区了，直接拿它去 insert 是 UB。Qt 的 QVector::insert 同样是先算
    // offset 再 realloc。
    iterator insert(const_iterator before, const T &t)
    {
        const PkInner &cv = m_d.PkConst();
        const difference_type offset = std::distance(cv.begin(), before);
        assert(offset >= 0 && offset <= static_cast<difference_type>(cv.size()));
        PkInner &v = m_d.PkMut();
        return v.insert(v.begin() + offset, t);
    }

    // ---- 删 ----

    void remove(int i)
    {
        assert(i >= 0 && i < size());
        PkInner &v = m_d.PkMut();
        v.erase(v.begin() + i);
    }

    void remove(int i, int n)
    {
        assert(i >= 0 && n >= 0 && i + n <= size());
        PkInner &v = m_d.PkMut();
        v.erase(v.begin() + i, v.begin() + i + n);
    }

    void clear() { m_d.PkMut().clear(); }

    iterator erase(const_iterator pos)
    {
        const PkInner &cv = m_d.PkConst();
        const difference_type offset = std::distance(cv.begin(), pos);
        assert(offset >= 0 && offset < static_cast<difference_type>(cv.size()));
        PkInner &v = m_d.PkMut();
        return v.erase(v.begin() + offset);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        const PkInner &cv = m_d.PkConst();
        const difference_type from = std::distance(cv.begin(), first);
        const difference_type to = std::distance(cv.begin(), last);
        assert(from >= 0 && to >= from && to <= static_cast<difference_type>(cv.size()));
        PkInner &v = m_d.PkMut();
        return v.erase(v.begin() + from, v.begin() + to);
    }

    // 交换缓冲区指针本身：零分配、noexcept，**不 detach**（Qt 的 swap 同样
    // 不 detach，共享关系跟着缓冲区走）。这是 PkMut() 唯一的例外，理由见类头。
    void swap(Derived &other) noexcept { m_d.PkSwap(other.m_d); }

    // ---- 查找 ----

    bool contains(const T &t) const { return indexOf(t) != -1; }

    // 逐条复刻 Qt5 的 from 折算规则：负数按 from + size 折算，折算后仍为负
    // 就从头找；超出末尾则直接找不到。
    int indexOf(const T &t, int from = 0) const
    {
        const PkInner &v = m_d.PkConst();
        const int n = static_cast<int>(v.size());
        if (from < 0) {
            from = from + n < 0 ? 0 : from + n;
        }
        for (int i = from; i < n; ++i) {
            if (v[static_cast<std::size_t>(i)] == t) {
                return i;
            }
        }
        return -1;
    }

    // Qt5 的 lastIndexOf：负 from 按 from + size 折算（折算后仍为负 → 找不到），
    // 超出末尾按最后一个元素算。
    int lastIndexOf(const T &t, int from = -1) const
    {
        const PkInner &v = m_d.PkConst();
        const int n = static_cast<int>(v.size());
        if (from < 0) {
            from += n;
        } else if (from >= n) {
            from = n - 1;
        }
        for (int i = from; i >= 0; --i) {
            if (v[static_cast<std::size_t>(i)] == t) {
                return i;
            }
        }
        return -1;
    }

    // ---- 迭代器 ----
    //
    // 非 const 的 begin()/end()/rbegin()/rend() 会 detach（Qt 同样如此：拿到
    // 可写迭代器就意味着可能写）；constBegin/constEnd/cbegin/cend 与 const
    // 重载绝不 detach。

    iterator begin() { return m_d.PkMut().begin(); }
    const_iterator begin() const noexcept { return m_d.PkConst().begin(); }
    iterator end() { return m_d.PkMut().end(); }
    const_iterator end() const noexcept { return m_d.PkConst().end(); }

    const_iterator cbegin() const noexcept { return m_d.PkConst().begin(); }
    const_iterator cend() const noexcept { return m_d.PkConst().end(); }
    const_iterator constBegin() const noexcept { return m_d.PkConst().begin(); }
    const_iterator constEnd() const noexcept { return m_d.PkConst().end(); }

    reverse_iterator rbegin() { return m_d.PkMut().rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return m_d.PkConst().rbegin(); }
    reverse_iterator rend() { return m_d.PkMut().rend(); }
    const_reverse_iterator rend() const noexcept { return m_d.PkConst().rend(); }

    // ---- 比较 ----

    bool operator==(const Derived &other) const
    {
        // 共享同一缓冲区时 O(1) 判真（Qt 的 `if (d == v.d) return true`）。
        return m_d.PkIsSharedWith(other.m_d) || m_d.PkConst() == other.m_d.PkConst();
    }
    bool operator!=(const Derived &other) const { return !(*this == other); }

    // ---- 追加运算符（返回**派生类**引用，才能链式写 v << 1 << 2）----

    Derived &operator+=(const T &t)
    {
        append(t);
        return pkSelf();
    }
    Derived &operator+=(const Derived &other)
    {
        append(other);
        return pkSelf();
    }
    Derived &operator<<(const T &t)
    {
        append(t);
        return pkSelf();
    }
    Derived &operator<<(const Derived &other)
    {
        append(other);
        return pkSelf();
    }

    // ---- 只给单测用，不进 compat 垫片 ----
    //
    // Qt 的 isDetached()/isSharedWith() 在 Krita 调用点实测都是 0 处，暴露它们
    // 等于凭空多实现两项。这两个 Pk 前缀的观测器不在改名表里，垫片映射不到，
    // 只有单测会调——而单测非有它们不可：「写方法漏用 PkMut()」是本任务最容易
    // 写错、也最难在别处发现的缺陷，只有直接看共享状态才查得出来。
    long PkUseCount() const noexcept { return m_d.PkUseCount(); }
    bool PkIsSharedWith(const Derived &other) const noexcept
    {
        return m_d.PkIsSharedWith(other.m_d);
    }

protected:
    // 只给派生类构造/析构：析构非虚且 protected，杜绝经基类指针 delete。
    PkArrayContainer() = default;
    explicit PkArrayContainer(PkInner init) : m_d(std::move(init)) {}
    ~PkArrayContainer() = default;

    // 五个特殊成员全部显式写出：一旦声明了析构，移动构造/移动赋值就不再隐式
    // 生成；而移动之后「源是空且完全可用的容器」这条 Qt 语义是 PkArrayData
    // 兜住的，退回拷贝会让它悄悄失效。拷贝同理——必须留着且是 O(1)。
    PkArrayContainer(const PkArrayContainer &) = default;
    PkArrayContainer &operator=(const PkArrayContainer &) = default;
    PkArrayContainer(PkArrayContainer &&) = default;
    PkArrayContainer &operator=(PkArrayContainer &&) = default;

    Derived &pkSelf() noexcept { return static_cast<Derived &>(*this); }
    const Derived &pkSelf() const noexcept { return static_cast<const Derived &>(*this); }

    PkArrayData<PkInner> m_d;
};
