#include "PkForeachTest.h"

#include "../PkContainerAlgo.h"
#include "../PkHash.h"
#include "../PkList.h"
#include "../PkMap.h"
#include "../PkQueue.h"
#include "../PkSet.h"
#include "../PkStack.h"
#include "../PkVector.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

// PkTestBinder<PkForeachTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkForeachTest> 实例化前对本 TU
// 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkForeachTest.inc"

// ---------------------------------------------------------------------------
// 堆分配探针 —— 与 tests/test_arraydata.cpp 同一套（程序级替换全局
// operator new/delete，测量一律"窗口前后取差"）。
//
// 为什么本文件非要它不可：PK_FOREACH 的全部价值就是"拷贝容器 O(1)"。只数元素
// 拷贝的话，"实现改成每轮 make_shared 一份新缓冲区"这种把 O(1) 换成 N 次分配的
// 退化查不出来；只数引用计数的话，"多拷一个元素"查不出来。**两个维度都要压。**
// （R-02 上一轮 removeAll 的第二条偏离——不命中时白拷一个元素——正是只压了
//  PkIsSharedWith、没压拷贝计数才漏掉的。）
// ---------------------------------------------------------------------------

static long g_pkAllocCount = 0;   // 常量初始化，先于任何动态初始化

void *operator new(std::size_t n)
{
    ++g_pkAllocCount;
    void *p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void *operator new[](std::size_t n)
{
    ++g_pkAllocCount;
    void *p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {

// 输出改行缓冲：断言失败若伴随段错误，全缓冲的 stdout 会把崩溃前的行整段吞掉，
// 现场只剩一个不知道死在哪的 SIGSEGV。静态对象的构造先于 main。
struct PkLineBufferedStdout
{
    PkLineBufferedStdout() { std::setvbuf(stdout, nullptr, _IOLBF, 0); }
};
const PkLineBufferedStdout g_pkLineBuffered;

// 带拷贝计数器的元素类型。移动显式 noexcept，否则 vector 扩容会退化成逐元素
// 拷贝，计数器就分不清"容器拷贝拷的"与"扩容拷的"（与 test_arraydata.cpp 的
// Counted、PkSeqTestShared.h 的 PkSeqCounted 同因）。
struct PkForeachCounted
{
    int v = 0;

    PkForeachCounted() = default;
    explicit PkForeachCounted(int x) : v(x) {}
    PkForeachCounted(const PkForeachCounted &o) : v(o.v) { ++s_copies; }
    PkForeachCounted &operator=(const PkForeachCounted &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    PkForeachCounted(PkForeachCounted &&) noexcept = default;
    PkForeachCounted &operator=(PkForeachCounted &&) noexcept = default;
    ~PkForeachCounted() = default;

    bool operator==(const PkForeachCounted &o) const { return v == o.v; }

    static int s_copies;
};

int PkForeachCounted::s_copies = 0;

// 带析构计数器的元素类型，专供 qDeleteAll —— 证明"每个元素恰好 delete 一次"。
struct PkDeleteProbe
{
    int v = 0;
    explicit PkDeleteProbe(int x) : v(x) {}
    ~PkDeleteProbe() { ++s_destroyed; }

    PkDeleteProbe(const PkDeleteProbe &) = delete;
    PkDeleteProbe &operator=(const PkDeleteProbe &) = delete;

    static int s_destroyed;
};

int PkDeleteProbe::s_destroyed = 0;

} // namespace

// ---------------------------------------------------------------------------
// 1. 迭代顺序与内容
// ---------------------------------------------------------------------------

void PkForeachTest::iterationOrderAndContent()
{
    // 序列容器：顺序必须与 at(0..n-1) 一致
    PkVector<int> vec{10, 20, 30};
    PkVector<int> seenVec;
    PK_FOREACH (int x, vec) {
        seenVec.append(x);
    }
    PK_VERIFY((seenVec == PkVector<int>{10, 20, 30}));

    PkList<int> list{1, 2, 3, 4};
    int sum = 0;
    PK_FOREACH (int x, list) {
        sum += x;
    }
    PK_COMPARE(sum, 10);

    // 派生类（PkStack : PkVector、PkQueue : PkList）—— const_iterator 是继承来的，
    // PkForeachContainer<T> 的 `typename T::const_iterator` 必须在派生类上也成立。
    PkStack<int> stack;
    stack.push(7);
    stack.push(8);
    int stackSum = 0;
    PK_FOREACH (int x, stack) {
        stackSum += x;
    }
    PK_COMPARE(stackSum, 15);

    PkQueue<int> queue;
    queue.enqueue(3);
    queue.enqueue(4);
    int queueSum = 0;
    PK_FOREACH (int x, queue) {
        queueSum += x;
    }
    PK_COMPARE(queueSum, 7);

    // 关联容器：`*it` 给的是 **value** 不是 pair（PkAssocIterator 的形状），
    // 所以 PK_FOREACH 遍历的是 value —— 与 Qt 的 Q_FOREACH(V v, map) 一致。
    PkMap<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);
    int mapSum = 0;
    int mapCount = 0;
    PK_FOREACH (int v, map) {
        mapSum += v;
        ++mapCount;
    }
    PK_COMPARE(mapSum, 600);
    PK_COMPARE(mapCount, 3);

    // PkHash 的迭代顺序未定义，只断言"每个 value 恰好出现一次"
    PkHash<int, int> hash;
    hash.insert(1, 11);
    hash.insert(2, 22);
    PkVector<int> hashSeen;
    PK_FOREACH (int v, hash) {
        hashSeen.append(v);
    }
    PK_COMPARE(hashSeen.size(), 2);
    PK_VERIFY(hashSeen.contains(11));
    PK_VERIFY(hashSeen.contains(22));

    // PkSet 同样无序
    PkSet<int> set{5, 6, 7};
    int setSum = 0;
    int setCount = 0;
    PK_FOREACH (int x, set) {
        setSum += x;
        ++setCount;
    }
    PK_COMPARE(setSum, 18);
    PK_COMPARE(setCount, 3);

    // 循环变量的三种写法都要能编：按值、const 引用、带 const 的按值
    PkVector<int> forms{1, 2};
    int a = 0;
    PK_FOREACH (int x, forms) {
        a += x;
    }
    PK_FOREACH (const int &x, forms) {
        a += x;
    }
    PK_FOREACH (const int x, forms) {
        a += x;
    }
    PK_COMPARE(a, 9);
}

// ---------------------------------------------------------------------------
// 2. 拷贝语义：循环体内改原容器，本次迭代不受影响
// ---------------------------------------------------------------------------

void PkForeachTest::bodyMutationDoesNotAffectIteration()
{
    // append：迭代的是拷贝，新加的元素这一轮看不见（否则会无限循环）
    PkVector<int> v{1, 2, 3};
    int rounds = 0;
    int sum = 0;
    PK_FOREACH (int x, v) {
        sum += x;
        ++rounds;
        v.append(99);
        if (rounds > 10) {
            break;   // 保险丝：真的退化成"迭代原容器"时不至于卡死测试
        }
    }
    PK_COMPARE(rounds, 3);
    PK_COMPARE(sum, 6);
    PK_COMPARE(v.size(), 6);

    // clear()：把原容器整个清空，本次迭代照样跑完
    PkList<int> l{4, 5, 6};
    int cleared = 0;
    PK_FOREACH (int x, l) {
        cleared += x;
        l.clear();
    }
    PK_COMPARE(cleared, 15);
    PK_COMPARE(l.size(), 0);

    // 改元素值：拷贝先于循环发生，改的是原容器那一份
    PkVector<int> m{7, 8};
    int observed = 0;
    PK_FOREACH (int x, m) {
        observed += x;
        m[0] = 1000;
    }
    PK_COMPARE(observed, 15);
    PK_COMPARE(m.at(0), 1000);

    // 关联容器同理
    PkMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    int mapRounds = 0;
    PK_FOREACH (int x, map) {
        (void)x;
        ++mapRounds;
        map.insert(mapRounds + 100, 999);
        if (mapRounds > 10) {
            break;
        }
    }
    PK_COMPARE(mapRounds, 2);
    PK_COMPARE(map.size(), 4);
}

// ---------------------------------------------------------------------------
// 3. 拷贝是 O(1)：引用计数 2 + 元素零拷贝 + 堆分配零次
// ---------------------------------------------------------------------------

void PkForeachTest::copyIsConstantTime()
{
    PkVector<PkForeachCounted> v;
    v.reserve(8);
    for (int k = 0; k < 5; ++k) {
        v.append(PkForeachCounted(k));
    }
    PK_COMPARE(v.PkUseCount(), 1L);

    // 窗口里只放循环本身：PK_COMPARE 自己会构造 std::string，放进窗口会污染
    // 分配计数。观测值先落进局部变量，出了窗口再断言。
    int sum = 0;
    long useCountInside = -1;
    int copiesInsideFirstRound = -1;

    PkForeachCounted::s_copies = 0;
    const long allocBefore = g_pkAllocCount;
    PK_FOREACH (const PkForeachCounted &e, v) {
        sum += e.v;
        useCountInside = v.PkUseCount();
        if (copiesInsideFirstRound < 0) {
            copiesInsideFirstRound = PkForeachCounted::s_copies;
        }
    }
    const long allocAfter = g_pkAllocCount;
    const int copiesTotal = PkForeachCounted::s_copies;

    PK_COMPARE(sum, 10);
    // ① 循环期间原容器与宏内部那份共享 —— 引用计数正好 2
    PK_COMPARE(useCountInside, 2L);
    // ② 第一轮进入循环体时，容器拷贝已经完成而元素**一个都没被拷**
    PK_COMPARE(copiesInsideFirstRound, 0);
    // ③ 整个循环下来仍然是 0（const 引用绑定不产生拷贝）
    PK_COMPARE(copiesTotal, 0);
    // ④ 堆分配零次 —— 拷贝就是一次 shared_ptr 引用计数自增，没有 malloc
    PK_COMPARE(allocAfter - allocBefore, 0L);
    // ⑤ 循环结束后那份拷贝析构，引用计数回到 1
    PK_COMPARE(v.PkUseCount(), 1L);

    // 关联容器上同样是 O(1)
    PkMap<int, int> map;
    for (int k = 0; k < 5; ++k) {
        map.insert(k, k * 10);
    }
    long mapUseCountInside = -1;
    int mapSum = 0;
    const long mapAllocBefore = g_pkAllocCount;
    PK_FOREACH (int x, map) {
        mapSum += x;
        mapUseCountInside = map.PkUseCount();
    }
    const long mapAllocAfter = g_pkAllocCount;
    PK_COMPARE(mapSum, 100);
    PK_COMPARE(mapUseCountInside, 2L);
    PK_COMPARE(mapAllocAfter - mapAllocBefore, 0L);
    PK_COMPARE(map.PkUseCount(), 1L);
}

void PkForeachTest::byValueLoopVariableCopiesEachElementOnce()
{
    // 反面证据：上一条的"零拷贝"不是因为计数器坏了。按值绑定循环变量时，
    // **每个元素恰好拷一次**（那是循环变量自己的构造，不是容器拷贝）。
    PkVector<PkForeachCounted> v;
    v.reserve(8);
    for (int k = 0; k < 5; ++k) {
        v.append(PkForeachCounted(k));
    }

    int sum = 0;
    PkForeachCounted::s_copies = 0;
    PK_FOREACH (PkForeachCounted e, v) {
        sum += e.v;
    }
    const int copies = PkForeachCounted::s_copies;

    PK_COMPARE(sum, 10);
    PK_COMPARE(copies, 5);
}

// ---------------------------------------------------------------------------
// 4. break / continue
// ---------------------------------------------------------------------------

void PkForeachTest::breakAndContinue()
{
    PkVector<int> v{1, 2, 3, 4, 5};

    // break：控制位机制让外层也一起退出
    int sum = 0;
    int rounds = 0;
    PK_FOREACH (int x, v) {
        ++rounds;
        if (x == 3) {
            break;
        }
        sum += x;
    }
    PK_COMPARE(sum, 3);
    PK_COMPARE(rounds, 3);

    // 第一轮就 break
    int firstRoundBreak = 0;
    PK_FOREACH (int x, v) {
        (void)x;
        ++firstRoundBreak;
        break;
    }
    PK_COMPARE(firstRoundBreak, 1);

    // continue：跳过本轮剩余，循环照常走完
    int odd = 0;
    int continueRounds = 0;
    PK_FOREACH (int x, v) {
        ++continueRounds;
        if (x % 2 == 0) {
            continue;
        }
        odd += x;
    }
    PK_COMPARE(odd, 9);
    PK_COMPARE(continueRounds, 5);

    // break 之后的代码照常执行（不是从函数里跳出去了）
    int after = 0;
    PK_FOREACH (int x, v) {
        if (x == 1) {
            break;
        }
    }
    after = 42;
    PK_COMPARE(after, 42);

    // 关联容器上的 break
    PkMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    int mapRounds = 0;
    PK_FOREACH (int x, map) {
        ++mapRounds;
        if (x == 20) {
            break;
        }
    }
    PK_COMPARE(mapRounds, 2);
}

// ---------------------------------------------------------------------------
// 5. 嵌套
// ---------------------------------------------------------------------------

void PkForeachTest::nested()
{
    PkVector<int> outer{1, 2, 3};
    PkVector<int> inner{10, 20};

    // 两层跑满：3 × 2 = 6 轮，和 = 3*(10+20) + 2*(1+2+3) = 90 + 12
    int pairs = 0;
    int total = 0;
    PK_FOREACH (int a, outer) {
        PK_FOREACH (int b, inner) {
            ++pairs;
            total += a + b;
        }
    }
    PK_COMPARE(pairs, 6);
    PK_COMPARE(total, 102);

    // 内层 break 只退内层，外层照常继续 —— 内层的 _pk_foreach_ 遮蔽外层的，
    // 两层的游标互不干扰。这是"作用域遮蔽"这条机制唯一的直接证据。
    int innerBreakPairs = 0;
    int outerRounds = 0;
    PK_FOREACH (int a, outer) {
        (void)a;
        ++outerRounds;
        PK_FOREACH (int b, inner) {
            ++innerBreakPairs;
            if (b == 10) {
                break;
            }
        }
    }
    PK_COMPARE(outerRounds, 3);
    PK_COMPARE(innerBreakPairs, 3);

    // 内层 continue 也只作用于内层
    int innerContinuePairs = 0;
    PK_FOREACH (int a, outer) {
        (void)a;
        PK_FOREACH (int b, inner) {
            if (b == 10) {
                continue;
            }
            ++innerContinuePairs;
        }
    }
    PK_COMPARE(innerContinuePairs, 3);

    // 三层，且内两层用**不同类型**的容器（模板实参不同 → _pk_foreach_ 类型不同，
    // 遮蔽仍然成立）
    PkList<int> third{100};
    int deep = 0;
    PK_FOREACH (int a, outer) {
        PK_FOREACH (int b, inner) {
            PK_FOREACH (int c, third) {
                deep += a + b + c;
            }
        }
    }
    PK_COMPARE(deep, 702);

    // 外层 break 时内层已经跑过 —— 外层退出，函数继续
    int mixed = 0;
    PK_FOREACH (int a, outer) {
        PK_FOREACH (int b, inner) {
            mixed += b;
        }
        if (a == 2) {
            break;
        }
    }
    PK_COMPARE(mixed, 60);
}

// ---------------------------------------------------------------------------
// 6. 空容器
// ---------------------------------------------------------------------------

void PkForeachTest::emptyContainerSkipsBody()
{
    int entered = 0;

    PkVector<int> emptyVec;
    PK_FOREACH (int x, emptyVec) {
        (void)x;
        ++entered;
    }

    PkList<int> emptyList;
    PK_FOREACH (int x, emptyList) {
        (void)x;
        ++entered;
    }

    PkMap<int, int> emptyMap;
    PK_FOREACH (int x, emptyMap) {
        (void)x;
        ++entered;
    }

    PkHash<int, int> emptyHash;
    PK_FOREACH (int x, emptyHash) {
        (void)x;
        ++entered;
    }

    PkSet<int> emptySet;
    PK_FOREACH (int x, emptySet) {
        (void)x;
        ++entered;
    }

    PK_COMPARE(entered, 0);

    // 空容器上跑一轮不该产生任何堆分配（空容器共享哨兵，拷贝也是 O(1)）
    const long before = g_pkAllocCount;
    PK_FOREACH (int x, emptyVec) {
        (void)x;
        ++entered;
    }
    const long after = g_pkAllocCount;
    PK_COMPARE(after - before, 0L);
    PK_COMPARE(entered, 0);
}

// ---------------------------------------------------------------------------
// 7. Qt 的两个拼法
// ---------------------------------------------------------------------------

void PkForeachTest::qtSpellingsWork()
{
    PkVector<int> v{1, 2, 3};

    // Q_FOREACH：保留范围 1543 处
    int sumUpper = 0;
    Q_FOREACH (int x, v) {
        sumUpper += x;
    }
    PK_COMPARE(sumUpper, 6);

    // foreach（小写关键字风格）：保留范围 101 处
    int sumLower = 0;
    foreach (int x, v) {
        sumLower += x;
    }
    PK_COMPARE(sumLower, 6);

    // 两个拼法混着嵌套也要对
    PkVector<int> w{10};
    int mixed = 0;
    Q_FOREACH (int a, v) {
        foreach (int b, w) {
            mixed += a + b;
        }
    }
    PK_COMPARE(mixed, 36);

    // break / continue 在 Qt 拼法下同样正确
    int broke = 0;
    foreach (int x, v) {
        if (x == 2) {
            break;
        }
        broke += x;
    }
    PK_COMPARE(broke, 1);
}

// ---------------------------------------------------------------------------
// qDeleteAll
// ---------------------------------------------------------------------------

void PkForeachTest::qDeleteAllDeletesEachElementOnce()
{
    PkDeleteProbe::s_destroyed = 0;

    PkVector<PkDeleteProbe *> v;
    v.append(new PkDeleteProbe(1));
    v.append(new PkDeleteProbe(2));
    v.append(new PkDeleteProbe(3));

    qDeleteAll(v);

    // 每个元素恰好 delete 一次
    PK_COMPARE(PkDeleteProbe::s_destroyed, 3);
    // **不清空容器** —— Qt 语义就是这样，调用点通常紧跟一个 clear()
    PK_COMPARE(v.size(), 3);
    v.clear();

    // PkList 上同样
    PkDeleteProbe::s_destroyed = 0;
    PkList<PkDeleteProbe *> l;
    l.append(new PkDeleteProbe(4));
    l.append(new PkDeleteProbe(5));
    qDeleteAll(l);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 2);
    PK_COMPARE(l.size(), 2);
    l.clear();

    // 空容器：什么都不做，也不该崩
    PkDeleteProbe::s_destroyed = 0;
    PkVector<PkDeleteProbe *> emptyVec;
    qDeleteAll(emptyVec);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 0);

    // qDeleteAll 收 const 引用 → 不 detach。共享态下调用后仍然共享。
    PkDeleteProbe::s_destroyed = 0;
    PkVector<PkDeleteProbe *> owner;
    owner.append(new PkDeleteProbe(6));
    PkVector<PkDeleteProbe *> alias(owner);
    PK_VERIFY(owner.PkIsSharedWith(alias));
    qDeleteAll(owner);
    PK_VERIFY(owner.PkIsSharedWith(alias));
    PK_COMPARE(PkDeleteProbe::s_destroyed, 1);
    owner.clear();
    alias.clear();
}

