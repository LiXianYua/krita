#include "cases/size_case.h"
#include "../PkSize.h"
#include "size_macro_proof.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// PkTestBinder<PkSizeCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_point.cpp 相同）。
#include "pk_binder_size_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针 probe_size.cpp，链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core，
// QT_VERSION_STR "5.15.7"，`-DQT_NO_DEBUG`），不是"尺寸空就是宽高为零"这类直觉。
// 对齐口径：与 Qt 的任何行为差异默认都是缺陷，所以 Qt 那些反直觉的地方也一起钉住：
//   · **QSize() 是 (-1,-1)**，不是 (0,0) —— 默认构造出来的尺寸 isValid()==false
//   · isNull/isEmpty/isValid **三个互不等价**，且 QSize(0,0).isValid()==**true**
//     （矩形那边 QRect(0,0,0,0).isValid() 是 false，别照搬）
//   · QSize::isEmpty 是 `wd<1||ht<1`、QSizeF::isEmpty 是 `wd<=0.||ht<=0.`
//     —— 整数上两者等价、浮点上不等价（0.5 在前者会被判空，在后者不会）
//   · **源尺寸为空时 scaled 直接返回目标尺寸**（QSize(0,0).scaled(30,30,Keep)==30x30）
//   · QSize::scaled 内部用 qint64 中间量，narrowing 回 int 时会回绕
//
// ⚠ **PK_COMPARE 对 double 走的是 pk/test 的模糊比较（相对 1e-12），不是位相等**
//（R-11 harness 的能力边界）。凡是主张"与 Qt 逐位一致"的地方一律用下面的
// sameBits / std::signbit 直接查位模式。
// ---------------------------------------------------------------------------

namespace {

bool sameBits(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);   // 两侧都是 NaN 就算同（载荷可以不同）
}

bool sameSizeF(const PkSizeF &s, double w, double h)
{ return sameBits(s.width(), w) && sameBits(s.height(), h); }

// 挡住常量折叠。越界的 double→int 转换（int(inf)、int(2147483648.0)）是 UB，
// 编译期折叠与运行期 cvttsd2si 给的答案不一样 —— 理由与 test_point.cpp 的
// noFold() 完全相同，那边有完整说明。
double noFold(double d)
{
    volatile double v = d;
    return v;
}

const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

} // namespace

// ═══ PkSize ════════════════════════════════════════════════════════════════

void PkSizeCase::sizeDefaultIsMinusOne()
{
    // ⚠ 实测真 Qt：QSize() == (-1,-1)，**不是 (0,0)**。默认构造出来的尺寸
    // 是"无效尺寸"这个哨兵值，Krita 里 `QSize s;` 之后判 isValid() 的写法靠它。
    const PkSize d;
    PK_COMPARE(d.width(), -1);
    PK_COMPARE(d.height(), -1);
    PK_VERIFY(!d.isNull());
    PK_VERIFY(d.isEmpty());
    PK_VERIFY(!d.isValid());
}

void PkSizeCase::sizeConstructionAndLayout()
{
    const PkSize s(3, -4);
    PK_COMPARE(s.width(), 3);
    PK_COMPARE(s.height(), -4);

    // 实测 sizeof(QSize)==8：两个 int，没有额外成员/虚表。
    PK_COMPARE(sizeof(PkSize), sizeof(int) * 2);
    PK_VERIFY(std::is_trivially_copyable<PkSize>::value);
    PK_VERIFY(std::is_standard_layout<PkSize>::value);
    PK_VERIFY((std::is_same<decltype(s.width()), int>::value));
}

void PkSizeCase::sizeAccessorsAndReferences()
{
    PkSize s(1, 2);
    s.setWidth(10);
    PK_COMPARE(s.width(), 10);
    s.setHeight(-20);
    PK_COMPARE(s.height(), -20);

    // rwidth()/rheight() 返回**可写引用**。实测真 Qt：QSize(1,2) 上
    // rwidth()+=10、rheight()*=3 得 (11,6)，且两个引用相距 4 字节。
    PkSize r(1, 2);
    r.rwidth() += 10;
    r.rheight() *= 3;
    PK_COMPARE(r.width(), 11);
    PK_COMPARE(r.height(), 6);
    PK_VERIFY((std::is_same<decltype(r.rwidth()), int &>::value));
    PK_VERIFY((std::is_same<decltype(r.rheight()), int &>::value));
    PK_VERIFY(reinterpret_cast<char *>(&r.rheight()) - reinterpret_cast<char *>(&r.rwidth())
              == static_cast<std::ptrdiff_t>(sizeof(int)));
}

