#include "cases/point_case.h"
#include "../PkPoint.h"
#include "point_macro_proof.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// PkTestBinder<PkPointCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_global.cpp 相同：显式特化必须在 qExec<T>
// 实例化前对本 TU 可见）。
#include "pk_binder_point_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core，
// QT_VERSION_STR "5.15.7"），不是「四舍五入」「点相等就是坐标相等」这类直觉。
// 对齐口径：与 Qt 的任何行为差异默认都是缺陷，所以 Qt 那些看着像 bug 的地方
// （负半值取整方向、inf==inf 为假、-0.0 算 null）也一起钉住。
//
// ⚠ **PK_COMPARE 对 double 走的是 pk/test 的模糊比较（相对 1e-12），不是位相等**
//（R-11 harness 的能力边界）。凡是主张「与 Qt 逐位一致」的地方一律不能用它，
// 要用下面的 sameBits / std::signbit 直接查位模式。
// ---------------------------------------------------------------------------

namespace {

// 位精确比 double：`==` 会把 +0/-0 判等、把 NaN 判不等，两者都不是我们要的。
bool sameBits(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);   // 两侧都是 NaN 就算同（载荷可以不同）
}

// 挡住常量折叠。**越界的 double→int 转换（`int(inf)`、`int(2147483648.0)`）是 UB，
// 编译期折叠与运行期 `cvttsd2si` 给的答案不一样**：整条表达式都是字面量时
// `-O1` 及以上会在编译期算，实测把
// `PkPointF(2147483648.0,2147483648.0).toPoint() == PkPoint(INT_MIN,INT_MIN)`
// 判成 false（`-O0` 为 true）。真 Qt 侧的调用点拿到的是运行期算出来的值，
// 所以要对齐的也是运行期那条路径 —— 把输入经一次 volatile 读送进去，转换就一定
// 发生在运行期。不这么做，这批断言事实上把整套单测锁死在 `-O0`。
//
// 探针实测（真 Qt 5.15.7，同一个 volatile 屏障，`-O0` 与 `-O2` 逐字一致）：
//   2147483648.0/1e308/+inf/4294967296.0 → INT_MIN；-1e308/-inf/nan → 0。
double noFold(double d)
{
    volatile double v = d;
    return v;
}

const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

} // namespace

// ═══ PkPoint ═══════════════════════════════════════════════════════════════

void PkPointCase::pointConstruction()
{
    const PkPoint d;
    PK_COMPARE(d.x(), 0);
    PK_COMPARE(d.y(), 0);

    const PkPoint p(3, -4);
    PK_COMPARE(p.x(), 3);
    PK_COMPARE(p.y(), -4);

    // 实测 sizeof(QPoint)==8：两个 int，没有额外成员/虚表。
    PK_COMPARE(sizeof(PkPoint), sizeof(int) * 2);
    PK_VERIFY(std::is_trivially_copyable<PkPoint>::value);
    PK_VERIFY((std::is_same<decltype(p.x()), int>::value));
}

void PkPointCase::pointAccessorsAndReferences()
{
    PkPoint p(1, 2);
    p.setX(10);
    PK_COMPARE(p.x(), 10);
    p.setY(-20);
    PK_COMPARE(p.y(), -20);

    // rx()/ry() 返回**可写引用**，不是副本。实测真 Qt：
    // QPoint(1,2) 上 rx()+=10、ry()*=3 得 (11,6)。
    PkPoint q(1, 2);
    q.rx() += 10;
    q.ry() *= 3;
    PK_COMPARE(q.x(), 11);
    PK_COMPARE(q.y(), 6);
    PK_VERIFY((std::is_same<decltype(q.rx()), int &>::value));
    PK_VERIFY((std::is_same<decltype(q.ry()), int &>::value));
    // 引用真的指向本对象的两个分量（探针实测两者相距 4 字节，即相邻的两个 int）
    PK_VERIFY(&q.rx() == &q.rx());
    PK_VERIFY(reinterpret_cast<char *>(&q.ry()) - reinterpret_cast<char *>(&q.rx())
              == static_cast<std::ptrdiff_t>(sizeof(int)));
}

void PkPointCase::pointIsNull()
{
    PK_VERIFY(PkPoint().isNull());
    PK_VERIFY(PkPoint(0, 0).isNull());
    PK_VERIFY(!PkPoint(0, 1).isNull());
    PK_VERIFY(!PkPoint(1, 0).isNull());
    PK_VERIFY(!PkPoint(-1, -1).isNull());
}

