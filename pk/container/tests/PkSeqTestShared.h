#pragma once

// ---------------------------------------------------------------------------
// PkVector<T> 与 PkList<T> 的**共同**单测。
//
// 两个容器共用一份实现（PkArrayContainer<T, Derived>），测试也共用一份：
// 每个用例都是 `template <template <typename> class Seq>`，在
// tests/test_pkvector.cpp 里用 PkVector 实例化、在 tests/test_pklist.cpp 里用
// PkList 实例化。抄两份的话，改一条断言就得记得改两个地方，漏改的那一半会
// 静默地一直绿着。
//
// 断言宏在这些模板里照常用：PK_VERIFY/PK_COMPARE 失败时 `return` 的是**本模板
// 函数**而不是测试函数，但失败已经通过 PkTestCase::checkResult() 记下了，
// 整个测试函数照样判失败。
// ---------------------------------------------------------------------------

#include "PkTest.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

// 带拷贝计数器的元素类型：证明"拷贝容器 = O(1)"不是靠 PkUseCount 自我印证的，
// 而是真的一个元素都没拷。移动显式 noexcept，否则 vector 扩容会退化成逐元素
// 拷贝，计数器就分不清"detach 拷的"与"扩容拷的"（与 test_arraydata.cpp 的
// Counted 同因）。inline static（C++17）让它可以直接住在头里。
struct PkSeqCounted
{
    int v = 0;

