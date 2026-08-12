#pragma once

#include "PkArrayData.h"
#include "PkHashFunctions.h"
#include "PkList.h"

#include <cstddef>
#include <initializer_list>
#include <unordered_set>
#include <utility>

// ---------------------------------------------------------------------------
// PkSet<T> —— Qt5 QSet<T> 的替代品。COW，内层 std::unordered_set<T, PkHasher<T>>。
//
// **不走 PkAssocContainer**：那是 key→value 映射的共同实现（value()/key()/
// operator[]/insert(k,v)/迭代器解引用得 value），QSet 一项都用不上。QSet 的
// 元素就是 key 本身。
//
// **迭代器直接用内层的 const_iterator**，iterator 与 const_iterator 是同一个
// 类型——这与 Qt 一致：QSet 的元素是哈希表的 key，改了就会破坏哈希不变量，
// 所以 QSet<T>::iterator 的 `*it` 给的也是 **const T&**。这里不需要像
// PkMap/PkHash 那样包一层（那一层是为了"解引用得 value 而不是 pair"，
// QSet 没有 pair 可言）。
//
// **无序**：迭代顺序在 Qt 里本就未定义，单测只断言集合相等（排序后比较）。
//
// 哈希靠 qHash 自由函数 + ADL，机制见 PkHashFunctions.h；T 还要求
// operator==（std::equal_to<T>）——QSet 同样要求。
//
// 方法面严格等于实测用量表：insert / contains / remove / values / toList /
// unite / intersect / subtract / count / size / isEmpty / clear /
// begin / end / constBegin / constEnd / operator== / operator!=。
// **toSet / fromSet / isDetached / isSharedWith 实测 0 调用点，不做。**
//
// 三条硬要求（与 PkArrayData 的契约一致）：PkMut() 是唯一写入口 ·
// PkConst() 绝不 detach · 拷贝 O(1)。size() 返回 int 是 Qt5 口径。
// ---------------------------------------------------------------------------

template <typename T>
class PkSet
{
public:
    using PkInner = std::unordered_set<T, PkHasher<T>>;

    using value_type = T;
    using key_type = T;
    // 元素不可写 → 两套迭代器是同一个类型（QSet 同样如此）。
    using iterator = typename PkInner::const_iterator;
    using const_iterator = typename PkInner::const_iterator;
    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using size_type = int;

    PkSet() = default;

    PkSet(std::initializer_list<T> args)
    {
        PkInner &s = m_d.PkMut();
        for (const auto &v : args) {
            s.insert(v);
        }
    }

    // 五个特殊成员全部显式 = default：声明了移动构造会把隐式拷贝 deleted 掉，
    // 而拷贝 O(1) 是 2286 处 Q_FOREACH 的命根子。
    ~PkSet() = default;
    PkSet(const PkSet &) = default;
    PkSet &operator=(const PkSet &) = default;
    PkSet(PkSet &&) = default;
    PkSet &operator=(PkSet &&) = default;

    // ---- 容量 ----

    int size() const noexcept { return static_cast<int>(m_d.PkConst().size()); }
    int count() const noexcept { return size(); }
    bool isEmpty() const noexcept { return m_d.PkConst().empty(); }

    // ---- 读 ----

    bool contains(const T &value) const
    {
        const PkInner &s = m_d.PkConst();
        return s.find(value) != s.end();
    }

    PkList<T> values() const
    {
        PkList<T> result;
        result.reserve(size());
        for (const auto &v : m_d.PkConst()) {
            result.append(v);
        }
        return result;
    }

    // Qt5 里 QSet::toList() 与 values() 结果相同（toList 是老名字）。
    PkList<T> toList() const { return values(); }

    // ---- 写 ----

    // 去重：已经在集合里就什么都不加（实测 Qt：`QSet 去重: insert(1) 两次后 size=1`）。
    // 返回指向该元素的迭代器（Qt 语义）。
    iterator insert(const T &value)
    {
        PkInner &s = m_d.PkMut();
        return s.insert(value).first;
    }

    // Qt5 的 QSet::remove 返回**删没删掉**（bool），不是删了几个
    // ——与 QMap/QHash::remove 返回 int 不同，别抄错。
    bool remove(const T &value) { return m_d.PkMut().erase(value) > 0; }

    void clear() { m_d.PkMut().clear(); }