void PkPointCase::pointManhattanLength()
{
    // 实测 QPoint(-3,4).manhattanLength() == 7
    PK_COMPARE(PkPoint(-3, 4).manhattanLength(), 7);
    PK_COMPARE(PkPoint(3, 4).manhattanLength(), 7);
    PK_COMPARE(PkPoint(-3, -4).manhattanLength(), 7);
    PK_COMPARE(PkPoint(0, 0).manhattanLength(), 0);

    // ⚠ **Qt 不防溢出**，这里钉的是实测取值而不是"数学上对的值"。
    // 真 Qt 5.15.7 实测：INT_MAX,INT_MAX → -2；INT_MIN,INT_MIN → 0；
    // INT_MIN,0 → INT_MIN（qAbs(INT_MIN) 回绕成 INT_MIN）。
    // 顺手"修正"成 64 位求和是行为差异，必须被这三条拦下。
    PK_COMPARE(PkPoint(INT_MAX, INT_MAX).manhattanLength(), -2);
    PK_COMPARE(PkPoint(INT_MIN, INT_MIN).manhattanLength(), 0);
    PK_COMPARE(PkPoint(INT_MIN, 0).manhattanLength(), INT_MIN);
}

void PkPointCase::pointAdditiveOperators()
{
    const PkPoint a(3, -4), b(-1, 6);
    PK_VERIFY(a + b == PkPoint(2, 2));
    PK_VERIFY(a - b == PkPoint(4, -10));
    PK_VERIFY(-a == PkPoint(-3, 4));
    PK_VERIFY(+a == a);           // 一元 + 原样返回

    PkPoint c(3, -4);
    c += b;
    PK_VERIFY(c == PkPoint(2, 2));
    c -= b;
    PK_VERIFY(c == a);

    // 加法同样不防溢出：实测 QPoint(INT_MAX,1)+QPoint(1,1) == (INT_MIN,2)
    PK_VERIFY(PkPoint(INT_MAX, 1) + PkPoint(1, 1) == PkPoint(INT_MIN, 2));
    // 一元负号在 INT_MIN 上原样回绕：实测 -QPoint(INT_MIN,INT_MIN) == (INT_MIN,INT_MIN)
    PK_VERIFY(-PkPoint(INT_MIN, INT_MIN) == PkPoint(INT_MIN, INT_MIN));
}