    PkSeqCounted() = default;
    explicit PkSeqCounted(int x) : v(x) {}
    PkSeqCounted(const PkSeqCounted &o) : v(o.v) { ++s_copies; }
    PkSeqCounted &operator=(const PkSeqCounted &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    PkSeqCounted(PkSeqCounted &&) noexcept = default;
    PkSeqCounted &operator=(PkSeqCounted &&) noexcept = default;
    ~PkSeqCounted() = default;

    bool operator==(const PkSeqCounted &o) const { return v == o.v; }

    inline static int s_copies = 0;
};

// ---------------------------------------------------------------------------
// 1. 容量与 int 语义
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestSizeIsInt()
{
    Seq<int> empty;

    // size() 必须是 int（不是 size_t）——Qt5 的口径。调用点大量把它塞进 int
    // 变量、与 int 比较、做减法。
    static_assert(std::is_same<decltype(empty.size()), int>::value,
                  "size() 必须返回 int");
    static_assert(std::is_same<decltype(empty.count()), int>::value,
                  "count() 必须返回 int");

    // 空容器上 size()-1 == -1，而不是回绕成天文数字。这正是要对齐的行为。
    PK_COMPARE(empty.size(), 0);
    PK_COMPARE(empty.size() - 1, -1);
    PK_VERIFY(empty.size() - 1 < 0);

    // for (int i = 0; i < v.size(); ++i) 不该有有符号/无符号比较警告，
    // 编译期由 -Wall -Wextra 守着；这里顺带压一遍运行期行为。
    Seq<int> v{10, 20, 30};
    int seen = 0;
    for (int i = 0; i < v.size(); ++i) {
        seen += v.at(i);
    }
    PK_COMPARE(seen, 60);
    PK_COMPARE(v.size(), 3);
    PK_COMPARE(v.count(), 3);
}

template <template <typename> class Seq>
void pkSeqTestSizeAndEmptiness()
{
    Seq<int> v;
    PK_VERIFY(v.isEmpty());
    PK_VERIFY(v.empty());
    PK_COMPARE(v.size(), 0);

    v.append(1);
    PK_VERIFY(!v.isEmpty());
    PK_VERIFY(!v.empty());
    PK_COMPARE(v.size(), 1);

    // count(const T&) 数出现次数（与无参 count() 是两个重载）
    Seq<int> multi{1, 2, 1, 3, 1};
    PK_COMPARE(multi.count(), 5);
    PK_COMPARE(multi.count(1), 3);
    PK_COMPARE(multi.count(2), 1);
    PK_COMPARE(multi.count(9), 0);

    // reserve 不改变 size
    Seq<int> r{1, 2};
    r.reserve(128);
    PK_COMPARE(r.size(), 2);
    PK_VERIFY((r == Seq<int>{1, 2}));

    v.clear();
    PK_VERIFY(v.isEmpty());
    PK_COMPARE(v.size(), 0);
}

// ---------------------------------------------------------------------------
// 2. 元素访问
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestElementAccess()
{
    Seq<int> v{10, 20, 30};
    const Seq<int> &cv = v;

    PK_COMPARE(v.at(0), 10);
    PK_COMPARE(v.at(2), 30);
    PK_COMPARE(cv[1], 20);
    PK_COMPARE(v[1], 20);

    PK_COMPARE(v.first(), 10);
    PK_COMPARE(cv.first(), 10);
    PK_COMPARE(v.last(), 30);
    PK_COMPARE(cv.last(), 30);
    PK_COMPARE(v.front(), 10);
    PK_COMPARE(cv.front(), 10);
    PK_COMPARE(v.back(), 30);
    PK_COMPARE(cv.back(), 30);

    // 非 const 的访问器返回可写引用
    v[0] = 11;
    PK_COMPARE(v.at(0), 11);
    v.first() = 12;
    PK_COMPARE(v.at(0), 12);
    v.last() = 31;
    PK_COMPARE(v.at(2), 31);
    v.front() = 13;
    PK_COMPARE(v.at(0), 13);
    v.back() = 32;
    PK_COMPARE(v.at(2), 32);

    // data()/constData() 指向同一段连续内存
    PK_COMPARE(*v.data(), 13);
    PK_COMPARE(*cv.data(), 13);
    PK_COMPARE(*cv.constData(), 13);
    PK_COMPARE(v.data()[2], 32);
    *v.data() = 14;
    PK_COMPARE(v.at(0), 14);
}

template <template <typename> class Seq>
void pkSeqTestValueOutOfRange()
{
    Seq<int> v{10, 20, 30};

    // 界内照常取值
    PK_COMPARE(v.value(0), 10);
    PK_COMPARE(v.value(2), 30);
    PK_COMPARE(v.value(1, 77), 20);

    // 越界（含负数）返回 T()：-2 / -1 / size / size+1 四个位置
    PK_COMPARE(v.value(-2), 0);
    PK_COMPARE(v.value(-1), 0);
    PK_COMPARE(v.value(v.size()), 0);
    PK_COMPARE(v.value(v.size() + 1), 0);

    // value(i, def) 同样四个位置都返回 def
    PK_COMPARE(v.value(-2, 77), 77);
    PK_COMPARE(v.value(-1, 77), 77);
    PK_COMPARE(v.value(v.size(), 77), 77);
    PK_COMPARE(v.value(v.size() + 1, 77), 77);

    // 空容器上任何下标都越界
    Seq<int> empty;
    PK_COMPARE(empty.value(0), 0);
    PK_COMPARE(empty.value(-1), 0);
    PK_COMPARE(empty.value(0, 5), 5);

    // value() 是 const 路径：不得 detach
    Seq<int> a{1, 2};
    Seq<int> b(a);
    (void)a.value(0);
    (void)a.value(99);
    (void)a.value(99, 3);
    PK_VERIFY(a.PkIsSharedWith(b));
}

// ---------------------------------------------------------------------------
// 3. 增删
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestAppendAndPrepend()
{
    Seq<int> v;
    // 字面量是右值 → 走 append(T&&)；下面那个具名 const 变量才走 append(const T&)。
    v.append(1);
    const int lvalue = 2;
    v.append(lvalue);
    PK_VERIFY((v == Seq<int>{1, 2}));

    int movable = 3;
    v.append(std::move(movable));
    PK_VERIFY((v == Seq<int>{1, 2, 3}));

    v.push_back(4);
    const int lvalue5 = 5;
    v.push_back(lvalue5);
    PK_VERIFY((v == Seq<int>{1, 2, 3, 4, 5}));
    v.remove(v.size() - 1);
    PK_VERIFY((v == Seq<int>{1, 2, 3, 4}));

    v.prepend(0);
    PK_VERIFY((v == Seq<int>{0, 1, 2, 3, 4}));

    v.push_front(-1);
    PK_VERIFY((v == Seq<int>{-1, 0, 1, 2, 3, 4}));

    // 追加另一个容器
    Seq<int> other{7, 8};
    Seq<int> base{1};
    base.append(other);
    PK_VERIFY((base == Seq<int>{1, 7, 8}));
    PK_VERIFY((other == Seq<int>{7, 8}));

    // 自追加：v.append(v) 不得因为迭代器失效而炸
    Seq<int> self{1, 2};
    self.append(self);
    PK_VERIFY((self == Seq<int>{1, 2, 1, 2}));

    // 追加自身的元素（引用指向自己的缓冲区）
    Seq<int> alias{5};
    alias.append(alias.at(0));
    PK_VERIFY((alias == Seq<int>{5, 5}));

    // 追加空容器是 no-op
    Seq<int> empty;
    Seq<int> keep{1, 2};
    keep.append(empty);
    PK_VERIFY((keep == Seq<int>{1, 2}));
}

template <template <typename> class Seq>
void pkSeqTestInsertAndRemove()
{
    Seq<int> v{1, 2, 3};

    v.insert(0, 0);
    PK_VERIFY((v == Seq<int>{0, 1, 2, 3}));
    v.insert(v.size(), 4);
    PK_VERIFY((v == Seq<int>{0, 1, 2, 3, 4}));
    v.insert(2, 9);
    PK_VERIFY((v == Seq<int>{0, 1, 9, 2, 3, 4}));

    // 迭代器版：返回指向新插入元素的迭代器
    auto it = v.insert(v.begin() + 1, 8);
    PK_COMPARE(*it, 8);
    PK_COMPARE(static_cast<int>(it - v.begin()), 1);
    PK_VERIFY((v == Seq<int>{0, 8, 1, 9, 2, 3, 4}));

    v.remove(1);
    PK_VERIFY((v == Seq<int>{0, 1, 9, 2, 3, 4}));
    v.remove(2, 2);
    PK_VERIFY((v == Seq<int>{0, 1, 3, 4}));
    v.remove(0, 0);   // n==0 是 no-op
    PK_VERIFY((v == Seq<int>{0, 1, 3, 4}));

    v.clear();
    PK_VERIFY(v.isEmpty());
    PK_COMPARE(v.size(), 0);
    // clear 之后照常可用
    v.append(42);
    PK_VERIFY((v == Seq<int>{42}));
}

template <template <typename> class Seq>
void pkSeqTestErase()
{
    Seq<int> v{1, 2, 3, 4, 5};

    // erase(pos) 返回被删元素之后那个元素的迭代器
    auto it = v.erase(v.begin() + 1);
    PK_COMPARE(*it, 3);
    PK_COMPARE(static_cast<int>(it - v.begin()), 1);
    PK_VERIFY((v == Seq<int>{1, 3, 4, 5}));

    // erase(first, last) 返回最后一个被删元素之后那个元素的迭代器
    auto it2 = v.erase(v.begin() + 1, v.begin() + 3);
    PK_COMPARE(*it2, 5);
    PK_COMPARE(static_cast<int>(it2 - v.begin()), 1);
    PK_VERIFY((v == Seq<int>{1, 5}));

    // 删到末尾：返回 end()
    Seq<int> w{1, 2};
    auto it3 = w.erase(w.begin() + 1);
    PK_VERIFY(it3 == w.end());
    PK_VERIFY((w == Seq<int>{1}));

    // 空区间是 no-op
    Seq<int> x{1, 2, 3};
    auto it4 = x.erase(x.begin() + 1, x.begin() + 1);
    PK_COMPARE(*it4, 2);
    PK_VERIFY((x == Seq<int>{1, 2, 3}));

    // 共享状态下 erase：偏移必须按调用时的缓冲区算，detach 之后仍然指对位置
    Seq<int> a{1, 2, 3, 4};
    Seq<int> b(a);
    auto bit = b.begin();          // begin() 自己会 detach
    auto it5 = b.erase(bit + 2);
    PK_COMPARE(*it5, 4);
    PK_VERIFY((b == Seq<int>{1, 2, 4}));
    PK_VERIFY((a == Seq<int>{1, 2, 3, 4}));
}

// ---------------------------------------------------------------------------
// 4. 查找
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestSearch()
{
    Seq<int> v{1, 2, 3, 2, 1};

    PK_VERIFY(v.contains(1));
    PK_VERIFY(v.contains(3));
    PK_VERIFY(!v.contains(9));

    PK_COMPARE(v.indexOf(2), 1);
    PK_COMPARE(v.indexOf(2, 2), 3);
    PK_COMPARE(v.indexOf(9), -1);
    PK_COMPARE(v.indexOf(1, 1), 4);
    // from 越过末尾 → 找不到
    PK_COMPARE(v.indexOf(1, v.size()), -1);
    PK_COMPARE(v.indexOf(1, 99), -1);
    // 负 from 按 from + size 折算，折算后仍为负则从头找（Qt 口径）
    PK_COMPARE(v.indexOf(1, -1), 4);
    PK_COMPARE(v.indexOf(1, -99), 0);

    PK_COMPARE(v.lastIndexOf(2), 3);
    PK_COMPARE(v.lastIndexOf(2, 2), 1);
    PK_COMPARE(v.lastIndexOf(9), -1);
    PK_COMPARE(v.lastIndexOf(1, -1), 4);
    // from 超出末尾按最后一个元素算
    PK_COMPARE(v.lastIndexOf(1, 99), 4);
    // 折算后为负 → 找不到
    PK_COMPARE(v.lastIndexOf(1, -99), -1);

    Seq<int> empty;
    PK_VERIFY(!empty.contains(0));
    PK_COMPARE(empty.indexOf(0), -1);
    PK_COMPARE(empty.lastIndexOf(0), -1);

    // 查找是 const 路径：不得 detach
    Seq<int> a{1, 2};
    Seq<int> b(a);
    (void)a.contains(1);
    (void)a.indexOf(1);
    (void)a.lastIndexOf(1);
    (void)a.count(1);
    PK_VERIFY(a.PkIsSharedWith(b));
}

// ---------------------------------------------------------------------------
// 5. 迭代器
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestIterators()
{
    Seq<int> v{1, 2, 3};
    const Seq<int> &cv = v;

    // range-for 走 begin()/end()
    int sum = 0;
    for (int x : v) {
        sum += x;
    }
    PK_COMPARE(sum, 6);

    // const 容器上的 range-for 走 const 重载
    int csum = 0;
    for (int x : cv) {
        csum += x;
    }
    PK_COMPARE(csum, 6);

    PK_COMPARE(static_cast<int>(v.end() - v.begin()), 3);
    PK_COMPARE(*v.begin(), 1);
    PK_COMPARE(*(v.end() - 1), 3);

    // constBegin/constEnd 与 cbegin/cend 行为完全一致
    PK_VERIFY(v.constBegin() == v.cbegin());
    PK_VERIFY(v.constEnd() == v.cend());
    PK_COMPARE(static_cast<int>(v.constEnd() - v.constBegin()), 3);
    PK_COMPARE(static_cast<int>(v.cend() - v.cbegin()), 3);
    PK_COMPARE(*v.constBegin(), 1);
    PK_COMPARE(*v.cbegin(), 1);
    PK_COMPARE(*(v.constEnd() - 1), 3);
    PK_COMPARE(*(v.cend() - 1), 3);

    // 反向迭代器
    PK_COMPARE(*v.rbegin(), 3);
    PK_COMPARE(*(v.rend() - 1), 1);
    PK_COMPARE(static_cast<int>(v.rend() - v.rbegin()), 3);
    PK_COMPARE(*cv.rbegin(), 3);
    PK_COMPARE(*(cv.rend() - 1), 1);

    // 非 const 迭代器可写
    *v.begin() = 9;
    PK_COMPARE(v.at(0), 9);
    for (int &x : v) {
        x += 1;
    }
    PK_VERIFY((v == Seq<int>{10, 3, 4}));

    // 空容器：begin()==end()
    Seq<int> empty;
    PK_VERIFY(empty.begin() == empty.end());
    PK_VERIFY(empty.constBegin() == empty.constEnd());
    PK_VERIFY(empty.cbegin() == empty.cend());
}

template <template <typename> class Seq>
void pkSeqTestConstIteratorsDoNotDetach()
{
    Seq<int> a{1, 2, 3};
    Seq<int> b(a);
    PK_VERIFY(a.PkIsSharedWith(b));

    // 167 处 constBegin + 190 处 constEnd 靠这条保持 O(1)
    for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
        (void)*it;
    }
    for (auto it = a.cbegin(); it != a.cend(); ++it) {
        (void)*it;
    }
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);

    // const 引用上的 begin()/end()/rbegin()/rend() 走 const 重载，同样不 detach
    const Seq<int> &ca = a;
    for (auto it = ca.begin(); it != ca.end(); ++it) {
        (void)*it;
    }
    for (auto it = ca.rbegin(); it != ca.rend(); ++it) {
        (void)*it;
    }
    PK_VERIFY(a.PkIsSharedWith(b));
}