void PkForeachTest::qDeleteAllIteratorRangeOverload()
{
    // 双实参重载的唯一真实调用点形态：
    //   plugins/paintops/hairy/hairy_brush.cpp
    //     qDeleteAll(m_bristles.begin(), m_bristles.end());
    // 注意实参是**非 const** 容器上的 begin()/end()（会 detach），照抄这个形态。
    PkDeleteProbe::s_destroyed = 0;

    PkVector<PkDeleteProbe *> v;
    v.append(new PkDeleteProbe(1));
    v.append(new PkDeleteProbe(2));

    qDeleteAll(v.begin(), v.end());
    PK_COMPARE(PkDeleteProbe::s_destroyed, 2);
    PK_COMPARE(v.size(), 2);
    v.clear();

    // const 迭代器版也要能编（constBegin/constEnd）
    PkDeleteProbe::s_destroyed = 0;
    PkVector<PkDeleteProbe *> w;
    w.append(new PkDeleteProbe(3));
    qDeleteAll(w.constBegin(), w.constEnd());
    PK_COMPARE(PkDeleteProbe::s_destroyed, 1);
    w.clear();

    // 只删一段（区间语义真的是区间，不是"整个容器"）
    PkDeleteProbe::s_destroyed = 0;
    PkVector<PkDeleteProbe *> part;
    part.append(new PkDeleteProbe(1));
    part.append(new PkDeleteProbe(2));
    part.append(new PkDeleteProbe(3));
    qDeleteAll(part.constBegin(), part.constBegin() + 2);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 2);
    delete part.at(2);
    part.clear();
}