void PkPointCase::pointScalingRoundsLikeQt()
{
    // ⚠ 取整方向是本任务最容易写错的一条：qRound 对负半值**向 +∞**，
    // 不是"远离零"也不是"截断"。下面每一行都是真 Qt 5.15.7 的实测输出。
    //   1*0.5=0.5  → 1     -1*0.5=-0.5 → 0
    //   3*0.5=1.5  → 2     -3*0.5=-1.5 → -1
    //   5*0.5=2.5  → 3     -5*0.5=-2.5 → -2
    //   7*0.5=3.5  → 4     -7*0.5=-3.5 → -3
    PK_VERIFY(PkPoint(1, 1) * 0.5 == PkPoint(1, 1));
    PK_VERIFY(PkPoint(-1, -1) * 0.5 == PkPoint(0, 0));
    PK_VERIFY(PkPoint(3, 3) * 0.5 == PkPoint(2, 2));
    PK_VERIFY(PkPoint(-3, -3) * 0.5 == PkPoint(-1, -1));
    PK_VERIFY(PkPoint(5, 5) * 0.5 == PkPoint(3, 3));
    PK_VERIFY(PkPoint(-5, -5) * 0.5 == PkPoint(-2, -2));
    PK_VERIFY(PkPoint(-7, -7) * 0.5 == PkPoint(-3, -3));

    // ⚠ **半值用例单独一组是不够的**：把 qRound 换成 `int(v*f + 0.5)`（= 丢掉负
    // 分支、"半值远离零"那种直觉写法）时，上面每一条的取值都**恰好不变**
    //（-1*0.5=-0.5 → int(0)=0 ✓；-3*0.5=-1.5 → int(-1)=-1 ✓），28 条单测全绿，
    // 只有对拍抓得到。分辨这两种实现要靠**负数非半值**：小数部分不是 ±0.5 时
    // 两条公式才分家。下面三条实测真 Qt 5.15.7（-O0/-O2 一致），
    // `int(v*f+0.5)` 变体分别给 0 / -1 / -1，逐条冲突。
    PK_VERIFY(PkPoint(-3, -3) * 0.25 == PkPoint(-1, -1));    // -0.75 → -1（变体给 0）
    PK_VERIFY(PkPoint(-3, -3) * 0.75 == PkPoint(-2, -2));    // -2.25 → -2（变体给 -1）
    PK_VERIFY(PkPoint(-7, -7) * 0.25 == PkPoint(-2, -2));    // -1.75 → -2（变体给 -1）
    PK_VERIFY(PkPoint(3, 3) * 0.25 == PkPoint(1, 1));        // 正侧对照：0.75 → 1
    PK_VERIFY(0.25 * PkPoint(-3, -3) == PkPoint(-1, -1));    // 左乘同语义
    PK_VERIFY(PkPoint(-3, -3) / 4.0 == PkPoint(-1, -1));     // 除法走同一条 qRound
    { PkPoint n(-3, -3); n *= 0.25; PK_VERIFY(n == PkPoint(-1, -1)); }
    { PkPoint n(-3, -3); n /= 4.0;  PK_VERIFY(n == PkPoint(-1, -1)); }

    // 左乘与右乘同语义
    PK_VERIFY(0.5 * PkPoint(-5, -5) == PkPoint(-2, -2));
    PK_VERIFY(0.5 * PkPoint(5, 5) == PkPoint(3, 3));

    // 除法走同一条 qRound
    PK_VERIFY(PkPoint(-3, -3) / 2.0 == PkPoint(-1, -1));
    PK_VERIFY(PkPoint(-7, -7) / 2.0 == PkPoint(-3, -3));
    PK_VERIFY(PkPoint(7, 7) / 2.0 == PkPoint(4, 4));

    // 复合赋值与二元形式必须一致（Qt 是两份独立代码，抄漏一份就在这里现形）
    PkPoint m(-3, -3); m *= 0.5;
    PK_VERIFY(m == PkPoint(-1, -1));
    PkPoint d(-3, -3); d /= 2.0;
    PK_VERIFY(d == PkPoint(-1, -1));
}

void PkPointCase::pointFloatOverloadIsReallyFloat()
{
    // float 与 double 两个重载**不是摆设**：0.49999997f 按 float 精度做
    // `xp*factor + 0.5f` 会进位到 1，提升到 double 之后不会。
    // 实测真 Qt：QPoint(1,1)*0.49999997f == (1,1)，同一数值走 double == (0,0)。
    // 把两个重载合并成一个 double 版，这两条里的第一条立刻变红。
    const float f = 0.49999997f;
    const double d = f;
    PK_VERIFY(PkPoint(1, 1) * f == PkPoint(1, 1));
    PK_VERIFY(PkPoint(1, 1) * d == PkPoint(0, 0));
    PK_VERIFY(f * PkPoint(1, 1) == PkPoint(1, 1));
    PK_VERIFY(d * PkPoint(1, 1) == PkPoint(0, 0));

    PkPoint p(1, 1); p *= f;
    PK_VERIFY(p == PkPoint(1, 1));
    PkPoint q(1, 1); q *= d;
    PK_VERIFY(q == PkPoint(0, 0));
}

void PkPointCase::pointIntegerScaling()
{
    // int 重载**不走 qRound**，是纯整数乘法。实测 QPoint(1,1)*3 == (3,3)。
    const int k = 3;
    PK_VERIFY(PkPoint(1, 1) * k == PkPoint(3, 3));
    PK_VERIFY(k * PkPoint(1, 1) == PkPoint(3, 3));
    PK_VERIFY(PkPoint(-3, -3) * 2 == PkPoint(-6, -6));
    PkPoint r(-3, -3); r *= 2;
    PK_VERIFY(r == PkPoint(-6, -6));
    // 整数乘法不会把 -3*2 变成 -6 以外的东西，也不会因为"顺手"改用 qreal
    // 中转而丢精度：3 与 3.0 走的是两个重载。
    PK_VERIFY(PkPoint(1000000, 1) * 3 == PkPoint(3000000, 3));
}