// ---------------------------------------------------------------------------
// 6. 比较与运算符
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestComparison()
{
    Seq<int> a{1, 2, 3};
    Seq<int> b{1, 2, 3};
    Seq<int> c{1, 2};
    Seq<int> d{1, 2, 4};

    PK_VERIFY(a == b);
    PK_VERIFY(!(a != b));
    PK_VERIFY(a != c);
    PK_VERIFY(a != d);
    PK_VERIFY(!(a == c));

    // 共享同一份缓冲区时是 O(1) 的真
    Seq<int> shared(a);
    PK_VERIFY(a == shared);

    // 自比较
    const Seq<int> &alias = a;
    PK_VERIFY(a == alias);

    Seq<int> e1;
    Seq<int> e2;
    PK_VERIFY(e1 == e2);
    PK_VERIFY(!(e1 != e2));
    PK_VERIFY(e1 != a);

    // 比较是 const 路径：不得 detach
    Seq<int> x{5};
    Seq<int> y(x);
    (void)(x == y);
    (void)(x != y);
    PK_VERIFY(x.PkIsSharedWith(y));
}

template <template <typename> class Seq>
void pkSeqTestStreamOperators()
{
    Seq<int> v;
    v << 1 << 2 << 3;
    PK_VERIFY((v == Seq<int>{1, 2, 3}));

    v += 4;
    PK_VERIFY((v == Seq<int>{1, 2, 3, 4}));

    Seq<int> other{8, 9};
    v += other;
    PK_VERIFY((v == Seq<int>{1, 2, 3, 4, 8, 9}));

    Seq<int> w{0};
    w << other;
    PK_VERIFY((w == Seq<int>{0, 8, 9}));

    // 返回自身引用，可链式
    Seq<int> chain;
    (chain << 1) << 2;
    PK_VERIFY((chain == Seq<int>{1, 2}));
    (chain += 3) += 4;
    PK_VERIFY((chain == Seq<int>{1, 2, 3, 4}));
}