void PkForeachTest::qDeleteAllOnAssociativeContainers()
{
    // 关联容器上 `*it` 是 value，所以删的是 value 那一侧的指针（Qt 一致）。
    PkDeleteProbe::s_destroyed = 0;
    PkMap<int, PkDeleteProbe *> map;
    map.insert(1, new PkDeleteProbe(1));
    map.insert(2, new PkDeleteProbe(2));
    qDeleteAll(map);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 2);
    PK_COMPARE(map.size(), 2);
    map.clear();

    PkDeleteProbe::s_destroyed = 0;
    PkHash<int, PkDeleteProbe *> hash;
    hash.insert(1, new PkDeleteProbe(1));
    hash.insert(2, new PkDeleteProbe(2));
    hash.insert(3, new PkDeleteProbe(3));
    qDeleteAll(hash);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 3);
    PK_COMPARE(hash.size(), 3);
    hash.clear();

    // PkSet 上元素本身就是指针
    PkDeleteProbe::s_destroyed = 0;
    PkSet<PkDeleteProbe *> set;
    set.insert(new PkDeleteProbe(1));
    set.insert(new PkDeleteProbe(2));
    qDeleteAll(set);
    PK_COMPARE(PkDeleteProbe::s_destroyed, 2);
    PK_COMPARE(set.size(), 2);
    set.clear();
}

PK_TEST_MAIN(PkForeachTest)
