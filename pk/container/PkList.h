#pragma once

// PkVector 与 PkList 互相要对方的完整类型（toList/toVector）。本文件开头把
// PkVector.h 拉进来，两个转换函数的定义统一放在**本文件末尾**——那时两个类
// 都已完整。包含顺序两种都成立，理由见 PkVector.h 末尾那段注释。
#include "PkVector.h"

#include "PkArrayContainer.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// PkList<T> —— Qt5 QList<T> 的替代品。COW，内层 std::vector<T>。
//
// 共同 API 全在 PkArrayContainer<T, PkList<T>> 里（PkMut() 是唯一写入口、
// PkConst() 绝不 detach、拷贝 O(1)）。**本文件只放 QList 专有的那批**：
// removeAt / removeAll / removeOne / removeFirst / removeLast / takeAt /
// takeFirst / takeLast / pop_back / pop_front / move / toVector。
//
// 对齐目标是 **Qt 5.15.7**：QList 与 QVector 在那一版是两个类型、两套方法集。
// Qt5 的 QList 内部在元素大到装不进一个指针时会退化成指针数组——那是 Qt 为
// QList<QVariant> 一类老包袱留的实现细节，公开语义用连续数组全都能给到，
// 不复刻。
// ---------------------------------------------------------------------------

template <typename T>
class PkList : public PkArrayContainer<T, PkList<T>>
{
    using PkBase = PkArrayContainer<T, PkList<T>>;
    using PkInner = typename PkBase::PkInner;

public:
    PkList() = default;
    PkList(std::initializer_list<T> args) : PkBase(PkInner(args)) {}

    // 理由与 PkVector 同：声明了移动构造就会把隐式拷贝 deleted 掉，
    // 而拷贝 O(1) 是 2286 处 Q_FOREACH 的命根子。
    ~PkList() = default;
    PkList(const PkList &) = default;
    PkList &operator=(const PkList &) = default;
    PkList(PkList &&) = default;
    PkList &operator=(PkList &&) = default;

    // ---- QList 专有 ----
    //
    // **QList 没有 remove(int)**（Qt5 里只有 QVector 有），所以本类不公开它，
    // 按下标删一律走基类的 protected 原语 pkRemoveAt。

    void removeAt(int i) { this->pkRemoveAt(i); }

    // Qt5：返回删掉了几个。
    //
    // **删不到东西时不得 detach，也不得拷任何元素**——真 Qt 5.15.7 探针实测
    // （带元素拷贝计数器）：
    //
    //   QList removeAll(不命中)  返回=0  元素拷贝=0  isDetached=0
    //   QList removeAll(命中)    返回=1  元素拷贝=3  isDetached=1
    //   QList removeOne(不命中)  返回=0  元素拷贝=0  isDetached=0
    //   QList removeOne(命中)    返回=1  元素拷贝=2  isDetached=1
    //
    // 所以先走 **const 路径**确认存在性，不存在就直接返回 0、根本不碰 PkMut()。
    // 无脑先 PkMut() 会让一个共享的容器凭空分裂一次并深拷全部元素，是与 Qt 的
    // 真实偏离（按「默认全对齐」即缺陷）。代价是命中时多一次 O(n) 的 const 扫描
    // ——Qt 自己也是先找后删（`indexOf` → 早退 → `const T t = _t` → `detach()`），
    // 这个常数因子可以接受。
    //
    // **副本必须在早退之后、PkMut() 之前取**，两侧都是硬要求：
    //  - 早退之前取 → 不命中时白拷一个元素，「元素拷贝=0」那格对不上（实测咬到过）
    //  - PkMut() 之后取 → t 可能指向本容器的某个元素（l.removeAll(l.at(0))），
    //    detach 换缓冲区、std::remove 搬元素，原引用就悬垂了
    int removeAll(const T &t)
    {
        const PkInner &cv = this->m_d.PkConst();
        if (std::find(cv.begin(), cv.end(), t) == cv.end()) {
            return 0;
        }

        const T copy(t);
        PkInner &v = this->m_d.PkMut();
        const std::size_t before = v.size();
        v.erase(std::remove(v.begin(), v.end(), copy), v.end());
        return static_cast<int>(before - v.size());
    }

    // Qt5：只删第一个，返回删没删掉。
    // 同上表：不命中不 detach、不拷元素 —— indexOf 是 const 路径，失败直接返回。
    //
    // 这里**不需要**取 t 的副本（removeAll 那边需要）：pkRemoveAt 按**下标**删，
    // 全程不再读 t，所以 t 指向本容器元素时也不存在悬垂。Qt 的 removeOne 同样
    // 是「indexOf → removeAt(index)」、不取副本。多取一个副本会让不命中那格的
    // 「元素拷贝=0」对不上。
    bool removeOne(const T &t)
    {
        const int i = this->indexOf(t);
        if (i < 0) {
            return false;
        }
        this->pkRemoveAt(i);
        return true;
    }

    void removeFirst()
    {
        assert(!this->isEmpty());
        this->pkRemoveAt(0);
    }

    void removeLast()
    {
        assert(!this->isEmpty());
        this->pkRemoveAt(this->size() - 1);
    }

    // 取走并返回。空容器上 Qt 是 Q_ASSERT + release 下 UB，这里由断言兜住。
    T takeAt(int i)
    {
        assert(i >= 0 && i < this->size());
        PkInner &v = this->m_d.PkMut();
        T taken = std::move(v[static_cast<std::size_t>(i)]);
        v.erase(v.begin() + i);
        return taken;
    }

    T takeFirst() { return takeAt(0); }
    T takeLast() { return takeAt(this->size() - 1); }

    void pop_back() { removeLast(); }
    void pop_front() { removeFirst(); }

    // QList::move(from, to)：把 from 处的元素搬到下标 to，其余元素顺次让位
    // ——等价于「先摘出来，再插到 to」。
    void move(int from, int to)
    {
        assert(from >= 0 && from < this->size());
        assert(to >= 0 && to < this->size());
        // 先取内层引用：from == to 也要经 PkMut()，非 const 方法一律不留例外。
        PkInner &v = this->m_d.PkMut();
        if (from == to) {
            return;
        }
        T moved = std::move(v[static_cast<std::size_t>(from)]);
        v.erase(v.begin() + from);
        v.insert(v.begin() + to, std::move(moved));
    }

    // 定义在本文件末尾——那里 PkVector<T> 已经是完整类型。
    PkVector<T> toVector() const;
};

// ---------------------------------------------------------------------------
// 两个转换函数：定义在这里，因为只有到了这一行 PkVector<T> 与 PkList<T> 才都
// 完整。逐元素追加（Qt5 的 QVector::toList/QList::toVector 就是这么实现的），
// 结果是一份**独立**的容器——两边内层类型虽然都是 std::vector<T>，但共享缓冲区
// 会让 PkVector 与 PkList 互相污染，那不是 Qt 的语义。
// ---------------------------------------------------------------------------

template <typename T>
PkList<T> PkVector<T>::toList() const
{
    PkList<T> result;
    result.reserve(this->size());
    for (auto it = this->constBegin(); it != this->constEnd(); ++it) {
        result.append(*it);
    }
    return result;
}

template <typename T>
PkVector<T> PkList<T>::toVector() const
{
    PkVector<T> result;
    result.reserve(this->size());
    for (auto it = this->constBegin(); it != this->constEnd(); ++it) {
        result.append(*it);
    }
    return result;
}
