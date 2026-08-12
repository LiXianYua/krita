#include "PkStackTest.h"

#include "../PkStack.h"

#include "PkSeqTestShared.h"

#include <string>
#include <type_traits>
#include <utility>

// PkTestBinder<PkStackTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkStackTest> 实例化前对本 TU
// 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkStackTest.inc"

namespace {

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntStack = PkStack<int>;

// ---- 契约的编译期部分（签名形状，不是行为）----

// **PkStack 必须是派生类，不是 typedef**：写成 typedef 的话每个 PkVector<int>
// 都会长出 push/pop/top。这两条一起把它钉死。
static_assert(std::is_base_of<PkVector<int>, IntStack>::value,
              "PkStack 必须派生自 PkVector");
static_assert(!std::is_same<IntStack, PkVector<int>>::value,
              "PkStack 不能是 PkVector 的 typedef");

// pop() 按值返回 T（Qt 的签名），不是 void。
static_assert(std::is_same<decltype(std::declval<IntStack &>().pop()), int>::value,
              "pop() 必须按值返回 T");

// top() 两个版本：非 const 返回 T&，const 返回 const T&。
static_assert(std::is_same<decltype(std::declval<IntStack &>().top()), int &>::value,
              "非 const top() 必须返回 T&");
static_assert(std::is_same<decltype(std::declval<const IntStack &>().top()), const int &>::value,
              "const top() 必须返回 const T&");

// **链式操作符必须返回 PkStack&，不是 PkVector&**（协变返回类型的坑）。
// 基类的 operator<< 返回 Derived& == PkVector<int>&，不重新声明就会退化。
static_assert(std::is_same<decltype(std::declval<IntStack &>() << 1), IntStack &>::value,
              "operator<<(T) 必须返回 PkStack&，否则链式调用后类型退化");
static_assert(std::is_same<decltype(std::declval<IntStack &>() += 1), IntStack &>::value,
              "operator+=(T) 必须返回 PkStack&");

// 刻意**不**提供 PkVector → PkStack 的隐式转换（Qt 也没有）。
static_assert(!std::is_convertible<PkVector<int>, IntStack>::value,
              "不该存在 PkVector → PkStack 的隐式转换（Qt 没有这条）");

} // namespace

// ---------------------------------------------------------------------------
// 共同 API：逐条在 PkStack 上实例化 tests/PkSeqTestShared.h 里的用例。
//
// 这一整块**不是抄的**——同一份模板已经在 PkVector 与 PkList 上各跑过一遍，
// 这里是第三个实例化。基类语义（COW、迭代器、比较、移动、reserve 的 detach
// 规则）由它整体覆盖，本文件下半部分只补 PkStack 自己新增的那几格。
// ---------------------------------------------------------------------------

void PkStackTest::sizeIsInt() { pkSeqTestSizeIsInt<PkStack>(); }
void PkStackTest::sizeAndEmptiness() { pkSeqTestSizeAndEmptiness<PkStack>(); }
void PkStackTest::elementAccess() { pkSeqTestElementAccess<PkStack>(); }
void PkStackTest::valueOutOfRange() { pkSeqTestValueOutOfRange<PkStack>(); }
void PkStackTest::appendAndPrepend() { pkSeqTestAppendAndPrepend<PkStack>(); }
void PkStackTest::insertAndRemove() { pkSeqTestInsertAndRemove<PkStack>(); }
void PkStackTest::erase() { pkSeqTestErase<PkStack>(); }
void PkStackTest::search() { pkSeqTestSearch<PkStack>(); }
void PkStackTest::iterators() { pkSeqTestIterators<PkStack>(); }
void PkStackTest::constIteratorsDoNotDetach() { pkSeqTestConstIteratorsDoNotDetach<PkStack>(); }
void PkStackTest::comparison() { pkSeqTestComparison<PkStack>(); }
void PkStackTest::streamOperators() { pkSeqTestStreamOperators<PkStack>(); }
void PkStackTest::cowIsolation() { pkSeqTestCowIsolation<PkStack>(); }
void PkStackTest::copyIsConstantTime() { pkSeqTestCopyIsConstantTime<PkStack>(); }
void PkStackTest::constNeverDetaches() { pkSeqTestConstNeverDetaches<PkStack>(); }
void PkStackTest::everyWriterDetaches() { pkSeqTestEveryWriterDetaches<PkStack>(); }
void PkStackTest::reserveDetachRules() { pkSeqTestReserveDetachRules<PkStack>(); }
void PkStackTest::swap() { pkSeqTestSwap<PkStack>(); }
void PkStackTest::selfAssignment() { pkSeqTestSelfAssignment<PkStack>(); }
void PkStackTest::moveLeavesSourceUsable() { pkSeqTestMoveLeavesSourceUsable<PkStack>(); }
void PkStackTest::initializerListAndDefaults() { pkSeqTestInitializerListAndDefaults<PkStack>(); }

// ---------------------------------------------------------------------------
// PkStack 专有
// ---------------------------------------------------------------------------