void PkPointCase::pointDivisionByZeroMatchesQt()
{
    // 除以 0 在 C++ 层面是 int(inf) 这类未定义行为，但 Qt 就是这么写的，
    // 调用点撞上时两侧必须给同一个答案。下面是真 Qt 5.15.7 在本机
    // （x86-64 / g++ / cvttsd2si）的实测取值，不是"数学上对的值"：
    //   QPoint(1,1)/0.0   → (INT_MIN, INT_MIN)     1/0.0 = +inf
    //   QPoint(-1,-1)/0.0 → (0, 0)                -1/0.0 = -inf 走 qRound 负分支
    //   QPoint(0,0)/0.0   → (0, 0)                 0/0.0 = nan
    //   QPoint(1,1)/-0.0  → (0, 0)                 1/-0.0 = -inf
    // 除数经 noFold 送进去：`int(±inf)` 是 UB，全字面量的表达式在 -O1 及以上会被
    // 编译期折叠成另一个答案（见 noFold 上方注释）。
    PK_VERIFY(PkPoint(1, 1) / noFold(0.0) == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPoint(-1, -1) / noFold(0.0) == PkPoint(0, 0));
    PK_VERIFY(PkPoint(0, 0) / noFold(0.0) == PkPoint(0, 0));
    PK_VERIFY(PkPoint(1, 1) / noFold(-0.0) == PkPoint(0, 0));
}

void PkPointCase::pointEquality()
{
    // 整数点是**位相等**，没有模糊比较（与 PkPointF 相反）
    PK_VERIFY(PkPoint(1, 2) == PkPoint(1, 2));
    PK_VERIFY(!(PkPoint(1, 2) == PkPoint(1, 3)));
    PK_VERIFY(PkPoint(1, 2) != PkPoint(2, 2));
    PK_VERIFY(!(PkPoint(1, 2) != PkPoint(1, 2)));
    PK_VERIFY(PkPoint(0, 0) == PkPoint());
    PK_VERIFY(PkPoint(1, 0) != PkPoint(0, 1));
}

void PkPointCase::pointDotProduct()
{
    // ⚠ 计划里把 dotProduct 列进「族内 0 次、不实现」——**实测证伪**：
    // plugins/tools/basictools/kis_tool_measure.cc:139 有
    // `QPointF::dotProduct(diff, offset)`。那份用量导出只数 `.name(` 与
    // `->name(`，静态调用落在别处（与 QTransform::fromTranslate 同一个陷阱）。
    // 实测真 Qt 5.15.7：
    PK_COMPARE(PkPoint::dotProduct(PkPoint(3, 4), PkPoint(5, -6)), -9);
    PK_COMPARE(PkPoint::dotProduct(PkPoint(0, 0), PkPoint(1, 1)), 0);
    // 不防溢出，照抄：实测 (INT_MAX,0)·(2,0) == -2、(INT_MIN,INT_MIN)·(1,1) == 0
    PK_COMPARE(PkPoint::dotProduct(PkPoint(INT_MAX, 0), PkPoint(2, 0)), -2);
    PK_COMPARE(PkPoint::dotProduct(PkPoint(INT_MIN, INT_MIN), PkPoint(1, 1)), 0);
    // 是静态成员，不是自由函数也不是成员函数
    PK_VERIFY((std::is_same<decltype(PkPoint::dotProduct(PkPoint(), PkPoint())), int>::value));
}

// ═══ PkPointF ══════════════════════════════════════════════════════════════

void PkPointCase::pointfConstruction()
{
    const PkPointF d;
    PK_VERIFY(sameBits(d.x(), 0.0));
    PK_VERIFY(sameBits(d.y(), 0.0));
    PK_VERIFY(!std::signbit(d.x()));      // 默认构造是 +0.0，不是 -0.0

    const PkPointF p(1.5, -2.5);
    PK_VERIFY(sameBits(p.x(), 1.5));
    PK_VERIFY(sameBits(p.y(), -2.5));

    // 实测 sizeof(QPointF)==16
    PK_COMPARE(sizeof(PkPointF), sizeof(double) * 2);
    PK_VERIFY(std::is_trivially_copyable<PkPointF>::value);
    PK_VERIFY((std::is_same<qreal, double>::value));
    PK_VERIFY((std::is_same<decltype(p.x()), qreal>::value));
}

