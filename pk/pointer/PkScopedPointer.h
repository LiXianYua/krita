#ifndef PK_SCOPED_POINTER_H
#define PK_SCOPED_POINTER_H

// PkScopedPointer<T> / PkScopedArrayPointer<T> —— QScopedPointer<T>/
// QScopedArrayPointer<T> 的替代品。判据 C（探针 P10，真 Qt 5.15.13）：
// QScopedPointer 的拷贝构造/移动构造/拷贝赋值/移动赋值四项全部不可用
// （对照 std::unique_ptr 的 move 两项可用）——这不是"限制"，是对齐：Krita 里
// 不可能存在依赖移动的代码，因为 Qt 的就不让移。

#include <cstddef>

template <class T>
class PkScopedPointer
{
public:
    explicit PkScopedPointer(T *p = nullptr) noexcept : m_p(p) {}
    ~PkScopedPointer() { delete m_p; }

    // 判据 C：拷贝与移动全部不可用，与 QScopedPointer 一致。
    PkScopedPointer(const PkScopedPointer &) = delete;
    PkScopedPointer &operator=(const PkScopedPointer &) = delete;
    PkScopedPointer(PkScopedPointer &&) = delete;
    PkScopedPointer &operator=(PkScopedPointer &&) = delete;

    T *data() const noexcept { return m_p; }
    T *get() const noexcept { return m_p; }
    bool isNull() const noexcept { return m_p == nullptr; }
    T &operator*() const { return *m_p; }
    T *operator->() const noexcept { return m_p; }
    bool operator!() const noexcept { return m_p == nullptr; }

    typedef T *PkScopedPointer::*RestrictedBool;
    operator RestrictedBool() const noexcept { return m_p ? &PkScopedPointer::m_p : nullptr; }

    // 自赋值保护：Qt 的 QScopedPointer::reset 有同样的 `if (d == other) return;`
    // （qscopedpointer.h）。少了它，`p.reset(p.data())` 会先 delete 再把 m_p
    // 挂回一个刚被释放的野指针。
    void reset(T *other = nullptr)
    {
        if (m_p == other) return;
        T *old = m_p;
        m_p = other;
        delete old;
    }

    T *take() noexcept { T *r = m_p; m_p = nullptr; return r; }

private:
    T *m_p;
};

// 与 Qt 的 qscopedpointer.h 一致：两个 PkScopedPointer 比、与 nullptr 比
// （两个方向都要，Qt 两个方向都提供）。
template <class T>
bool operator==(const PkScopedPointer<T> &a, const PkScopedPointer<T> &b) noexcept
{ return a.data() == b.data(); }
template <class T>
bool operator!=(const PkScopedPointer<T> &a, const PkScopedPointer<T> &b) noexcept
{ return a.data() != b.data(); }
template <class T>
bool operator==(const PkScopedPointer<T> &a, std::nullptr_t) noexcept
{ return a.isNull(); }
template <class T>
bool operator==(std::nullptr_t, const PkScopedPointer<T> &b) noexcept
{ return b.isNull(); }
template <class T>
bool operator!=(const PkScopedPointer<T> &a, std::nullptr_t) noexcept
{ return !a.isNull(); }
template <class T>
bool operator!=(std::nullptr_t, const PkScopedPointer<T> &b) noexcept
{ return !b.isNull(); }

// PkScopedArrayPointer<T> —— 独立类型，**不**继承 PkScopedPointer。
// 按 API 面实测用量表收窄：只给 data()/operator[]/reset() 三个成员，不给
// isNull/take/swap/operator*/operator->（真 Qt 里 QScopedArrayPointer 公开
// 继承自 QScopedPointer 会连带暴露这些，但 Krita 全仓对数组指针的调用点
// 用不到它们）。
template <class T>
class PkScopedArrayPointer
{
public:
    explicit PkScopedArrayPointer(T *p = nullptr) noexcept : m_p(p) {}
    ~PkScopedArrayPointer() { delete[] m_p; }

    // 判据 C 同样适用于数组版：探针 P10 只测了 QScopedPointer 本身，但
    // QScopedArrayPointer 继承自它、Q_DISABLE_COPY 是同一条，两者禁拷贝/
    // 禁移动的结论相同（shape_asserts.cpp 的 static_assert 钉住这一点）。
    PkScopedArrayPointer(const PkScopedArrayPointer &) = delete;
    PkScopedArrayPointer &operator=(const PkScopedArrayPointer &) = delete;
    PkScopedArrayPointer(PkScopedArrayPointer &&) = delete;
    PkScopedArrayPointer &operator=(PkScopedArrayPointer &&) = delete;

    T *data() const noexcept { return m_p; }
    T &operator[](int i) const { return m_p[i]; }

    void reset(T *other = nullptr)
    {
        if (m_p == other) return;
        T *old = m_p;
        m_p = other;
        delete[] old;
    }

private:
    T *m_p;
};

#endif // PK_SCOPED_POINTER_H