void PkSizeCase::sizeThreePredicates()
{
    // ⚠ 三条公式互不相同，负数与 0 的组合逐个钉住（实测网格见探针 §B）：
    //   isNull  = wd==0 && ht==0
    //   isEmpty = wd<1  || ht<1
    //   isValid = wd>=0 && ht>=0
    // 最险的一条：**QSize(0,0).isValid() 是 true**，而 QRect(0,0,0,0).isValid()
    // 是 false —— 抄矩形的语义过来会在这里翻车。
    PK_VERIFY(PkSize(0, 0).isNull());
    PK_VERIFY(PkSize(0, 0).isEmpty());
    PK_VERIFY(PkSize(0, 0).isValid());

    PK_VERIFY(!PkSize(10, 0).isNull());
    PK_VERIFY(PkSize(10, 0).isEmpty());
    PK_VERIFY(PkSize(10, 0).isValid());

    PK_VERIFY(!PkSize(0, 10).isNull());
    PK_VERIFY(PkSize(0, 10).isEmpty());
    PK_VERIFY(PkSize(0, 10).isValid());

    PK_VERIFY(!PkSize(-1, -1).isNull());
    PK_VERIFY(PkSize(-1, -1).isEmpty());
    PK_VERIFY(!PkSize(-1, -1).isValid());

    PK_VERIFY(!PkSize(1, 1).isNull());
    PK_VERIFY(!PkSize(1, 1).isEmpty());
    PK_VERIFY(PkSize(1, 1).isValid());

    // isValid 与 isEmpty 都盯着"零"，但方向不同：(2,-1) 非空判定为空、且无效；
    // (2,0) 为空却**有效**。把 isValid 写成 `wd>0 && ht>0` 会在这一条上红。
    PK_VERIFY(PkSize(2, -1).isEmpty() && !PkSize(2, -1).isValid());
    PK_VERIFY(PkSize(2, 0).isEmpty() && PkSize(2, 0).isValid());
    PK_VERIFY(!PkSize(-2, 2).isValid());
}

void PkSizeCase::sizeExpandedTo()
{
    // qMax 逐分量。实测：(3,7).expandedTo(5,2)==(5,7)、
    // (-3,-7).expandedTo(-5,-2)==(-3,-2)、(0,0).expandedTo(-1,-1)==(0,0)。
    // 用 qMin 写会在第一条上就红。
    PK_VERIFY(PkSize(3, 7).expandedTo(PkSize(5, 2)) == PkSize(5, 7));
    PK_VERIFY(PkSize(-3, -7).expandedTo(PkSize(-5, -2)) == PkSize(-3, -2));
    PK_VERIFY(PkSize(0, 0).expandedTo(PkSize(-1, -1)) == PkSize(0, 0));
    // 真实调用点 plugins/impex/libkra/kra_converter.cpp:297 写的是 `.expandedTo({1,1})`
    // ——花括号初始化要求构造函数非 explicit，这一条同时钉住那个形状。
    PK_VERIFY(PkSize(0, 0).expandedTo({1, 1}) == PkSize(1, 1));
}

void PkSizeCase::sizeScaledThreeModes()
{
    // 实测真 Qt：QSize(10,20).scaled(30,30,·) = Ignore 30x30 / Keep 15x30 / Expand 30x60
    const PkSize s(10, 20);
    PK_VERIFY(s.scaled(30, 30, Qt::IgnoreAspectRatio) == PkSize(30, 30));
    PK_VERIFY(s.scaled(30, 30, Qt::KeepAspectRatio) == PkSize(15, 30));
    PK_VERIFY(s.scaled(30, 30, Qt::KeepAspectRatioByExpanding) == PkSize(30, 60));
    // QSize 重载与 (w,h) 重载必须一致
    PK_VERIFY(s.scaled(PkSize(30, 30), Qt::KeepAspectRatio) == s.scaled(30, 30, Qt::KeepAspectRatio));

    // 整数除法**截断**（不是四舍五入）：(3,7)→10x10 Keep 得 (4,10)，
    // 因为 rw = 10*3/7 = 4（30/7=4.28…截断）；Expand 得 (10,23)（10*7/3=23.33…）。
    const PkSize t(3, 7);
    PK_VERIFY(t.scaled(10, 10, Qt::KeepAspectRatio) == PkSize(4, 10));
    PK_VERIFY(t.scaled(10, 10, Qt::KeepAspectRatioByExpanding) == PkSize(10, 23));
    // 截断到 0 的形态：(1,3)→3x1 Keep 得 (0,1)
    PK_VERIFY(PkSize(1, 3).scaled(3, 1, Qt::KeepAspectRatio) == PkSize(0, 1));

    // 负分量照样参与（Qt 不做任何合法性检查）：实测
    // (-10,20)→30x30 Keep=(-15,30) Expand=(30,-60)；(10,20)→(-30,30) Keep=(-30,-60)
    PK_VERIFY(PkSize(-10, 20).scaled(30, 30, Qt::KeepAspectRatio) == PkSize(-15, 30));
    PK_VERIFY(PkSize(-10, 20).scaled(30, 30, Qt::KeepAspectRatioByExpanding) == PkSize(30, -60));
    PK_VERIFY(PkSize(10, 20).scaled(-30, 30, Qt::KeepAspectRatio) == PkSize(-30, -60));
    PK_VERIFY(PkSize(10, 20).scaled(-30, 30, Qt::KeepAspectRatioByExpanding) == PkSize(15, 30));
}