    // 并（就地）：把 other 的元素全并进来。
    //
    // `operator|=` 是它的运算符写法，**4 处真实调用点**：
    //   libs/image/brushengine/kis_paintop_lod_limitations.h:29  limitations |= rhs.limitations;
    //   libs/image/brushengine/kis_paintop_lod_limitations.h:30  blockers    |= rhs.blockers;
    //   libs/image/kis_layer_utils.cpp:341   frames |= fetchLayerFramesRecursive(node);
    //   libs/image/kis_layer_utils.cpp:1418  frames |= fetchLayerFramesRecursive(node);
    // 前两处的元素类型是 QSet<KoID>，而 KoID 的 qHash 正是那 18 处自定义重载
    // 之一（kis_paintop_lod_limitations.h:14 `inline uint qHash(const KoID &id)`）
    // ——PkHashFunctions.h 的 ADL 链路就是为这类调用点存在的。
    //
    // **`operator&` / `&=` / `-` / `-=` / `+` / `+=` 一律不做**：核实下来 0 处
    // 调用点（`&` 的全部命中是 Qt::MouseButtons / KeyboardModifiers 位掩码，
    // `-` 的全部命中是算术减法）。给了就是凭空多六项。
    PkSet &unite(const PkSet &other)
    {
        if (this == &other || other.isEmpty()) {
            return *this;
        }
        // 先绑源再 PkMut()：共享同一缓冲区时，PkMut() 只把**自己**换到新缓冲区，
        // 老的那份仍被 other 持有，src 不会悬垂。
        const PkInner &src = other.m_d.PkConst();
        PkInner &s = m_d.PkMut();
        s.insert(src.begin(), src.end());
        return *this;
    }

    // 交：只留下也在 other 里的元素。
    PkSet &intersect(const PkSet &other)
    {
        if (this == &other) {
            return *this;
        }
        const PkInner &src = other.m_d.PkConst();
        PkInner &s = m_d.PkMut();
        for (auto it = s.begin(); it != s.end();) {
            if (src.find(*it) == src.end()) {
                it = s.erase(it);
            } else {
                ++it;
            }
        }
        return *this;
    }

    // 差：去掉出现在 other 里的元素。自减法的结果是空集（Qt 同样）。
    PkSet &subtract(const PkSet &other)
    {
        if (this == &other) {
            m_d.PkMut().clear();
            return *this;
        }
        const PkInner &src = other.m_d.PkConst();
        PkInner &s = m_d.PkMut();
        for (const auto &v : src) {
            s.erase(v);
        }
        return *this;
    }

    // ---- 集合运算符 ----

    PkSet &operator|=(const PkSet &other) { return unite(other); }

    // ---- 迭代器 ----
    //
    // QSet 的元素不可写，两套迭代器同类型，因此 begin()/end() 无论 const 与否
    // 都走 PkConst()、**都不 detach**。这与 PkMap/PkHash 的非 const begin()
    // 走 PkMut() 不同 —— 那条规则的前提是"拿到的是**可写**迭代器"，QSet 拿不到。

    const_iterator begin() const noexcept { return m_d.PkConst().begin(); }
    const_iterator end() const noexcept { return m_d.PkConst().end(); }
    const_iterator constBegin() const noexcept { return m_d.PkConst().begin(); }
    const_iterator constEnd() const noexcept { return m_d.PkConst().end(); }

    // ---- 比较 ----

    bool operator==(const PkSet &other) const
    {
        return m_d.PkIsSharedWith(other.m_d) || m_d.PkConst() == other.m_d.PkConst();
    }
    bool operator!=(const PkSet &other) const { return !(*this == other); }

    // ---- 只给单测用，不进 compat 垫片 ----
    long PkUseCount() const noexcept { return m_d.PkUseCount(); }
    bool PkIsSharedWith(const PkSet &other) const noexcept
    {
        return m_d.PkIsSharedWith(other.m_d);
    }

private:
    PkArrayData<PkInner> m_d;
};

// 集合并（非就地）。**1 处真实调用点**：
//   libs/image/kis_layer_utils.cpp:150
//     frames = fetchLayerFramesRecursive(prevLayer) | fetchLayerFramesRecursive(currLayer);
// 两侧都是右值，所以形参收 const 引用、结果按值返回（Qt 的 operator| 同样是
// 自由函数、同样按值返回）。
template <typename T>
PkSet<T> operator|(const PkSet<T> &lhs, const PkSet<T> &rhs)
{
    PkSet<T> result(lhs);
    result.unite(rhs);
    return result;
}