// ---------------------------------------------------------------------------
// 7. COW —— 本任务的核心
// ---------------------------------------------------------------------------

template <template <typename> class Seq>
void pkSeqTestCowIsolation()
{
    Seq<int> a{1, 2, 3};
    Seq<int> b(a);

    // 拷贝 = 共享
    PK_VERIFY(a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 2L);
    PK_VERIFY(a == b);

    // 改 b，a 一个字节都没变
    b.append(4);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_COMPARE(a.size(), 3);
    PK_VERIFY((a == Seq<int>{1, 2, 3}));
    PK_VERIFY((b == Seq<int>{1, 2, 3, 4}));

    // 反方向：再共享一次，这回改 a
    Seq<int> c(a);
    PK_VERIFY(a.PkIsSharedWith(c));
    a[0] = 99;
    PK_VERIFY(!a.PkIsSharedWith(c));
    PK_VERIFY((c == Seq<int>{1, 2, 3}));
    PK_VERIFY((a == Seq<int>{99, 2, 3}));

    // 拷贝赋值同样是共享
    Seq<int> d;
    d = a;
    PK_VERIFY(a.PkIsSharedWith(d));
    d.clear();
    PK_VERIFY(!a.PkIsSharedWith(d));
    PK_VERIFY((a == Seq<int>{99, 2, 3}));

    // 三方共享：一方写只把自己摘出去，另外两方继续共享
    Seq<int> p{1};
    Seq<int> q(p);
    Seq<int> r(p);
    PK_COMPARE(p.PkUseCount(), 3L);
    q.append(2);
    PK_VERIFY(p.PkIsSharedWith(r));
    PK_COMPARE(p.PkUseCount(), 2L);
    PK_VERIFY((p == Seq<int>{1}));
    PK_VERIFY((r == Seq<int>{1}));
}