void PkSizeCase::sizeScaledDegenerateSource()
{
    // ⚠ **反直觉但实测如此**：源的任一分量为 0 时（含 Keep/Expand），
    // 直接返回目标尺寸，不做任何比例运算。
    PK_VERIFY(PkSize(0, 0).scaled(30, 30, Qt::KeepAspectRatio) == PkSize(30, 30));
    PK_VERIFY(PkSize(0, 20).scaled(30, 30, Qt::KeepAspectRatio) == PkSize(30, 30));
    PK_VERIFY(PkSize(10, 0).scaled(30, 30, Qt::KeepAspectRatioByExpanding) == PkSize(30, 30));
    // 目标为空则原样返回目标（同一条分支的另一侧）
    PK_VERIFY(PkSize(10, 20).scaled(0, 0, Qt::KeepAspectRatio) == PkSize(0, 0));
    // IgnoreAspectRatio 永远原样返回目标
    PK_VERIFY(PkSize(10, 20).scaled(0, 0, Qt::IgnoreAspectRatio) == PkSize(0, 0));
    PK_VERIFY(PkSize(-1, -1).scaled(-1, -1, Qt::KeepAspectRatio) == PkSize(-1, -1));
}

void PkSizeCase::sizeScaledUsesInt64Intermediate()
{
    // 中间量是 qint64（`qint64(s.ht) * qint64(wd) / qint64(ht)`），不是 int：
    // 写成 int 会在这几条上先溢出再比较，选错分支。实测真 Qt：
    PK_VERIFY(PkSize(INT_MAX, 1).scaled(INT_MAX, INT_MAX, Qt::KeepAspectRatio)
              == PkSize(INT_MAX, 1));
    PK_VERIFY(PkSize(INT_MAX, 1).scaled(INT_MAX, INT_MAX, Qt::KeepAspectRatioByExpanding)
              == PkSize(1, INT_MAX));
    PK_VERIFY(PkSize(1, INT_MAX).scaled(INT_MAX, 1, Qt::KeepAspectRatio) == PkSize(0, 1));
    PK_VERIFY(PkSize(INT_MAX, INT_MAX).scaled(INT_MAX, INT_MAX, Qt::KeepAspectRatio)
              == PkSize(INT_MAX, INT_MAX));
    PK_VERIFY(PkSize(INT_MIN, 1).scaled(5, 5, Qt::KeepAspectRatio) == PkSize(INT_MIN, 5));
    PK_VERIFY(PkSize(INT_MIN, 1).scaled(5, 5, Qt::KeepAspectRatioByExpanding) == PkSize(5, 0));
    PK_VERIFY(PkSize(1, INT_MIN).scaled(5, 5, Qt::KeepAspectRatioByExpanding) == PkSize(5, INT_MIN));
    // qint64 → int 的窄化回绕：实测 (INT_MAX,2).scaled(2,INT_MAX,Expand) == (INT_MIN,INT_MAX)
    PK_VERIFY(PkSize(INT_MAX, 2).scaled(2, INT_MAX, Qt::KeepAspectRatioByExpanding)
              == PkSize(INT_MIN, INT_MAX));
    PK_VERIFY(PkSize(INT_MAX, 2).scaled(2, INT_MAX, Qt::KeepAspectRatio) == PkSize(2, 0));
}

void PkSizeCase::sizeScaleInPlace()
{
    // scale() 就是 `*this = scaled(...)`，两个重载都在
    PkSize a(10, 20);
    a.scale(30, 30, Qt::KeepAspectRatio);
    PK_VERIFY(a == PkSize(15, 30));

    PkSize b(0, 0);
    b.scale(PkSize(30, 30), Qt::KeepAspectRatio);
    PK_VERIFY(b == PkSize(30, 30));

    // 真实调用点形态：libs/image/kis_paint_device.cc:1731
    // `thumbnailSize.scale(imageRect.size(), Qt::KeepAspectRatio);`
    // ⚠ **接收者是被缩放的那个，实参是目标框** —— 反过来是另一回事。
    // 实测真 Qt：QSize(128,128).scale(QSize(1000,500),Keep) → (500,500)，
    // Expand → (1000,1000)。
    PkSize thumb(128, 128);
    thumb.scale(PkSize(1000, 500), Qt::KeepAspectRatio);
    PK_VERIFY(thumb == PkSize(500, 500));
    PkSize thumb2(128, 128);
    thumb2.scale(PkSize(1000, 500), Qt::KeepAspectRatioByExpanding);
    PK_VERIFY(thumb2 == PkSize(1000, 1000));
}

void PkSizeCase::sizeAdditiveOperators()
{
    PK_VERIFY(PkSize(3, 7) + PkSize(1, 2) == PkSize(4, 9));
    PK_VERIFY(PkSize(3, 7) - PkSize(1, 2) == PkSize(2, 5));
    { PkSize s(3, 7); s += PkSize(1, 2); PK_VERIFY(s == PkSize(4, 9)); }
    { PkSize s(3, 7); s -= PkSize(1, 2); PK_VERIFY(s == PkSize(2, 5)); }
    // ⚠ 不防溢出，照抄。实测 QSize(INT_MAX,INT_MAX)+QSize(1,1) == (INT_MIN,INT_MIN)
    //（构建带 -fwrapv，两侧都按二补数回绕，理由见 README 覆盖度缺口）。
    PK_VERIFY(PkSize(INT_MAX, INT_MAX) + PkSize(1, 1) == PkSize(INT_MIN, INT_MIN));
}