void PkPointCase::pointfPromotionFromPkPoint()
{
    // 隐式提升（构造函数**非 explicit**）——Krita 里大量调用点靠这条。
    // 实测 QPointF(QPoint(3,-4)) == (3,-4)。
    const PkPoint ip(3, -4);
    const PkPointF fp = ip;
    PK_VERIFY(sameBits(fp.x(), 3.0));
    PK_VERIFY(sameBits(fp.y(), -4.0));
    PK_VERIFY((std::is_convertible<PkPoint, PkPointF>::value));
    // 反方向没有隐式转换，只有 toPoint()
    PK_VERIFY((!std::is_convertible<PkPointF, PkPoint>::value));

    // 混合运算靠这条隐式提升成立：实测 QPointF(1.5,1.5)+QPoint(1,1) == (2.5,2.5)
    const PkPointF sum = PkPointF(1.5, 1.5) + ip;
    PK_VERIFY(sameBits(sum.x(), 4.5));
    PK_VERIFY(sameBits(sum.y(), -2.5));
}

void PkPointCase::pointfAccessorsAndReferences()
{
    PkPointF p(1.5, 2.5);
    p.setX(-3.25);
    PK_VERIFY(sameBits(p.x(), -3.25));
    p.setY(0.125);
    PK_VERIFY(sameBits(p.y(), 0.125));

    // 实测真 Qt：QPointF(1.5,2.5) 上 rx()+=0.25、ry()=-1 得 (1.75,-1)
    PkPointF q(1.5, 2.5);
    q.rx() += 0.25;
    q.ry() = -1.0;
    PK_VERIFY(sameBits(q.x(), 1.75));
    PK_VERIFY(sameBits(q.y(), -1.0));
    PK_VERIFY((std::is_same<decltype(q.rx()), qreal &>::value));
    PK_VERIFY((std::is_same<decltype(q.ry()), qreal &>::value));
    PK_VERIFY(reinterpret_cast<char *>(&q.ry()) - reinterpret_cast<char *>(&q.rx())
              == static_cast<std::ptrdiff_t>(sizeof(qreal)));
}

void PkPointCase::pointfIsNull()
{
    // ⚠ QPointF::isNull() 用的是 qIsNull，而 qglobal.h:925 的 qIsNull(d) 就是
    // `d == 0.0` —— 于是 **-0.0 也算 null**，而次正规数 5e-324 不算，NaN 不算。
    // 写成 qFuzzyIsNull（绝对阈值 1e-12）会让 5e-324 那条变红。实测真 Qt：
    PK_VERIFY(PkPointF().isNull());
    PK_VERIFY(PkPointF(0.0, 0.0).isNull());
    PK_VERIFY(PkPointF(-0.0, -0.0).isNull());
    PK_VERIFY(PkPointF(-0.0, 0.0).isNull());
    PK_VERIFY(!PkPointF(5e-324, 0.0).isNull());
    PK_VERIFY(!PkPointF(kNaN, 0.0).isNull());
    PK_VERIFY(!PkPointF(1.0, 0.0).isNull());
}

void PkPointCase::pointfManhattanLength()
{
    PK_VERIFY(sameBits(PkPointF(-3.0, 4.0).manhattanLength(), 7.0));
    PK_VERIFY(sameBits(PkPointF(1.5, -2.25).manhattanLength(), 3.75));

    // ⚠ qAbs(-0.0) 返回 -0.0（条件是 t >= 0），于是 -0.0 + -0.0 = **-0.0**。
    // 实测真 Qt：QPointF(-0.0,-0.0).manhattanLength() 的位模式是 0x8000000000000000。
    // 用 PK_COMPARE 比 0.0 是查不出来的（-0.0 == 0.0 为真），只能查符号位。
    const double mz = PkPointF(-0.0, -0.0).manhattanLength();
    PK_VERIFY(sameBits(mz, -0.0));
    PK_VERIFY(std::signbit(mz));
    PK_VERIFY(!std::signbit(PkPointF(0.0, 0.0).manhattanLength()));

    // 特值传播：实测 nan 分量 → nan（位模式 0xfff8...，即 -nan），
    // -1e308 两个分量 → +inf（qAbs 之后相加溢出）
    PK_VERIFY(std::isnan(PkPointF(kNaN, 1.0).manhattanLength()));
    PK_VERIFY(sameBits(PkPointF(-1e308, -1e308).manhattanLength(), kInf));
}

