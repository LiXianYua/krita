#include "PkQueueTest.h"

#include "../PkQueue.h"

#include "PkSeqTestShared.h"

#include <string>
#include <type_traits>
#include <utility>

#include "pk_binder_PkQueueTest.inc"

namespace {

using IntQueue = PkQueue<int>;

// ---- 契约的编译期部分（形状与 test_pkstack.cpp 对称，理由见那里）----

static_assert(std::is_base_of<PkList<int>, IntQueue>::value,
              "PkQueue 必须派生自 PkList");
static_assert(!std::is_same<IntQueue, PkList<int>>::value,
              "PkQueue 不能是 PkList 的 typedef");

static_assert(std::is_same<decltype(std::declval<IntQueue &>().dequeue()), int>::value,
              "dequeue() 必须按值返回 T");
static_assert(std::is_same<decltype(std::declval<IntQueue &>().head()), int &>::value,
              "非 const head() 必须返回 T&");
static_assert(std::is_same<decltype(std::declval<const IntQueue &>().head()), const int &>::value,
              "const head() 必须返回 const T&");

static_assert(std::is_same<decltype(std::declval<IntQueue &>() << 1), IntQueue &>::value,
              "operator<<(T) 必须返回 PkQueue&，否则链式调用后类型退化");
static_assert(std::is_same<decltype(std::declval<IntQueue &>() += 1), IntQueue &>::value,
              "operator+=(T) 必须返回 PkQueue&");

static_assert(!std::is_convertible<PkList<int>, IntQueue>::value,
              "不该存在 PkList → PkQueue 的隐式转换（Qt 没有这条）");

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：在 PkQueue 上实例化 tests/PkSeqTestShared.h（第四个实例化）
// ---------------------------------------------------------------------------

void PkQueueTest::sizeIsInt() { pkSeqTestSizeIsInt<PkQueue>(); }
void PkQueueTest::sizeAndEmptiness() { pkSeqTestSizeAndEmptiness<PkQueue>(); }
void PkQueueTest::elementAccess() { pkSeqTestElementAccess<PkQueue>(); }
void PkQueueTest::valueOutOfRange() { pkSeqTestValueOutOfRange<PkQueue>(); }
void PkQueueTest::appendAndPrepend() { pkSeqTestAppendAndPrepend<PkQueue>(); }
void PkQueueTest::insertAndRemove() { pkSeqTestInsertAndRemove<PkQueue>(); }
void PkQueueTest::erase() { pkSeqTestErase<PkQueue>(); }
void PkQueueTest::search() { pkSeqTestSearch<PkQueue>(); }
void PkQueueTest::iterators() { pkSeqTestIterators<PkQueue>(); }
void PkQueueTest::constIteratorsDoNotDetach() { pkSeqTestConstIteratorsDoNotDetach<PkQueue>(); }
void PkQueueTest::comparison() { pkSeqTestComparison<PkQueue>(); }
void PkQueueTest::streamOperators() { pkSeqTestStreamOperators<PkQueue>(); }
void PkQueueTest::cowIsolation() { pkSeqTestCowIsolation<PkQueue>(); }
void PkQueueTest::copyIsConstantTime() { pkSeqTestCopyIsConstantTime<PkQueue>(); }
void PkQueueTest::constNeverDetaches() { pkSeqTestConstNeverDetaches<PkQueue>(); }
void PkQueueTest::everyWriterDetaches() { pkSeqTestEveryWriterDetaches<PkQueue>(); }
void PkQueueTest::reserveDetachRules() { pkSeqTestReserveDetachRules<PkQueue>(); }
void PkQueueTest::swap() { pkSeqTestSwap<PkQueue>(); }
void PkQueueTest::selfAssignment() { pkSeqTestSelfAssignment<PkQueue>(); }
void PkQueueTest::moveLeavesSourceUsable() { pkSeqTestMoveLeavesSourceUsable<PkQueue>(); }
void PkQueueTest::initializerListAndDefaults() { pkSeqTestInitializerListAndDefaults<PkQueue>(); }

// ---------------------------------------------------------------------------
// PkQueue 专有
// ---------------------------------------------------------------------------

void PkQueueTest::fifoOrder()
{
    // 实测（真 Qt 5.15.7）：
    //   enqueue 1,2,3 → size=3 head=1；dequeue()→1（剩 2）；dequeue()→2（剩 1）= FIFO
    //
    // 与 PkStack 同一个坑：**每次 dequeue() 单独一条语句、先存具名变量再断言**，
    // 一条语句里调两次的求值顺序是未指定的。
    PkQueue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);
    PK_COMPARE(q.size(), 3);
    PK_COMPARE(q.head(), 1);

