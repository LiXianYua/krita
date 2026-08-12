#pragma once

#include "PkVector.h"

#include <cassert>
#include <initializer_list>

// ---------------------------------------------------------------------------
// PkStack<T> —— Qt5 QStack<T> 的替代品。SRC 出现 48 次。
//
// **是薄派生类，不是 typedef** ——照 Qt（`class QStack : public QVector<T>`）。
// 理由与 PkStringList 同：push/pop/top 在调用点是**成员调用**，typedef 会让
// 每一个 PkVector<T> 都长出 push/pop/top。
//
// COW 全部由 PkVector → PkArrayContainer → PkArrayData 那条链兜住，本类只加
// 三个方法。**但新增的三个方法自己也必须守住 PkMut() 是唯一写入口**：
// push 与 pop 都要经它，漏一个就是一个 COW 漏洞（共享的两个栈互相污染）。
// 这里的做法是**一律转调基类已经验证过的方法**（append / last / pkRemoveAt
// 经由 removeLast 的等价物），不自己碰 m_d——转调是最不容易漏的写法。
//
// ---- 链式操作符必须重新声明（协变返回类型的坑）----
//
// 基类的 operator<< 返回 `PkVector<T>&`（CRTP 的 Derived 是 PkVector<T>，
// 不是 PkStack<T>）。不重新声明的话：
//     PkStack<int> s;
//     (s << 1 << 2).top();   // ← 编不过：第一个 << 返回的已经是 PkVector<int>&
// Qt 的 QStack 同样为此重新声明。单测里有一条直接压「链式之后还能调 top()」
// ——编得过就是证明。
//
// ---- 空栈上 pop()/top() ----
//
// Qt 里是 Q_ASSERT（release 下是 UB）。本类同样实现成 assert，与 Task 2 的
// first()/last() 一个口径。**对拍证明不了这一格**：release 下 Qt 是 UB，
// 没有"正确行为"可对；debug 下 Qt 会 abort，我们也 abort，但那是两个进程各自
// 崩掉，不是可比对的输出。
// ---------------------------------------------------------------------------

template <typename T>
class PkStack : public PkVector<T>
{
    using PkBase = PkVector<T>;

public:
    PkStack() = default;

    // **initializer_list 构造是 Qt 没有的**（Qt5 的 QStack 只有编译器生成的
    // 那几个特殊成员，不继承 QVector 的构造，`QStack<int> s{1,2,3}` 在 Qt 下
    // 编不过）。这里加它是为了让 PkStack 能直接复用 tests/PkSeqTestShared.h
    // 那一整套序列共享测试——那份测试通篇是 `Seq<int> a{1, 2, 3}`。
    //
    // **只放宽不收紧**：多一个构造只会让原本编不过的写法编得过，任何现有
    // 调用点的行为都不受影响，不构成与 Qt 的语义偏离。
    //
    // 刻意**不**加 `PkStack(const PkVector<T> &)`：Qt 没有这条转换，加了就是
    // 凭空多一项。基类 → 派生类的隐式转换只有 PkStringList 需要（实测
    // 「QList<QString> → QStringList 可隐式转换」，那是 Qt 真有的）。
    PkStack(std::initializer_list<T> args) : PkBase(args) {}

    ~PkStack() = default;
    PkStack(const PkStack &) = default;
    PkStack &operator=(const PkStack &) = default;
    PkStack(PkStack &&) = default;
    PkStack &operator=(PkStack &&) = default;

    // ---- QStack 专有：LIFO ----
    //
    // 实测（真 Qt 5.15.7）：
    //   push 1,2,3 → size=3 top=3；pop()→3（剩 2）；pop()→2（剩 1）  = LIFO

    void push(const T &t) { this->append(t); }
    void push(T &&t) { this->append(std::move(t)); }

    // Qt 的 QStack::pop() **按值返回**被弹出的元素（不是 void）。
    T pop()
    {
        assert(!this->isEmpty());
        // 先把值拷出来再删：删完再读就是读已释放的槽位。
        T t = this->last();
        this->remove(this->size() - 1);
        return t;
    }

    // const 与非 const 两个版本（Qt 都有）。非 const 版经 last() → PkMut()，
    // 因为它返回可写引用，调用方可能就着它改内容。
    T &top()
    {
        assert(!this->isEmpty());
        return this->last();
    }

    const T &top() const
    {
        assert(!this->isEmpty());
        return this->last();
    }

    // ---- 链式操作符：返回 PkStack&，不让类型退化成 PkVector& ----

    PkStack &operator<<(const T &t)
    {
        this->append(t);
        return *this;
    }

    PkStack &operator<<(const PkVector<T> &other)
    {
        this->append(other);
        return *this;
    }

    PkStack &operator+=(const T &t)
    {
        this->append(t);
        return *this;
    }

    PkStack &operator+=(const PkVector<T> &other)
    {
        this->append(other);
        return *this;
    }
};
