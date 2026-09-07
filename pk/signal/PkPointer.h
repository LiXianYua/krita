#pragma once
#include <atomic>
#include <cstddef>
#include <memory>

// PkPointer<T>：QPointer<T> 的替代（弱引用），对象析构后 isNull()==true、data()==nullptr。
// compat 垫片 `#define QPointer PkPointer`（Task 4 建 compat 时做）让 Krita 调用点零改写。
// 机制：持有 T* 裸指针 + PkObject 的 aliveFlag 弱视图（weak_ptr<atomic<bool>>）。
// T* 在对象析构后不再解引用——data()/isNull() 只看 aliveFlag；operator-> 在
// isNull() 时调用是 UB（与 Qt 的 QPointer::operator-> 一致，Qt 文档不保证悬垂后
// 解引用安全）。
template <typename T>
class PkPointer
{
public:
    PkPointer() = default;
    PkPointer(T* p) { reset(p); }
    PkPointer(const PkPointer& o) = default;
    PkPointer& operator=(const PkPointer& o) = default;

    bool isNull() const
    {
        if (!m_ptr) {
            return true;
        }
        if (!m_tracksLifetime) {
            return false;
        }
        auto f = m_alive.lock();
        return !f || !f->load();
    }

    T* data() const { return isNull() ? nullptr : m_ptr; }

    T* operator->() const { return data(); }
    T& operator*() const { return *data(); }

    explicit operator bool() const { return !isNull(); }
    operator T*() const { return data(); }

    void clear()
    {
        m_ptr = nullptr;
        m_alive.reset();
        m_tracksLifetime = false;
    }
    void reset(T* p)
    {
        m_ptr = p;
        m_alive.reset();
        m_tracksLifetime = false;
        if (p) {
            // PkObject::aliveFlag() 返回 shared_ptr<atomic<bool>>，转 weak 观察。
            // SFINAE 降级：过渡期部分 QObject 系类型（KoShapeManager/KoCanvasBase/
            // KoDocumentResourceManager）还不是 PkObject 派生，没有 aliveFlag()——
            // 此时弱视图为空，isNull() 退化为纯空指针判断（无析构防护）。这些类型
            // 迁到 PkObject 后防护自动生效，调用点零改写。
            m_alive = hasAliveFlag(p, &m_tracksLifetime, 0);
        }
    }

    bool operator==(const PkPointer& o) const { return data() == o.data(); }
    bool operator==(T* p) const { return data() == p; }
    bool operator==(std::nullptr_t) const { return isNull(); }

    // C++17 不从 == 推导 !=，显式补齐三个对应形式。
    bool operator!=(const PkPointer& o) const { return !(*this == o); }
    bool operator!=(T* p) const { return !(*this == p); }
    bool operator!=(std::nullptr_t) const { return !isNull(); }

private:
    template <typename U>
    static auto hasAliveFlag(U* p, bool *tracksLifetime, int) -> decltype(p->aliveFlag())
    {
        *tracksLifetime = true;
        return p->aliveFlag();
    }
    template <typename U>
    static std::shared_ptr<std::atomic<bool>> hasAliveFlag(U*, bool *tracksLifetime, long)
    {
        *tracksLifetime = false;
        return nullptr;
    }

    T* m_ptr = nullptr;
    std::weak_ptr<std::atomic<bool>> m_alive;
    bool m_tracksLifetime = false;
};