void PkPointCase::pointfAdditiveOperators()
{
    const PkPointF a(1.5, -2.5), b(-0.25, 6.0);
    PK_VERIFY(sameBits((a + b).x(), 1.25));
    PK_VERIFY(sameBits((a + b).y(), 3.5));
    PK_VERIFY(sameBits((a - b).x(), 1.75));
    PK_VERIFY(sameBits((a - b).y(), -8.5));
    PK_VERIFY(sameBits((-a).x(), -1.5));
    PK_VERIFY(sameBits((-a).y(), 2.5));

    PkPointF c(1.5, -2.5);
    c += b;
    PK_VERIFY(sameBits(c.x(), 1.25));
    c -= b;
    PK_VERIFY(sameBits(c.x(), 1.5));

    // 特值传播，实测：1e308+1e308 → +inf；nan+1 → nan
    PK_VERIFY(sameBits((PkPointF(1e308, 0.0) + PkPointF(1e308, 0.0)).x(), kInf));
    PK_VERIFY(std::isnan((PkPointF(kNaN, 0.0) + PkPointF(1.0, 1.0)).x()));
}

void PkPointCase::pointfScalarOperators()
{
    const PkPointF p(1.5, -2.5);
    PK_VERIFY(sameBits((p * 2.0).x(), 3.0));
    PK_VERIFY(sameBits((2.0 * p).y(), -5.0));
    PK_VERIFY(sameBits((p / 2.0).x(), 0.75));

    // PkPointF 的除法**不取整**（与 PkPoint 相反）：实测 QPointF(-3,-3)/2.0 == (-1.5,-1.5)
    PkPointF d(-3.0, -3.0);
    d /= 2.0;
    PK_VERIFY(sameBits(d.x(), -1.5));
    PK_VERIFY(sameBits(d.y(), -1.5));

    PkPointF m(1.5, -2.5);
    m *= 2.0;
    PK_VERIFY(sameBits(m.x(), 3.0));

    // 除以 0 是浮点语义，不是整数 UB。实测：1/0.0 → +inf、0/0.0 → nan、
    // inf*0.0 → nan
    PK_VERIFY(sameBits((PkPointF(1.0, 1.0) / 0.0).x(), kInf));
    PK_VERIFY(std::isnan((PkPointF(0.0, 0.0) / 0.0).x()));
    PK_VERIFY(std::isnan((PkPointF(kInf, 0.0) * 0.0).x()));
}

void PkPointCase::pointfSignedZeroIsBitExact()
{
    // 一元 - 与一元 + 在零号上的取值，实测真 Qt 的位模式：
    //   (-QPointF(0.0,0.0)).x  → 0x8000000000000000（-0.0）
    //   (+QPointF(-0.0,0.0)).x → 0x8000000000000000（原样返回，不规范化）
    // 把一元 + 写成"返回 qAbs(p)"或"规范化零号"这两条立刻变红。
    PK_VERIFY(std::signbit((-PkPointF(0.0, 0.0)).x()));
    PK_VERIFY(std::signbit((+PkPointF(-0.0, 0.0)).x()));
    PK_VERIFY(!std::signbit((+PkPointF(0.0, 0.0)).x()));
    PK_VERIFY(!std::signbit((-PkPointF(-0.0, 0.0)).x()));
    PK_VERIFY(sameBits((-PkPointF(0.0, 0.0)).x(), -0.0));
    PK_VERIFY(sameBits((+PkPointF(-0.0, 0.0)).x(), -0.0));
}

