#include "PkPairTest.h"

#include "../PkPair.h"
#include "../PkVector.h"

#include <string>
#include <type_traits>
#include <utility>

#include "pk_binder_PkPairTest.inc"

namespace {

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
using IntPair = PkPair<int, int>;
using MixedPair = PkPair<std::string, int>;

// ---- 契约的编译期部分 ----

// 别名不是新类型（这是选「别名」而不是「自写模板」的全部收益：与标准库天然互通）
static_assert(std::is_same<IntPair, std::pair<int, int>>::value,
              "PkPair<A,B> 必须就是 std::pair<A,B>");

// qMakePair 推导出的类型
static_assert(std::is_same<decltype(qMakePair(1, 2)), IntPair>::value,
              "qMakePair(int,int) 必须返回 PkPair<int,int>");

} // namespace

void PkPairTest::firstSecondAccess()
{
    IntPair p(1, 2);
    PK_COMPARE(p.first, 1);
    PK_COMPARE(p.second, 2);

    // 可写
    p.first = 10;
    p.second = 20;
    PK_COMPARE(p.first, 10);
    PK_COMPARE(p.second, 20);

    // 非平凡类型
    MixedPair m(std::string("abc"), 7);
    PK_COMPARE(m.first, std::string("abc"));
    PK_COMPARE(m.second, 7);

    // 默认构造：两个成员都值初始化
    IntPair d;
    PK_COMPARE(d.first, 0);
    PK_COMPARE(d.second, 0);
}

void PkPairTest::comparisonIsLexicographic()
{
    // 实测（真 Qt 5.15.7 的 QPair）：
    //   (1,2)<(1,3) 真 · (1,2)<(2,0) 真 · (1,2)==(1,2) 真 · qMakePair(1,2)==(1,2) 真
    //     = 字典序
    // 逐条对上——**不假设** std::pair 与 QPair 同口径，这里就是在证明它。

    // first 相等，比 second
    PK_VERIFY(IntPair(1, 2) < IntPair(1, 3));
    PK_VERIFY(!(IntPair(1, 3) < IntPair(1, 2)));

    // first 不等时 second 完全不参与（(1,2) < (2,0) 为真，尽管 2 > 0）
    PK_VERIFY(IntPair(1, 2) < IntPair(2, 0));
    PK_VERIFY(!(IntPair(2, 0) < IntPair(1, 2)));

    // 相等
    PK_VERIFY(IntPair(1, 2) == IntPair(1, 2));
    PK_VERIFY(!(IntPair(1, 2) != IntPair(1, 2)));
    PK_VERIFY(!(IntPair(1, 2) < IntPair(1, 2)));

    // qMakePair 的结果与直接构造的相等
    PK_VERIFY(qMakePair(1, 2) == IntPair(1, 2));

    // 另外四个运算符也都在，且与 < / == 自洽
    PK_VERIFY(IntPair(1, 2) <= IntPair(1, 2));
    PK_VERIFY(IntPair(1, 2) <= IntPair(1, 3));
    PK_VERIFY(IntPair(1, 3) > IntPair(1, 2));
    PK_VERIFY(IntPair(1, 2) >= IntPair(1, 2));
    PK_VERIFY(IntPair(2, 0) >= IntPair(1, 9));
    PK_VERIFY(IntPair(1, 2) != IntPair(1, 3));

    // 非平凡类型上同样是字典序
    PK_VERIFY(MixedPair(std::string("a"), 9) < MixedPair(std::string("b"), 0));
    PK_VERIFY(MixedPair(std::string("a"), 1) < MixedPair(std::string("a"), 2));
}

void PkPairTest::qMakePairDeducesTypes()
{
    const IntPair p = qMakePair(3, 4);
    PK_COMPARE(p.first, 3);
    PK_COMPARE(p.second, 4);

    // 两个类型不同
    const MixedPair m = qMakePair(std::string("k"), 5);
    PK_COMPARE(m.first, std::string("k"));
    PK_COMPARE(m.second, 5);

    // 从左值推导（按值取，不是引用——改原变量不该影响 pair）
    int a = 1;
    int b = 2;
    IntPair q = qMakePair(a, b);
    a = 100;
    b = 200;
    PK_COMPARE(q.first, 1);
    PK_COMPARE(q.second, 2);
}

void PkPairTest::aliasIsStdPair()
{
    // 别名的实际收益：与标准库产物可以直接互相赋值，不需要任何转换
    std::pair<int, int> s(1, 2);
    IntPair p = s;
    PK_VERIFY(p == s);

    IntPair q = qMakePair(7, 8);
    std::pair<int, int> t = q;
    PK_COMPARE(t.first, 7);
    PK_COMPARE(t.second, 8);

    // std::make_pair 与 qMakePair 的结果同型、可比较
    PK_VERIFY(std::make_pair(1, 2) == qMakePair(1, 2));
}

void PkPairTest::worksInsideContainers()
{
    // PkVector<PkPair<A,B>> 是真实用法；PkArrayData.cpp 已经为
    // std::vector<std::pair<int,int>> 放了一条显式实例化。
    PkVector<IntPair> v;
    v.append(qMakePair(1, 2));
    v.append(qMakePair(3, 4));
    PK_COMPARE(v.size(), 2);
    PK_COMPARE(v.at(0).first, 1);
    PK_COMPARE(v.at(1).second, 4);

    // 容器的 COW 照常（元素是 pair 不改变任何事）
    PkVector<IntPair> w(v);
    PK_VERIFY(v.PkIsSharedWith(w));
    w.append(qMakePair(5, 6));
    PK_VERIFY(!v.PkIsSharedWith(w));
    PK_COMPARE(v.size(), 2);
    PK_COMPARE(w.size(), 3);

    // 元素比较用得上 pair 的 operator==（容器的 operator== 靠它）
    PkVector<IntPair> x;
    x.append(qMakePair(1, 2));
    x.append(qMakePair(3, 4));
    PK_VERIFY(v == x);

    // indexOf 也走 operator==
    PK_COMPARE(v.indexOf(qMakePair(3, 4)), 1);
    PK_COMPARE(v.indexOf(qMakePair(9, 9)), -1);
}

PK_TEST_MAIN(PkPairTest)