void PkStackTest::lifoOrder()
{
    // 实测（真 Qt 5.15.7）：
    //   push 1,2,3 → size=3 top=3；pop()→3（剩 2）；pop()→2（剩 1）  = LIFO
    //
    // **每次 pop() 单独一条语句、先存进具名变量再断言。**
    // `printf("%d %d", st.pop(), st.pop())` 里两个 pop 的求值顺序在 C++ 里是
    // 未指定的（GCC 从右到左），那样写会得到一个假的失败——或者更糟，一个假的
    // 成功。这个坑在写对齐探针时真踩过。
    PkStack<int> st;
    st.push(1);
    st.push(2);
    st.push(3);
    PK_COMPARE(st.size(), 3);
    PK_COMPARE(st.top(), 3);

    const int first = st.pop();
    PK_COMPARE(first, 3);
    PK_COMPARE(st.size(), 2);
    PK_COMPARE(st.top(), 2);

    const int second = st.pop();
    PK_COMPARE(second, 2);
    PK_COMPARE(st.size(), 1);
    PK_COMPARE(st.top(), 1);

    const int third = st.pop();
    PK_COMPARE(third, 1);
    PK_COMPARE(st.size(), 0);
    PK_VERIFY(st.isEmpty());

    // push 的右值重载
    PkStack<std::string> s;
    s.push(std::string("a"));
    s.push(std::string("b"));
    const std::string popped = s.pop();
    PK_COMPARE(popped, std::string("b"));
    PK_COMPARE(s.size(), 1);

    // 栈底就是最先 push 进去的那个（顺序不是靠 top 一个点证明的）
    PkStack<int> deep;
    for (int i = 0; i < 5; ++i) {
        deep.push(i);
    }
    PK_COMPARE(deep.at(0), 0);
    PK_COMPARE(deep.at(4), 4);
    for (int i = 4; i >= 0; --i) {
        const int v = deep.pop();
        PK_COMPARE(v, i);
    }
    PK_VERIFY(deep.isEmpty());
}

void PkStackTest::topConstAndMutable()
{
    PkStack<int> st{1, 2, 3};

    // 非 const top() 返回可写引用：就着它改内容要真的改到
    st.top() = 99;
    PK_COMPARE(st.top(), 99);
    PK_COMPARE(st.at(2), 99);

    // const top() 返回 const 引用，且读不 detach
    PkStack<int> shared(st);
    const PkStack<int> &cst = shared;
    PK_COMPARE(cst.top(), 99);
    PK_VERIFY(st.PkIsSharedWith(shared));
}

void PkStackTest::stackWritersDetach()
{
    // **本任务最容易漏的一格**：基类方法 Task 2 已经验过了，push/pop 是派生类
    // 新增的写入口，各自都要走 PkMut()。漏一个就是共享的两个栈互相污染。
    //
    // 用共享的数据驱动表跑：`PkSeqCowCase<PkStack>` 与 PkVector/PkList 那两张
    // 表同型，只是这里只登记 PkStack 新增的方法。
    using S = PkStack<int>;

    static const PkSeqCowCase<PkStack> cases[] = {
        {"push", [](S &s) { s.push(9); }, true},
        {"pop", [](S &s) { (void)s.pop(); }, true},
        {"top()（非 const，可写引用）", [](S &s) { s.top() = 7; }, true},
        {"operator<<", [](S &s) { s << 5; }, true},
        {"operator+=", [](S &s) { s += 5; }, true},
    };

    pkSeqRunCowCases<PkStack>(cases);

    // pop() 除了 detach，还要把值正确地取出来、另一边一个字节不变
    PkStack<int> a{1, 2, 3};
    PkStack<int> b(a);
    const int v = b.pop();
    PK_COMPARE(v, 3);
    PK_VERIFY(!a.PkIsSharedWith(b));
    PK_VERIFY((a == PkStack<int>{1, 2, 3}));
    PK_VERIFY((b == PkStack<int>{1, 2}));

    // const top() 是读路径，**不得** detach
    PkStack<int> c{1, 2, 3};
    PkStack<int> d(c);
    const PkStack<int> &cd = d;
    PK_COMPARE(cd.top(), 3);
    PK_VERIFY(c.PkIsSharedWith(d));
}

void PkStackTest::chainedOperatorsKeepDerivedType()
{
    // 编得过就是证明：链式 << 之后仍是 PkStack，能直接调 PkStack 专有的 top()。
    // 不重新声明 operator<< 的话，第一个 << 就返回 PkVector<int>&，这里编不过。
    PkStack<int> s;
    PK_COMPARE((s << 1 << 2 << 3).top(), 3);
    PK_COMPARE(s.size(), 3);

    PkStack<int> t;
    PK_COMPARE((t += 5).top(), 5);

    // 列表版重载同样保持类型
    PkStack<int> u{1};
    PkVector<int> more{2, 3};
    PK_COMPARE((u << more).top(), 3);
    PK_COMPARE(u.size(), 3);

    PkStack<int> w{1};
    PK_COMPARE((w += more).top(), 3);

    // 链式之后 pop 顺序照常是 LIFO（不是只有类型对、行为跑偏了）
    PkStack<int> chain;
    chain << 10 << 20;
    const int top1 = chain.pop();
    PK_COMPARE(top1, 20);
    const int top2 = chain.pop();
    PK_COMPARE(top2, 10);
}

PK_TEST_MAIN(PkStackTest)