void PkSizeCase::sizeScalingRoundsLikeQt()
{
    // 乘 qreal 走 qRound，而 qRound 对负半值向 +∞ 取整。实测真 Qt：
    // QSize(-1,-1)*0.5 == (0,0)、QSize(-3,-3)*0.5 == (-1,-1)、0.5*QSize(5,5) == (3,3)
    PK_VERIFY(PkSize(-1, -1) * 0.5 == PkSize(0, 0));
    PK_VERIFY(PkSize(-3, -3) * 0.5 == PkSize(-1, -1));
    PK_VERIFY(0.5 * PkSize(5, 5) == PkSize(3, 3));
    { PkSize s(5, 5); s *= 0.5; PK_VERIFY(s == PkSize(3, 3)); }
    // 与 QPoint 不同：QSize 只有 qreal 一个重载（没有 float / int 版），
    // 所以 float 实参会提升到 double 再走同一条路。
    { const float f = 0.5f; PK_VERIFY(PkSize(5, 5) * f == PkSize(3, 3)); }
}

void PkSizeCase::sizeDivision()
{
    PK_VERIFY(PkSize(10, 20) / 2.0 == PkSize(5, 10));
    PK_VERIFY(PkSize(-1, -1) / 2.0 == PkSize(0, 0));   // qRound(-0.5)==0
    { PkSize s(10, 20); s /= 4.0; PK_VERIFY(s == PkSize(3, 5)); }  // qRound(2.5)=3 qRound(5)=5

    // Qt 在这里有 Q_ASSERT(!qFuzzyIsNull(c))，**Krita 的发布构建里整条编译掉**
    //（Qt5 的 cmake 模块给非 Debug 构建加 -DQT_NO_DEBUG，见 krita/CMakeLists.txt:968
    // 那条 option 的说明）。pk/geometry 没有断言设施（归 R-08），所以不实现它；
    // 对齐的是发布构建的形态：除以 0 得 qRound(±inf)，实测真 Qt(-DQT_NO_DEBUG)
    // 与本实现都是 INT_MIN。noFold 把越界转换压到运行期（理由见 test_point.cpp）。
    const PkSize z = PkSize(10, 20) / noFold(0.0);
    PK_COMPARE(z.width(), INT_MIN);
    PK_COMPARE(z.height(), INT_MIN);
}

void PkSizeCase::sizeEquality()
{
    // 整数尺寸用**位相等**（不是模糊比较）
    PK_VERIFY(PkSize(3, 7) == PkSize(3, 7));
    PK_VERIFY(PkSize(3, 7) != PkSize(3, 8));
    PK_VERIFY(PkSize(3, 7) != PkSize(4, 7));
    PK_VERIFY(!(PkSize(0, 0) == PkSize(-1, -1)));
    PK_VERIFY(PkSize() == PkSize(-1, -1));
}

// ═══ PkSizeF ═══════════════════════════════════════════════════════════════

void PkSizeCase::sizefDefaultIsMinusOne()
{
    const PkSizeF d;
    PK_VERIFY(sameSizeF(d, -1.0, -1.0));
    PK_VERIFY(!d.isNull());
    PK_VERIFY(d.isEmpty());
    PK_VERIFY(!d.isValid());
}

void PkSizeCase::sizefConstructionAndLayout()
{
    const PkSizeF s(3.5, -4.25);
    PK_VERIFY(sameSizeF(s, 3.5, -4.25));
    PK_COMPARE(sizeof(PkSizeF), sizeof(double) * 2);
    PK_VERIFY(std::is_trivially_copyable<PkSizeF>::value);
    PK_VERIFY(std::is_standard_layout<PkSizeF>::value);
    PK_VERIFY((std::is_same<decltype(s.width()), qreal>::value));
}

void PkSizeCase::sizefPromotionFromPkSize()
{
    // 非 explicit：PkSize → PkSizeF 是隐式提升（Task 4/5 的 PkRectF 靠这条）
    const PkSizeF f = PkSize(3, 4);
    PK_VERIFY(sameSizeF(f, 3.0, 4.0));
    PK_VERIFY((std::is_convertible<PkSize, PkSizeF>::value));
    PK_VERIFY((!std::is_convertible<PkSizeF, PkSize>::value));   // 反方向只有 toSize()

    // ⚠ **提升必须走 double，不能中途经 float。** int → float 只有 24 位有效位，
    // INT_MAX 会被舍到 2147483648.f；int → double 是精确的。复评实测：把两个
    // 初值改成 float(sz.width()) 之后**全部 33 个用例照样全绿**，只有对拍抓到
    // （962 323 处）—— 这一行就是把那条缺口的最粗一层堵上。
    // 用 == 不用 PK_COMPARE：后者对 double 是相对 1e-12 的模糊比较，
    // 而 2147483648.0 与 2147483647.0 的相对差是 4.7e-10，模糊比较照样判等。
    const PkSizeF big = PkSize(2147483647, 2147483646);          // INT_MAX, INT_MAX-1
    PK_VERIFY(big.width()  == 2147483647.0);
    PK_VERIFY(big.height() == 2147483646.0);
    PK_VERIFY(big.toSize() == PkSize(2147483647, 2147483646));   // 往返回到原值
}