    const int first = q.dequeue();
    PK_COMPARE(first, 1);
    PK_COMPARE(q.size(), 2);
    PK_COMPARE(q.head(), 2);

    const int second = q.dequeue();
    PK_COMPARE(second, 2);
    PK_COMPARE(q.size(), 1);
    PK_COMPARE(q.head(), 3);

    const int third = q.dequeue();
    PK_COMPARE(third, 3);
    PK_VERIFY(q.isEmpty());

    // enqueue 的右值重载
    PkQueue<std::string> s;
    s.enqueue(std::string("a"));
    s.enqueue(std::string("b"));
    const std::string out = s.dequeue();
    PK_COMPARE(out, std::string("a"));
    PK_COMPARE(s.size(), 1);

    // 出队顺序与入队顺序一致（不是靠 head 一个点证明的）
    PkQueue<int> deep;
    for (int i = 0; i < 5; ++i) {
        deep.enqueue(i);
    }
    PK_COMPARE(deep.at(0), 0);
    PK_COMPARE(deep.at(4), 4);
    for (int i = 0; i < 5; ++i) {
        const int v = deep.dequeue();
        PK_COMPARE(v, i);
    }
    PK_VERIFY(deep.isEmpty());
}

void PkQueueTest::headConstAndMutable()
{
    PkQueue<int> q{1, 2, 3};

    q.head() = 99;
    PK_COMPARE(q.head(), 99);
    PK_COMPARE(q.at(0), 99);

    PkQueue<int> shared(q);
    const PkQueue<int> &cq = shared;
    PK_COMPARE(cq.head(), 99);
    PK_VERIFY(q.PkIsSharedWith(shared));
}

void PkQueueTest::queueWritersDetach()
{
    // 与 PkStack 同因：enqueue/dequeue 是派生类新增的写入口，各自都要走 PkMut()。
    using Q = PkQueue<int>;

    static const PkSeqCowCase<PkQueue> cases[] = {
        {"enqueue", [](Q &s) { s.enqueue(9); }, true},
        {"dequeue", [](Q &s) { (void)s.dequeue(); }, true},
        {"head()（非 const，可写引用）", [](Q &s) { s.head() = 7; }, true},
        {"operator<<", [](Q &s) { s << 5; }, true},
        {"operator+=", [](Q &s) { s += 5; }, true},
    };

    pkSeqRunCowCases<PkQueue>(cases);

    PkQueue<int> a{1, 2, 3};
    PkQueue<int> b(a);
    const int v = b.dequeue();
    PK_COMPARE(v, 1);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY((a == PkQueue<int>{1, 2, 3}));
    PK_VERIFY((b == PkQueue<int>{2, 3}));

    // const head() 是读路径，**不得** detach
    PkQueue<int> c{1, 2, 3};
    PkQueue<int> d(c);
    const PkQueue<int> &cd = d;
    PK_COMPARE(cd.head(), 1);
    PK_VERIFY(c.PkIsSharedWith(d));
}

void PkQueueTest::chainedOperatorsKeepDerivedType()
{
    // 编得过就是证明：链式之后仍是 PkQueue，能直接调专有的 head()。
    PkQueue<int> q;
    PK_COMPARE((q << 1 << 2 << 3).head(), 1);
    PK_COMPARE(q.size(), 3);

    PkQueue<int> t;
    PK_COMPARE((t += 5).head(), 5);

    PkQueue<int> u{1};
    PkList<int> more{2, 3};
    PK_COMPARE((u << more).head(), 1);
    PK_COMPARE(u.size(), 3);

    PkQueue<int> w{1};
    PK_COMPARE((w += more).head(), 1);

    // 链式之后出队顺序照常是 FIFO
    PkQueue<int> chain;
    chain << 10 << 20;
    const int h1 = chain.dequeue();
    PK_COMPARE(h1, 10);
    const int h2 = chain.dequeue();
    PK_COMPARE(h2, 20);
}

PK_TEST_MAIN(PkQueueTest)