void PkPointCase::pointfFuzzyEquality()
{
    // ⚠ **QPointF::operator== 是模糊比较，不是位相等。** 每个分量二选一：
    // 只要有一侧恰好是 0，比 |差| <= 1e-12（绝对）；否则比相对误差 1e-12。
    // 实测真 Qt 5.15.7：
    PK_VERIFY(PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-13, 1.0));
    PK_VERIFY(!(PkPointF(1.0, 1.0) == PkPointF(1.0 + 1e-11, 1.0)));
    PK_VERIFY(PkPointF(-1.0, 0.0) == PkPointF(-1.0 - 1e-13, 0.0));

    // 零侧走绝对阈值：0 与 1e-300、1e-12、1e-13 相等，与 1e-11 不等
    PK_VERIFY(PkPointF(0.0, 0.0) == PkPointF(1e-300, 0.0));
    PK_VERIFY(PkPointF(1e-300, 0.0) == PkPointF(0.0, 0.0));
    PK_VERIFY(PkPointF(0.0, 0.0) == PkPointF(1e-12, 0.0));
    PK_VERIFY(PkPointF(0.0, 0.0) == PkPointF(1e-13, 0.0));
    PK_VERIFY(!(PkPointF(0.0, 0.0) == PkPointF(1e-11, 0.0)));
    // +0.0 与 -0.0 相等
    PK_VERIFY(PkPointF(0.0, 0.0) == PkPointF(-0.0, 0.0));

    // 非零侧走**相对**阈值，所以两个次正规数只差一个 ulp 也不相等
    PK_VERIFY(!(PkPointF(5e-324, 1.0) == PkPointF(1e-323, 1.0)));
    PK_VERIFY(PkPointF(5e-324, 1.0) == PkPointF(5e-324, 1.0));
    PK_VERIFY(!(PkPointF(1e308, 1.0) == PkPointF(1e307, 1.0)));

    // != 恒是 == 的取反
    PK_VERIFY(PkPointF(1.0, 1.0) != PkPointF(1.0 + 1e-11, 1.0));
    PK_VERIFY(!(PkPointF(1.0, 1.0) != PkPointF(1.0 + 1e-13, 1.0)));

    // 两个分量都要成立才算相等（&& 不是 ||）
    PK_VERIFY(!(PkPointF(1.0, 1.0) == PkPointF(1.0, 2.0)));
    PK_VERIFY(!(PkPointF(1.0, 1.0) == PkPointF(2.0, 1.0)));
}

void PkPointCase::pointfFuzzyEqualityOnSpecialValues()
{
    // ⚠ 两个反直觉但**实测确认**的取值，它们是 qFuzzyCompare 公式
    // `|p1-p2| * 1e12 <= min(|p1|,|p2|)` 在 ±inf 上的退化：
    //   inf == inf  → **false**（|inf-inf| = nan，nan <= inf 恒假）
    //   inf == -inf → **true** （|inf-(-inf)| = inf，inf*1e12 = inf <= inf 成立）
    // 写成"先判 p1.xp == p2.xp 短路"这类"优化"，第一条立刻变红。
    PK_VERIFY(!(PkPointF(kInf, 0.0) == PkPointF(kInf, 0.0)));
    PK_VERIFY(PkPointF(kInf, 0.0) == PkPointF(-kInf, 0.0));
    PK_VERIFY(!(PkPointF(kInf, 0.0) == PkPointF(1e308, 0.0)));

    // NaN 与任何东西都不等（含它自己）
    PK_VERIFY(!(PkPointF(kNaN, 0.0) == PkPointF(kNaN, 0.0)));
    PK_VERIFY(!(PkPointF(kNaN, 0.0) == PkPointF(0.0, 0.0)));
    PK_VERIFY(!(PkPointF(kNaN, 0.0) == PkPointF(1.0, 0.0)));
    PK_VERIFY(PkPointF(kNaN, 0.0) != PkPointF(kNaN, 0.0));

    // 1e308 与自身相等（相对比较在有限大值上正常工作）
    PK_VERIFY(PkPointF(1e308, 1.0) == PkPointF(1e308, 1.0));
    PK_VERIFY(!(PkPointF(1e308, 1.0) == PkPointF(-1e308, 1.0)));
}