void PkSizeCase::sizefAccessorsAndReferences()
{
    PkSizeF s(1.5, 2.5);
    s.setWidth(10.25);
    PK_VERIFY(sameBits(s.width(), 10.25));
    s.setHeight(-20.5);
    PK_VERIFY(sameBits(s.height(), -20.5));

    // 实测：QSizeF(1.5,2.5) 上 rwidth()+=0.25、rheight()-=0.5 得 (1.75,2)
    PkSizeF r(1.5, 2.5);
    r.rwidth() += 0.25;
    r.rheight() -= 0.5;
    PK_VERIFY(sameSizeF(r, 1.75, 2.0));
    PK_VERIFY((std::is_same<decltype(r.rwidth()), qreal &>::value));
    PK_VERIFY((std::is_same<decltype(r.rheight()), qreal &>::value));
}

void PkSizeCase::sizefThreePredicates()
{
    // ⚠ 与 PkSize **不是同一套公式**：
    //   isNull  = qIsNull(wd) && qIsNull(ht)（就是 ==0.0，所以 -0.0 也算 null）
    //   isEmpty = wd <= 0. || ht <= 0.        ← 整数版是 `< 1`
    //   isValid = wd >= 0. && ht >= 0.
    // 把整数版的 `< 1` 抄过来，(0.5,0.5) 会被判成空 —— 实测真 Qt 是**非空**。
    PK_VERIFY(!PkSizeF(0.5, 0.5).isEmpty());
    PK_VERIFY(!PkSizeF(0.5, 1.0).isEmpty());
    PK_VERIFY(!PkSizeF(5e-324, 5e-324).isEmpty());   // 次正规也算非空

    PK_VERIFY(PkSizeF(0.0, 0.0).isNull());
    PK_VERIFY(PkSizeF(-0.0, -0.0).isNull());          // -0.0 == 0.0
    PK_VERIFY(PkSizeF(-0.0, 0.0).isNull());
    PK_VERIFY(!PkSizeF(5e-324, 0.0).isNull());        // 次正规不是 null
    PK_VERIFY(PkSizeF(0.0, 0.0).isEmpty());
    PK_VERIFY(PkSizeF(0.0, 0.0).isValid());           // 与整数版一致：(0,0) 有效

    PK_VERIFY(PkSizeF(-1.0, -1.0).isEmpty() && !PkSizeF(-1.0, -1.0).isValid());

    // NaN：三条谓词各走各的。`nan <= 0.` 与 `nan >= 0.` 都是 false，于是
    //   isEmpty(nan,0.5) = false || false = **false**（非空！）
    //   isValid(nan,0.5) = false && true  = false
    PK_VERIFY(!PkSizeF(kNaN, 0.5).isEmpty());
    PK_VERIFY(!PkSizeF(kNaN, 0.5).isValid());
    PK_VERIFY(!PkSizeF(kNaN, kNaN).isEmpty());
    PK_VERIFY(!PkSizeF(kNaN, kNaN).isNull());
    PK_VERIFY(PkSizeF(kNaN, -1.0).isEmpty());          // 另一分量为负 → 空

    // ±inf
    PK_VERIFY(!PkSizeF(kInf, 0.5).isEmpty());
    PK_VERIFY(PkSizeF(kInf, 0.5).isValid());
    PK_VERIFY(PkSizeF(kInf, -kInf).isEmpty());
    PK_VERIFY(!PkSizeF(-kInf, 1.0).isValid());
    PK_VERIFY(PkSizeF(0.0, kInf).isEmpty());           // 0 分量把它拉成空
}

void PkSizeCase::sizefExpandedTo()
{
    PK_VERIFY(PkSizeF(3.5, 7.5).expandedTo(PkSizeF(5.0, 2.0)) == PkSizeF(5.0, 7.5));
    // ⚠ qMax(a,b) 写作 `(a < b) ? b : a`，NaN 参与时**不可交换**（实测真 Qt）：
    //   qMax(nan, 2) → nan（nan<2 为假，返回 a=nan）
    //   qMax(2, nan) → 2  （2<nan 为假，返回 a=2）
    // 用 std::fmax 之类"NaN 安全"的写法会在这两条上红。
    PK_VERIFY(sameSizeF(PkSizeF(kNaN, 1.0).expandedTo(PkSizeF(2.0, 2.0)), kNaN, 2.0));
    PK_VERIFY(sameSizeF(PkSizeF(2.0, 2.0).expandedTo(PkSizeF(kNaN, 1.0)), 2.0, 2.0));
    // 零号同理：qMax(-0.0, 0.0) → -0.0（-0.0 < 0.0 为假）。位模式实测一致。
    PK_VERIFY(sameSizeF(PkSizeF(-0.0, 1.0).expandedTo(PkSizeF(0.0, 1.0)), -0.0, 1.0));
    PK_VERIFY(sameSizeF(PkSizeF(0.0, 1.0).expandedTo(PkSizeF(-0.0, 1.0)), 0.0, 1.0));
}