template <template <typename> class Seq>
void pkSeqTestCopyIsConstantTime()
{
    // 硬要求 3：拷贝必须 O(1)。2286 处 Q_FOREACH 按值拷贝整个容器全指望这一条。
    Seq<PkSeqCounted> a;
    a.reserve(5);
    for (int i = 0; i < 5; ++i) {
        a.append(PkSeqCounted(i));
    }

    PkSeqCounted::s_copies = 0;
    Seq<PkSeqCounted> b(a);
    PK_COMPARE(PkSeqCounted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 2L);

    Seq<PkSeqCounted> c;
    c = a;
    PK_COMPARE(PkSeqCounted::s_copies, 0);
    PK_COMPARE(a.PkUseCount(), 3L);

    // 正向对照：共享状态下写一下，确实深拷了 5 个元素（计数器本身没失灵）
    PkSeqCounted::s_copies = 0;
    b.append(PkSeqCounted(9));
    PK_COMPARE(PkSeqCounted::s_copies, 5);

    // 摘出去之后再写就不拷了
    PkSeqCounted::s_copies = 0;
    b.append(PkSeqCounted(10));
    PK_COMPARE(PkSeqCounted::s_copies, 0);
}

template <template <typename> class Seq>
void pkSeqTestConstNeverDetaches()
{
    Seq<int> a{1, 2, 3};
    Seq<int> b(a);
    const Seq<int> &ca = a;
    PK_VERIFY(a.PkIsSharedWith(b));

    // 一串 const 方法轮流调，全程共享状态不得变
    for (int i = 0; i < 3; ++i) {
        (void)ca.size();
        (void)ca.count();
        (void)ca.count(1);
        (void)ca.isEmpty();
        (void)ca.empty();
        (void)ca.at(0);
        (void)ca[1];
        (void)ca.value(0);
        (void)ca.value(99, 7);
        (void)ca.first();
        (void)ca.last();
        (void)ca.front();
        (void)ca.back();
        (void)ca.data();
        (void)ca.constData();
        (void)ca.contains(2);
        (void)ca.indexOf(2);
        (void)ca.lastIndexOf(2);
        (void)ca.constBegin();
        (void)ca.constEnd();
        (void)ca.cbegin();
        (void)ca.cend();
        (void)ca.begin();
        (void)ca.end();
        (void)ca.rbegin();
        (void)ca.rend();
        (void)(ca == b);
        (void)(ca != b);

        PK_VERIFY(a.PkIsSharedWith(b));
        PK_COMPARE(a.PkUseCount(), 2L);
    }

    // 缓冲区自始至终没被换掉
    PK_VERIFY(a.constData() == b.constData());
    PK_VERIFY((a == Seq<int>{1, 2, 3}));
}