void PkPointCase::pointfToPointMatchesQt()
{
    // ⚠ toPoint 用 **qRound**，不是截断。qRound 对负半值向 +∞。
    // 实测真 Qt 5.15.7：
    PK_VERIFY(PkPointF(-1.5, -1.5).toPoint() == PkPoint(-1, -1));
    PK_VERIFY(PkPointF(-0.5, -0.5).toPoint() == PkPoint(0, 0));
    PK_VERIFY(PkPointF(0.5, 0.5).toPoint() == PkPoint(1, 1));
    PK_VERIFY(PkPointF(2.5, 2.5).toPoint() == PkPoint(3, 3));
    PK_VERIFY(PkPointF(-2.5, -2.5).toPoint() == PkPoint(-2, -2));
    // 截断实现会在这三条上给 (-1,-1)/(0,0)/(0,0)，与上面第 1/2/3 条冲突

    // qRound 的经典边界：int(d+0.5) 让 0.49999999999999994 进位到 1，
    // 而它的相反数得 0（负半值向 +∞）。实测确认。
    PK_VERIFY(PkPointF(0.49999999999999994, 0.0).toPoint() == PkPoint(1, 0));
    PK_VERIFY(PkPointF(-0.49999999999999994, 0.0).toPoint() == PkPoint(0, 0));

    // 越界与特值上的实测取值（C++ 层面是 UB，但两侧必须给同一个答案）。
    // ⚠ 全部经 noFold 送进去：越界的 double→int 是 UB，编译期折叠与运行期
    // cvttsd2si 的答案不一样，不加屏障时 -O1 及以上会红（见 noFold 上方注释）。
    // 探针实测真 Qt 5.15.7（同一屏障，-O0/-O2 逐字一致）：
    //   2147483647.0 → INT_MAX；2147483648.0 / 4294967296.0 → INT_MIN；
    //   -2147483648.0 / -2147483649.0 → INT_MIN；+1e308 / +inf → INT_MIN；
    //   -1e308 / -inf / nan → 0
    PK_VERIFY(PkPointF(noFold(2147483647.0), noFold(2147483647.0)).toPoint() == PkPoint(INT_MAX, INT_MAX));
    PK_VERIFY(PkPointF(noFold(2147483648.0), noFold(2147483648.0)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(4294967296.0), noFold(4294967296.0)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(-2147483648.0), noFold(-2147483648.0)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(-2147483649.0), noFold(-2147483649.0)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(1e308), noFold(1e308)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(-1e308), noFold(-1e308)).toPoint() == PkPoint(0, 0));
    PK_VERIFY(PkPointF(noFold(kInf), noFold(kInf)).toPoint() == PkPoint(INT_MIN, INT_MIN));
    PK_VERIFY(PkPointF(noFold(-kInf), noFold(-kInf)).toPoint() == PkPoint(0, 0));
    PK_VERIFY(PkPointF(noFold(kNaN), noFold(kNaN)).toPoint() == PkPoint(0, 0));
    // 次正规数向零收（这一条在 int 值域内，不是 UB，不需要屏障）
    PK_VERIFY(PkPointF(5e-324, -5e-324).toPoint() == PkPoint(0, 0));
}

void PkPointCase::pointfDotProduct()
{
    // 实测真 Qt 5.15.7：(1.5,-2.5)·(2,4) == -7
    PK_VERIFY(sameBits(PkPointF::dotProduct(PkPointF(1.5, -2.5), PkPointF(2.0, 4.0)), -7.0));
    // 零号：(-0,-0)·(0,0) 的位模式是 0x8000...（-0.0*0.0 = -0.0，两个 -0.0 相加还是 -0.0）
    const double z = PkPointF::dotProduct(PkPointF(-0.0, -0.0), PkPointF(0.0, 0.0));
    PK_VERIFY(sameBits(z, -0.0));
    PK_VERIFY(std::signbit(z));
    // 特值：inf*0 → nan；1e308 的平方和 → +inf
    PK_VERIFY(std::isnan(PkPointF::dotProduct(PkPointF(kInf, 0.0), PkPointF(0.0, 0.0))));
    PK_VERIFY(sameBits(PkPointF::dotProduct(PkPointF(1e308, 1e308), PkPointF(1e308, 1e308)), kInf));
    PK_VERIFY((std::is_same<decltype(PkPointF::dotProduct(PkPointF(), PkPointF())), qreal>::value));
}

// ═══ 跨切面 ════════════════════════════════════════════════════════════════

void PkPointCase::fuzzyEqualityIsMacroProof()
{
    // PkPointF::operator== 必须写 pkQtFuzzy* 而不是 qFuzzy*：后者在
    // 「pk/test 的垫片先进 TU」那条真实共存路径上是 **#define**，函数体会被
    // 预处理器当场改写到别人的实现上去，几何类型的相等语义静默换掉。
    // 探针 TU（point_macro_proof.cpp）把那条路径复现出来并指向一对破坏版实现。
    // oracle/ 对拍覆盖不到这一条（那边编译行里根本没有 pk/test 的垫片）。
    const PkPointMacroProof p = pkPointMacroProbe();
    PK_VERIFY(p.sabotagedFuzzyWasVisible);   // 探针没空转
    PK_VERIFY(p.nearIsEqual);
    PK_VERIFY(p.farIsNotEqual);
    PK_VERIFY(p.zeroSideIsEqual);
    PK_VERIFY(p.infVsInfIsNotEqual);
    PK_VERIFY(p.infVsNegInfIsEqual);
}

int run_point_tests()
{
    PkPointCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