void PkSizeCase::sizefScaledThreeModes()
{
    const PkSizeF s(10.0, 20.0);
    PK_VERIFY(s.scaled(30.0, 30.0, Qt::IgnoreAspectRatio) == PkSizeF(30.0, 30.0));
    PK_VERIFY(s.scaled(30.0, 30.0, Qt::KeepAspectRatio) == PkSizeF(15.0, 30.0));
    PK_VERIFY(s.scaled(30.0, 30.0, Qt::KeepAspectRatioByExpanding) == PkSizeF(30.0, 60.0));
    PK_VERIFY(s.scaled(PkSizeF(30.0, 30.0), Qt::KeepAspectRatio)
              == s.scaled(30.0, 30.0, Qt::KeepAspectRatio));

    // ⚠ 与整数版的关键区别：**这里是浮点除法，不截断**。
    // 实测 QSizeF(3,7).scaled(10,10,Keep) = (4.28571…,10)，整数版是 (4,10)。
    const PkSizeF t(3.0, 7.0);
    PK_VERIFY(sameBits(t.scaled(10.0, 10.0, Qt::KeepAspectRatio).width(), 10.0 * 3.0 / 7.0));
    PK_VERIFY(sameBits(t.scaled(10.0, 10.0, Qt::KeepAspectRatioByExpanding).height(),
                       10.0 * 7.0 / 3.0));
    PK_VERIFY(sameBits(PkSizeF(1.0, 3.0).scaled(3.0, 1.0, Qt::KeepAspectRatio).width(),
                       1.0 * 1.0 / 3.0));
    PK_VERIFY(PkSizeF(-10.0, 20.0).scaled(30.0, 30.0, Qt::KeepAspectRatio) == PkSizeF(-15.0, 30.0));
}

void PkSizeCase::sizefScaledDegenerateSource()
{
    // 分支条件是 qIsNull(wd) || qIsNull(ht)，即 ==0.0 —— **-0.0 也走这条**。
    PK_VERIFY(PkSizeF(0.0, 0.0).scaled(30.0, 30.0, Qt::KeepAspectRatio) == PkSizeF(30.0, 30.0));
    PK_VERIFY(PkSizeF(-0.0, 5.0).scaled(30.0, 30.0, Qt::KeepAspectRatio) == PkSizeF(30.0, 30.0));
    PK_VERIFY(PkSizeF(5.0, -0.0).scaled(30.0, 30.0, Qt::KeepAspectRatioByExpanding)
              == PkSizeF(30.0, 30.0));
    PK_VERIFY(PkSizeF(-0.0, -0.0).scaled(PkSizeF(7.0, 9.0), Qt::KeepAspectRatio)
              == PkSizeF(7.0, 9.0));
    // ⚠ 次正规**不**走这条分支（qIsNull 是 ==0.0，不是 fuzzy）：
    // 实测 QSizeF(5e-324,1).scaled(5,5,Keep) = (2.47033e-323, 5)，不是 (5,5)。
    PK_VERIFY(sameBits(PkSizeF(5e-324, 1.0).scaled(5.0, 5.0, Qt::KeepAspectRatio).width(),
                       5.0 * 5e-324 / 1.0));
}

void PkSizeCase::sizefScaledSpecialValues()
{
    // NaN/inf 全都原样流过公式（Qt 不做特判），实测：
    //   (nan,2).scaled(5,5,Keep)   = (5, nan)
    //   (2,2).scaled(nan,5,Keep)   = (nan, nan)
    //   (inf,2).scaled(5,5,Keep)   = (5, 0)      ← rw=5*inf/2=inf，inf<=5 假 → 走 else
    //   (inf,2).scaled(5,5,Expand) = (inf, 5)
    //   (2,inf).scaled(5,5,Keep)   = (0, 5)
    PK_VERIFY(sameSizeF(PkSizeF(kNaN, 2.0).scaled(5.0, 5.0, Qt::KeepAspectRatio), 5.0, kNaN));
    PK_VERIFY(sameSizeF(PkSizeF(2.0, kNaN).scaled(5.0, 5.0, Qt::KeepAspectRatio), 5.0, kNaN));
    PK_VERIFY(sameSizeF(PkSizeF(2.0, 2.0).scaled(kNaN, 5.0, Qt::KeepAspectRatio), kNaN, kNaN));
    PK_VERIFY(sameSizeF(PkSizeF(2.0, 2.0).scaled(kNaN, 5.0, Qt::IgnoreAspectRatio), kNaN, 5.0));
    PK_VERIFY(sameSizeF(PkSizeF(kInf, 2.0).scaled(5.0, 5.0, Qt::KeepAspectRatio), 5.0, 0.0));
    PK_VERIFY(sameSizeF(PkSizeF(kInf, 2.0).scaled(5.0, 5.0, Qt::KeepAspectRatioByExpanding),
                        kInf, 5.0));
    PK_VERIFY(sameSizeF(PkSizeF(2.0, kInf).scaled(5.0, 5.0, Qt::KeepAspectRatio), 0.0, 5.0));
    PK_VERIFY(sameSizeF(PkSizeF(1e308, 1e-308).scaled(5.0, 5.0, Qt::KeepAspectRatioByExpanding),
                        kInf, 5.0));
}