// 每一个非 const 方法逐个验证：共享状态下调用之后两边不再共享，且另一边内容不变。
// 这是查 PkMut() 漏用的唯一办法——漏一个方法就是一个 COW 漏洞。
template <template <typename> class Seq, typename F>
void pkSeqCheckDetaches(const char *what, F mutate)
{
    Seq<int> a{1, 2, 3};
    Seq<int> b(a);
    PK_VERIFY2(a.PkIsSharedWith(b), what);

    mutate(b);

    PK_VERIFY2(!a.PkIsSharedWith(b), what);
    PK_VERIFY2(a.PkUseCount() == 1L, what);
    PK_VERIFY2((a == Seq<int>{1, 2, 3}), what);
    PK_VERIFY2(a.size() == 3, what);
}

template <template <typename> class Seq>
void pkSeqTestEveryWriterDetaches()
{
    using S = Seq<int>;

    pkSeqCheckDetaches<Seq>("reserve", [](S &s) { s.reserve(64); });
    pkSeqCheckDetaches<Seq>("operator[]", [](S &s) { s[0] = 9; });
    pkSeqCheckDetaches<Seq>("data", [](S &s) { *s.data() = 9; });
    pkSeqCheckDetaches<Seq>("first", [](S &s) { s.first() = 9; });
    pkSeqCheckDetaches<Seq>("last", [](S &s) { s.last() = 9; });
    pkSeqCheckDetaches<Seq>("front", [](S &s) { s.front() = 9; });
    pkSeqCheckDetaches<Seq>("back", [](S &s) { s.back() = 9; });
    // append/push_back 各有 const T& 与 T&& 两个重载，**必须分别压**：
    // `s.append(4)` 里的 4 是右值，走的是 T&& 那条，const T& 那条一个字都没被
    // 执行到。这个陷阱是变异测试（把 append(const T&) 改成绕过 PkMut()）压出来的
    // ——改坏了它，只有 operator+= / operator<< 报错，两条 append 用例全绿。
    pkSeqCheckDetaches<Seq>("append(const T&)", [](S &s) {
        const int x = 4;
        s.append(x);
    });
    pkSeqCheckDetaches<Seq>("append(T&&)", [](S &s) {
        int x = 4;
        s.append(std::move(x));
    });
    pkSeqCheckDetaches<Seq>("append(container)", [](S &s) {
        S o{7, 8};
        s.append(o);
    });
    pkSeqCheckDetaches<Seq>("push_back(const T&)", [](S &s) {
        const int x = 4;
        s.push_back(x);
    });
    pkSeqCheckDetaches<Seq>("push_back(T&&)", [](S &s) {
        int x = 4;
        s.push_back(std::move(x));
    });
    pkSeqCheckDetaches<Seq>("prepend", [](S &s) { s.prepend(0); });
    pkSeqCheckDetaches<Seq>("push_front", [](S &s) { s.push_front(0); });
    pkSeqCheckDetaches<Seq>("insert(int, const T&)", [](S &s) { s.insert(1, 9); });
    pkSeqCheckDetaches<Seq>("insert(iterator, const T&)",
                            [](S &s) { s.insert(s.begin(), 9); });
    pkSeqCheckDetaches<Seq>("remove(int)", [](S &s) { s.remove(0); });
    pkSeqCheckDetaches<Seq>("remove(int, int)", [](S &s) { s.remove(0, 2); });
    pkSeqCheckDetaches<Seq>("clear", [](S &s) { s.clear(); });
    pkSeqCheckDetaches<Seq>("begin", [](S &s) { *s.begin() = 9; });
    pkSeqCheckDetaches<Seq>("end", [](S &s) { *(s.end() - 1) = 9; });
    pkSeqCheckDetaches<Seq>("rbegin", [](S &s) { *s.rbegin() = 9; });
    pkSeqCheckDetaches<Seq>("rend", [](S &s) { *(s.rend() - 1) = 9; });
    pkSeqCheckDetaches<Seq>("erase(pos)", [](S &s) { s.erase(s.begin()); });
    pkSeqCheckDetaches<Seq>("erase(first, last)",
                            [](S &s) { s.erase(s.begin(), s.begin() + 2); });
    pkSeqCheckDetaches<Seq>("operator+=(const T&)", [](S &s) { s += 4; });
    pkSeqCheckDetaches<Seq>("operator+=(container)", [](S &s) {
        S o{7};
        s += o;
    });
    pkSeqCheckDetaches<Seq>("operator<<(const T&)", [](S &s) { s << 4; });
    pkSeqCheckDetaches<Seq>("operator<<(container)", [](S &s) {
        S o{7};
        s << o;
    });
}

