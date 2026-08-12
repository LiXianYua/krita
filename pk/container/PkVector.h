#pragma once

#include "PkArrayContainer.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// PkVector<T> —— Qt5 QVector<T> 的替代品。COW，内层 std::vector<T>。
//
// 共同 API（size/at/append/insert/erase/迭代器/比较/…）全在
// PkArrayContainer<T, PkVector<T>> 里，见那个文件的类头注释：PkMut() 是唯一
// 写入口、PkConst() 绝不 detach、拷贝 O(1)。**本文件只放 QVector 专有的四项**：
// resize / fill / capacity / toList。
//
// 全局 Pk 前缀、不进 C++ namespace：compat 垫片靠 `#define QVector PkVector`，
// 而 Krita 里有 `class QVector;` 一类前置声明——套进 namespace 这个技巧就废了。
//
// 对齐目标是 **Qt 5.15.7**，不是 Qt6：Qt6 的 QList 与 QVector 已经合并，本仓库
// 不是那个语义，QVector 与 QList 在这里是两个方法集不同的类型。
// ---------------------------------------------------------------------------

template <typename T>
class PkList;

template <typename T>
class PkVector : public PkArrayContainer<T, PkVector<T>>
{
    using PkBase = PkArrayContainer<T, PkVector<T>>;
    using PkInner = typename PkBase::PkInner;

public:
    PkVector() = default;

    // QVector<T>(int size)：size 个默认构造的元素。explicit —— 否则
    // `PkVector<int> v = 5;` 会编过，那不是 Qt 的行为。
    explicit PkVector(int size)
    {
        assert(size >= 0);
        this->m_d.PkMut().resize(static_cast<std::size_t>(size < 0 ? 0 : size));
    }

    PkVector(int size, const T &t)
    {
        assert(size >= 0);
        this->m_d.PkMut().assign(static_cast<std::size_t>(size < 0 ? 0 : size), t);
    }

    PkVector(std::initializer_list<T> args) : PkBase(PkInner(args)) {}

    // 五个特殊成员全部显式 = default：一旦声明了移动构造，隐式的拷贝构造与
    // 拷贝赋值就会被定义为 deleted（[class.copy.ctor]/8），而「拷贝 O(1)」是
    // 2286 处 Q_FOREACH 的命根子。移动本身直接 = default 就够——「移动之后源是
    // 空且完全可用的容器」这条 Qt 语义由 PkArrayData 兜住，容器层不必再兜一遍。
    ~PkVector() = default;
    PkVector(const PkVector &) = default;
    PkVector &operator=(const PkVector &) = default;
    PkVector(PkVector &&) = default;
    PkVector &operator=(PkVector &&) = default;

    // ---- QVector 专有 ----

    // remove(int) / remove(int, int) 只有 QVector 有，QList 没有（Qt5 里
    // `QList::remove(...)` 根本编不过，所以调用点里不可能存在）。实现是共同
    // 基类的 pkRemoveAt / pkRemoveRange，这里只做公开。
    void remove(int i) { this->pkRemoveAt(i); }
    void remove(int i, int n) { this->pkRemoveRange(i, n); }

    void resize(int size)
    {
        assert(size >= 0);
        this->m_d.PkMut().resize(static_cast<std::size_t>(size < 0 ? 0 : size));
    }

    int capacity() const noexcept
    {
        return static_cast<int>(this->m_d.PkConst().capacity());
    }

    // fill(t, size)：size >= 0 时先 resize 再把**全部**元素改写成 t；
    // size 省略（-1）时只改写，不改大小。返回自身引用（Qt 的签名）。
    //
    // `const T copy(t)` 不是多余的：t 可能就指向本容器的某个元素
    // （v.fill(v.at(0))），边填边读会读到已经被覆盖的值。Qt 同样先取副本。
    PkVector &fill(const T &t, int size = -1)
    {
        const T copy(t);
        PkInner &v = this->m_d.PkMut();
        if (size >= 0) {
            v.resize(static_cast<std::size_t>(size));
        }
        std::fill(v.begin(), v.end(), copy);
        return *this;
    }

    // 定义在 PkList.h 末尾——那里 PkList<T> 才是完整类型。
    PkList<T> toList() const;
};

// PkVector 与 PkList 互相要对方的完整类型（toList/toVector），谁都不能只前置
// 声明了事。解法：本文件末尾把 PkList.h 拉进来，两个转换函数的定义统一放在
// PkList.h 的末尾（那时两个类都已完整）。两种包含顺序都成立：
//   先 include PkVector.h → 这一行把 PkList.h 拉进来，PkVector 此刻已完整；
//   先 include PkList.h   → 它开头 include 本文件，本文件这一行是空转，回到
//                           PkList.h 继续定义 PkList 与两个转换函数。
// 位置必须是**文件末尾**（PkVector 定义之后），提到开头就会变成循环。
#include "PkList.h"