void PkSizeCase::sizefScaleInPlace()
{
    PkSizeF a(10.0, 20.0);
    a.scale(30.0, 30.0, Qt::KeepAspectRatio);
    PK_VERIFY(a == PkSizeF(15.0, 30.0));

    PkSizeF b(0.0, 0.0);
    b.scale(PkSizeF(30.0, 30.0), Qt::KeepAspectRatio);
    PK_VERIFY(b == PkSizeF(30.0, 30.0));
}

void PkSizeCase::sizefArithmeticOperators()
{
    PK_VERIFY(PkSizeF(3.5, 7.5) + PkSizeF(1.0, 2.0) == PkSizeF(4.5, 9.5));
    PK_VERIFY(PkSizeF(3.5, 7.5) - PkSizeF(1.0, 2.0) == PkSizeF(2.5, 5.5));
    { PkSizeF s(3.5, 7.5); s += PkSizeF(1.0, 2.0); PK_VERIFY(s == PkSizeF(4.5, 9.5)); }
    { PkSizeF s(3.5, 7.5); s -= PkSizeF(1.0, 2.0); PK_VERIFY(s == PkSizeF(2.5, 5.5)); }
    // ⚠ 浮点版**不取整**（整数版走 qRound）。实测 QSizeF(1.5,2.5)*2.0 == (3,5)
    PK_VERIFY(sameSizeF(PkSizeF(1.5, 2.5) * 2.0, 3.0, 5.0));
    PK_VERIFY(sameSizeF(2.0 * PkSizeF(1.5, 2.5), 3.0, 5.0));
    { PkSizeF s(1.5, 2.5); s *= 2.0; PK_VERIFY(sameSizeF(s, 3.0, 5.0)); }
    // 零号保号：实测 QSizeF(-0.0,0.0)*1.0 的位模式是 (-0.0, 0.0)
    PK_VERIFY(sameSizeF(PkSizeF(-0.0, 0.0) * 1.0, -0.0, 0.0));
    // 相减得到的零是 +0.0（IEEE：x-x == +0.0）
    PK_VERIFY(sameSizeF(PkSizeF(1.0, 2.0) + PkSizeF(-1.0, -2.0), 0.0, 0.0));
}

void PkSizeCase::sizefFuzzyEquality()
{
    // ⚠ **与 PkPointF 的 == 不是同一个公式**：QPointF 逐分量二选一（任一侧为 0
    // 走 fuzzyIsNull），**QSizeF 直接对两个分量各做一次 qFuzzyCompare，没有零分支**。
    // 于是 (0,0)==(1e-300,0) 在 QPointF 上是 true、在 QSizeF 上是 **false**（实测）。
    PK_VERIFY(PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-13, 1.0));    // 阈内
    PK_VERIFY(!(PkSizeF(1.0, 1.0) == PkSizeF(1.0 + 1e-11, 1.0))); // 阈外
    PK_VERIFY(PkSizeF(0.0, 0.0) == PkSizeF(0.0, 0.0));            // 两侧全零：0<=0 成立
    PK_VERIFY(PkSizeF(0.0, 0.0) == PkSizeF(-0.0, 0.0));
    PK_VERIFY(!(PkSizeF(0.0, 0.0) == PkSizeF(1e-300, 0.0)));      // ← 与 PkPointF 相反
    PK_VERIFY(PkSizeF(0.0, 0.0) != PkSizeF(1e-300, 0.0));
    PK_VERIFY(PkSizeF() == PkSizeF(-1.0, -1.0));
    PK_VERIFY(PkSizeF(3.5, 7.5) != PkSizeF(3.5, 7.6));
}

void PkSizeCase::sizefFuzzyEqualityOnSpecialValues()
{
    // qFuzzyCompare 在 ±inf 上退化，实测真 Qt：
    //   (inf,1)==(inf,1)  → false（|inf-inf| = nan，nan<=x 恒假）
    //   (inf,1)==(-inf,1) → true （|inf-(-inf)| = inf <= inf）
    PK_VERIFY(!(PkSizeF(kInf, 1.0) == PkSizeF(kInf, 1.0)));
    PK_VERIFY(PkSizeF(kInf, 1.0) == PkSizeF(-kInf, 1.0));
    PK_VERIFY(!(PkSizeF(kNaN, 1.0) == PkSizeF(kNaN, 1.0)));
    // operator!= 照抄 Qt 的写法（`!fuzzy(w) || !fuzzy(h)`），与 !(==) 逐条一致
    PK_VERIFY(PkSizeF(kInf, 1.0) != PkSizeF(kInf, 1.0));
    PK_VERIFY(!(PkSizeF(kInf, 1.0) != PkSizeF(-kInf, 1.0)));
}