// swap 不在上面那张表里：它交换的是缓冲区指针本身，Qt 下同样**不 detach**，
// 共享关系跟着缓冲区走。用上面的判据压它会得出错误结论，所以单列。
template <template <typename> class Seq>
void pkSeqTestSwap()
{
    Seq<int> a{1, 2};
    Seq<int> b{3};
    Seq<int> aShare(a);   // a 的共享伙伴，交换后应当跟着缓冲区走到 b 那边

    a.swap(b);

    PK_VERIFY((a == Seq<int>{3}));
    PK_VERIFY((b == Seq<int>{1, 2}));
    PK_VERIFY(b.PkIsSharedWith(aShare));
    PK_VERIFY(!a.PkIsSharedWith(aShare));
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_COMPARE(a.PkUseCount(), 1L);

    // 自交换安全
    Seq<int> &alias = a;
    a.swap(alias);
    PK_VERIFY((a == Seq<int>{3}));
    PK_COMPARE(a.PkUseCount(), 1L);

    // 交换之后两边照常各自可写、COW 照常
    b.append(9);
    PK_VERIFY(!b.PkIsSharedWith(aShare));
    PK_VERIFY((aShare == Seq<int>{1, 2}));

    // 零元素拷贝
    Seq<PkSeqCounted> x;
    Seq<PkSeqCounted> y;
    x.append(PkSeqCounted(1));
    y.append(PkSeqCounted(2));
    y.append(PkSeqCounted(3));
    PkSeqCounted::s_copies = 0;
    x.swap(y);
    PK_COMPARE(PkSeqCounted::s_copies, 0);
    PK_COMPARE(x.size(), 2);
    PK_COMPARE(y.size(), 1);
}

