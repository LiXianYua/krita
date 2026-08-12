#pragma once

#include "PkList.h"

#include <cassert>
#include <initializer_list>

// ---------------------------------------------------------------------------
// PkQueue<T> —— Qt5 QQueue<T> 的替代品。SRC 出现 36 次。
//
// **是薄派生类，不是 typedef** ——照 Qt（`class QQueue : public QList<T>`）。
// 机制、COW 归属、链式操作符的坑、空队列上的断言，全部与 PkStack.h 同因，
// 那里写了完整理由，这里不重复。两处唯一的区别是基类与三个方法名：
//   PkStack : PkVector —— push / pop / top      LIFO
//   PkQueue : PkList   —— enqueue / dequeue / head   FIFO
//
// 实测（真 Qt 5.15.7）：
//   enqueue 1,2,3 → size=3 head=1；dequeue()→1（剩 2）；dequeue()→2（剩 1）  = FIFO
// ---------------------------------------------------------------------------

template <typename T>
class PkQueue : public PkList<T>
{
    using PkBase = PkList<T>;

public:
    PkQueue() = default;

    // initializer_list 构造是 Qt 没有的，加它是为了复用 PkSeqTestShared.h；
    // 同样刻意不加 `PkQueue(const PkList<T> &)`。完整理由见 PkStack.h 同一处。
    PkQueue(std::initializer_list<T> args) : PkBase(args) {}

    ~PkQueue() = default;
    PkQueue(const PkQueue &) = default;
    PkQueue &operator=(const PkQueue &) = default;
    PkQueue(PkQueue &&) = default;
    PkQueue &operator=(PkQueue &&) = default;

    // ---- QQueue 专有：FIFO ----

    void enqueue(const T &t) { this->append(t); }
    void enqueue(T &&t) { this->append(std::move(t)); }

    // Qt 的 QQueue::dequeue() 按值返回队头元素（不是 void）。
    T dequeue()
    {
        assert(!this->isEmpty());
        // 先把值拷出来再删：删完再读就是读已释放的槽位。
        T t = this->first();
        this->removeFirst();
        return t;
    }

    T &head()
    {
        assert(!this->isEmpty());
        return this->first();
    }

    const T &head() const
    {
        assert(!this->isEmpty());
        return this->first();
    }

    // ---- 链式操作符：返回 PkQueue&，不让类型退化成 PkList& ----

    PkQueue &operator<<(const T &t)
    {
        this->append(t);
        return *this;
    }

    PkQueue &operator<<(const PkList<T> &other)
    {
        this->append(other);
        return *this;
    }

    PkQueue &operator+=(const T &t)
    {
        this->append(t);
        return *this;
    }

    PkQueue &operator+=(const PkList<T> &other)
    {
        this->append(other);
        return *this;
    }
};