void PkSizeCase::sizefToSizeMatchesQt()
{
    // qRound，**不是截断**；负半值向 +∞。实测真 Qt：
    // (-1.5,1.5)→(-1,2)  (-0.5,0.5)→(0,1)  (2.5,-2.5)→(3,-2)
    PK_VERIFY(PkSizeF(-1.5, 1.5).toSize() == PkSize(-1, 2));
    PK_VERIFY(PkSizeF(-0.5, 0.5).toSize() == PkSize(0, 1));
    PK_VERIFY(PkSizeF(2.5, -2.5).toSize() == PkSize(3, -2));
    // int(d+0.5) 的经典边界：0.49999999999999994 进位到 1
    PK_VERIFY(PkSizeF(0.49999999999999994, -0.49999999999999994).toSize() == PkSize(1, 0));

    // 越界与非有限：两侧都是 UB，实机上编成同一条 cvttsd2si。noFold 把转换压到
    // 运行期（编译期折叠给的是另一个答案，理由见 test_point.cpp 的 noFold）。
    PK_VERIFY(PkSizeF(noFold(2147483648.0), noFold(-2147483648.0)).toSize()
              == PkSize(INT_MIN, INT_MIN));
    PK_VERIFY(PkSizeF(noFold(kInf), noFold(-kInf)).toSize() == PkSize(INT_MIN, 0));
    PK_VERIFY(PkSizeF(noFold(kNaN), noFold(-kNaN)).toSize() == PkSize(0, 0));
}

void PkSizeCase::sizefDivision()
{
    PK_VERIFY(sameSizeF(PkSizeF(10.0, 20.0) / 4.0, 2.5, 5.0));
    { PkSizeF s(10.0, 20.0); s /= 4.0; PK_VERIFY(sameSizeF(s, 2.5, 5.0)); }
    // Q_ASSERT 不实现（见 sizeDivision 的说明）：除以 0 得 ±inf / nan，实测一致
    PK_VERIFY(sameSizeF(PkSizeF(10.0, 20.0) / noFold(0.0), kInf, kInf));
    PK_VERIFY(sameSizeF(PkSizeF(0.0, 0.0) / noFold(0.0), kNaN, kNaN));
}

// ═══ 跨切面 ════════════════════════════════════════════════════════════════

void PkSizeCase::aspectRatioModeEnumValues()
{
    // ⚠ 照 `enum { A, B, C }` 的顺序写才对得上；实测真 Qt 5.15.7 是 0/1/2、
    // sizeof==4、底层类型无符号。Transform 那边的 TransformationType 是**位标志**
    //（TxRotate=4、TxShear=8），两者不是同一个套路，别互相照抄。
    PK_COMPARE(static_cast<int>(Qt::IgnoreAspectRatio), 0);
    PK_COMPARE(static_cast<int>(Qt::KeepAspectRatio), 1);
    PK_COMPARE(static_cast<int>(Qt::KeepAspectRatioByExpanding), 2);
    PK_COMPARE(sizeof(Qt::AspectRatioMode), sizeof(int));
    PK_VERIFY(!std::is_signed<std::underlying_type<Qt::AspectRatioMode>::type>::value);
}

void PkSizeCase::noexceptSurfaceMatchesQt()
{
    // qsize.h 里几乎每个成员都标了 noexcept（qpoint.h 只有 transposed 标了，
    // 所以 PkPoint 那边没有这一条）。noexcept 是**可观察**的（noexcept 运算符、
    // 容器的移动选择），漏标不是风格问题。带 Q_ASSERT 的除法两条 Qt 恰好**没有**
    // 标 noexcept —— 连这个不对称也照抄。
    PkSize s(1, 1);
    PkSizeF f(1.0, 1.0);
    PK_VERIFY(noexcept(PkSize()));
    PK_VERIFY(noexcept(s.isEmpty()) && noexcept(s.isValid()) && noexcept(s.isNull()));
    PK_VERIFY(noexcept(s.width()) && noexcept(s.rwidth()));
    PK_VERIFY(noexcept(s.expandedTo(s)));
    PK_VERIFY(noexcept(s.scaled(s, Qt::KeepAspectRatio)));
    PK_VERIFY(noexcept(s.scale(1, 1, Qt::KeepAspectRatio)));
    PK_VERIFY(noexcept(s == s) && noexcept(s + s) && noexcept(s * 2.0));
    PK_VERIFY(!noexcept(s / 2.0));      // Qt 这一个没标 noexcept
    PK_VERIFY(!noexcept(s /= 2.0));
    PK_VERIFY(noexcept(f.isEmpty()) && noexcept(f.toSize()));
    PK_VERIFY(!noexcept(f / 2.0));
}

void PkSizeCase::sizeFuzzyEqualityIsMacroProof()
{
    // PkSizeF::operator== 必须写 pkQtFuzzy* 而不是 qFuzzy*：后者在
    // 「pk/test 的垫片先进 TU」那条真实共存路径上是 **#define**，函数体会被
    // 预处理器当场改写到别人的实现上去。探针 TU（size_macro_proof.cpp）把那条
    // 路径复现出来并指向一对破坏版实现；oracle/ 覆盖不到这一类预处理期偷换。
    const PkSizeMacroProof p = pkSizeMacroProbe();
    PK_VERIFY(p.sabotagedFuzzyWasVisible);   // 探针没空转
    PK_VERIFY(p.nearIsEqual);
    PK_VERIFY(p.farIsNotEqual);
    PK_VERIFY(p.zeroSideIsNotEqual);
    PK_VERIFY(p.bothZeroIsEqual);
    PK_VERIFY(p.infVsInfIsNotEqual);
    PK_VERIFY(p.infVsNegInfIsEqual);
}

int run_size_tests()
{
    PkSizeCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