template <template <typename> class Seq>
void pkSeqTestSelfAssignment()
{
    Seq<int> a{1, 2, 3};
    // 经引用绕一道：直接写 a = a 会被 -Wself-assign-overloaded 拦下，
    // 而真实调用点里的自赋值本来就是通过别名/引用发生的。
    Seq<int> &alias = a;
    a = alias;

    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY((a == Seq<int>{1, 2, 3}));

    // 自赋值之后照常可写
    a.append(4);
    PK_VERIFY((a == Seq<int>{1, 2, 3, 4}));
    PK_COMPARE(a.PkUseCount(), 1L);

    // 共享状态下的自赋值：共享关系与内容都不该被破坏
    Seq<int> b{5, 6};
    Seq<int> c(b);
    Seq<int> &bAlias = b;
    b = bAlias;
    PK_COMPARE(b.PkUseCount(), 2L);
    PK_VERIFY(b.PkIsSharedWith(c));
    PK_VERIFY((b == Seq<int>{5, 6}));
    PK_VERIFY((c == Seq<int>{5, 6}));

    // 自赋值之后 COW 仍然生效
    b.append(7);
    PK_VERIFY(!b.PkIsSharedWith(c));
    PK_VERIFY((c == Seq<int>{5, 6}));
}

// 移动语义由 PkArrayData 兜底（移动之后源是「空且完全可用」的容器，Qt 语义），
// 容器层只 = default。这里验证那份兜底真的透过容器层生效了。
template <template <typename> class Seq>
void pkSeqTestMoveLeavesSourceUsable()
{
    Seq<int> a{1, 2, 3};
    Seq<int> b(std::move(a));

    PK_VERIFY((b == Seq<int>{1, 2, 3}));
    PK_COMPARE(b.PkUseCount(), 1L);

    // 源：空容器，且完全可用
    PK_COMPARE(a.size(), 0);
    PK_VERIFY(a.isEmpty());
    PK_COMPARE(a.PkUseCount(), 1L);
    PK_VERIFY(a.begin() == a.end());
    a.append(42);
    PK_VERIFY((a == Seq<int>{42}));
    PK_VERIFY((b == Seq<int>{1, 2, 3}));
    PK_VERIFY(!a.PkIsSharedWith(b));

    // 移动赋值同样
    Seq<int> c{7, 8};
    Seq<int> d{0};
    d = std::move(c);
    PK_VERIFY((d == Seq<int>{7, 8}));
    PK_COMPARE(c.size(), 0);
    PK_VERIFY(c.isEmpty());
    c.append(5);
    PK_VERIFY((c == Seq<int>{5}));
    PK_VERIFY((d == Seq<int>{7, 8}));

    // 移动是 O(1)
    Seq<PkSeqCounted> e;
    e.append(PkSeqCounted(1));
    e.append(PkSeqCounted(2));
    PkSeqCounted::s_copies = 0;
    Seq<PkSeqCounted> f(std::move(e));
    PK_COMPARE(PkSeqCounted::s_copies, 0);
    PK_COMPARE(f.size(), 2);
    PK_COMPARE(e.size(), 0);

    // 拷贝构造/拷贝赋值没有因为声明了移动而被 deleted
    static_assert(std::is_copy_constructible<Seq<int>>::value,
                  "拷贝构造必须存在——声明移动构造会把它 deleted 掉");
    static_assert(std::is_copy_assignable<Seq<int>>::value, "拷贝赋值必须存在");
    static_assert(std::is_move_constructible<Seq<int>>::value, "移动构造必须存在");
    static_assert(std::is_move_assignable<Seq<int>>::value, "移动赋值必须存在");
}

template <template <typename> class Seq>
void pkSeqTestInitializerListAndDefaults()
{
    Seq<int> empty;
    PK_COMPARE(empty.size(), 0);
    PK_VERIFY(empty.isEmpty());

    Seq<int> one{5};
    PK_COMPARE(one.size(), 1);
    PK_COMPARE(one.at(0), 5);

    Seq<int> many{1, 2, 3, 4};
    PK_COMPARE(many.size(), 4);
    PK_COMPARE(many.at(3), 4);
    PK_COMPARE(many.PkUseCount(), 1L);
}
