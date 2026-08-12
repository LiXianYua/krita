#include "cases/rectf_case.h"
#include "rectf_macro_proof.h"
#include "../PkRect.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// PkTestBinder<PkRectFCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_rect.cpp 相同）。
#include "pk_binder_rectf_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针 probe_rectf.cpp /
// probe_rectf2.cpp，链 /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的
// libQt5Core，QT_VERSION_STR "5.15.7"，`-DQT_NO_DEBUG`；完整输出在 Task 5 报告
// §2），不是"浮点矩形右边界当然是 x+w"这类直觉 —— 直觉在这一族里对了一半、
// 错了另一半，而错的那一半正是 PkRect 抄过来的：
//   · **right()/bottom() 没有差一**（PkRect 那边有）
//   · isNull/isEmpty/isValid 的门槛是 `==0.` / `<=0.` / `>0.`
//     （PkRect 那边等价于 `跨距==-1` / `<1` / `>=0`），且**三者在 NaN 上互不为补**
//   · set* 一族**保对边、改宽高**（PkRect 那边只摆一个坐标，不保宽高）
//   · normalized 的条件是 `w < 0`（PkRect 是 `x2 < x1 - 1`）
//   · toRect 在**四条边**上取整、toAlignedRect 向外扩，两者常常不同
//   · operator== 是**模糊比较**（PkRect 是整数精确相等）
//
// ⚠ 本文件里大量的量是 double，而 **PK_COMPARE 对 double 走的是 pk/test 的模糊
// 比较（相对 1e-12），不是位相等**（R-11 harness 的能力边界，写进 README 覆盖度
// 缺口）。所以凡是要主张"位一致"（±0.0、NaN、次正规）的断言一律用
// PK_VERIFY + 下面的 sameD()，不用 PK_COMPARE。
// ---------------------------------------------------------------------------

namespace {

// 与 test_point.cpp / test_size.cpp 的 noFold() 完全相同，那边有完整说明：
// 把越界的浮点→int 转换压到运行期（编译期折叠给的是另一个答案）。
double noFold(double d)
{
    volatile double v = d;
    return v;
}

const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();
const double kSub = 5e-324;                    // 最小正次正规

// **位精确**比较：`==` 会把 +0.0/-0.0 判等、把 NaN 判不等，两者都不是我们要的。
// PkRectF 的 -0.0 是真的会传播出去的（normalized 不翻正 -0.0 那一条就靠它）。
bool sameD(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);               // 都是 NaN 就算同
}

// 断言四个内部字段。**刻意不走 getRect()**：那个取值器自己也在被测，拿它当检查
// 手段会让"getRect 坏了"这一整类缺陷自己把自己藏起来。x()/y()/width()/height()
// 是四条各自独立的一行取值器，用它们当地基（right()/bottom() 带加法，不当地基）。
bool fieldsAre(const PkRectF &r, double x, double y, double w, double h)
{
    return sameD(r.x(), x) && sameD(r.y(), y)
        && sameD(r.width(), w) && sameD(r.height(), h);
}

// PkRect 侧断言内部四坐标，与 test_rect.cpp 的 coordsAre 同形。
bool coordsAre(const PkRect &r, int x1, int y1, int x2, int y2)
{
    return r.left() == x1 && r.top() == y1 && r.right() == x2 && r.bottom() == y2;
}

} // namespace

// ═══ 构造与布局 ═══════════════════════════════════════════════════════════

void PkRectFCase::rectfDefaultIsAllZero()
{
    // ⚠ 实测真 Qt：QRectF() 是 (0,0,0,0) —— **不是** QRect() 的 (0,0,-1,-1) 哨兵。
    const PkRectF d;
    PK_VERIFY(fieldsAre(d, 0.0, 0.0, 0.0, 0.0));
    PK_VERIFY(d.isNull());
    PK_VERIFY(d.isEmpty());
    PK_VERIFY(!d.isValid());
    // 同一句话的另一面：两个族的默认值不互相等价。
    PK_VERIFY(!coordsAre(PkRect(), 0, 0, 0, 0));
    PK_VERIFY(coordsAre(PkRect(), 0, 0, -1, -1));
}

void PkRectFCase::rectfFiveConstructors()
{
    // (left, top, width, height) —— 直接摆四个字段，**没有 +w-1**
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4), 1, 2, 3, 4));
    PK_VERIFY(fieldsAre(PkRectF(5, 5, -3, -3), 5, 5, -3, -3));
    // (topLeft, size) —— 同样直接摆
    PK_VERIFY(fieldsAre(PkRectF(PkPointF(1, 2), PkSizeF(3, 4)), 1, 2, 3, 4));
    // (topLeft, bottomRight) —— **唯一做减法的那个**
    PK_VERIFY(fieldsAre(PkRectF(PkPointF(1, 2), PkPointF(5, 6)), 1, 2, 4, 4));
    // (PkRect) —— 走 x()/y()/width()/height()
    PK_VERIFY(fieldsAre(PkRectF(PkRect(0, 0, 10, 10)), 0, 0, 10, 10));

    // PkSizeF 的隐式提升在这里也能用（PkSize → PkSizeF）
    PK_VERIFY(fieldsAre(PkRectF(PkPointF(1, 2), PkSizeF(PkSize(3, 4))), 1, 2, 3, 4));
}

void PkRectFCase::rectfLayoutIsFourQreal()
{
    // 实测 sizeof(QRectF)==32：四个 double，没有额外成员/虚表。
    PK_COMPARE(sizeof(PkRectF), sizeof(qreal) * 4);
    PK_VERIFY(std::is_trivially_copyable<PkRectF>::value);
    PK_VERIFY(std::is_standard_layout<PkRectF>::value);
    // ⚠ 提升方向是单向的：PkRect → PkRectF 隐式，反向只能走 toRect/toAlignedRect。
    PK_VERIFY((std::is_convertible<PkRect, PkRectF>::value));
    PK_VERIFY(!(std::is_convertible<PkRectF, PkRect>::value));
}

// ═══ 差一：本 Task 与 Task 4 最尖锐的那条分界 ══════════════════════════════

void PkRectFCase::rectfRightBottomHaveNoOffByOne()
{
    // 实测真 Qt：QRectF(0,0,10,10) 的 right()==10 bottom()==10。
    const PkRectF r(0, 0, 10, 10);
    PK_COMPARE(r.left(), 0.0);
    PK_COMPARE(r.top(), 0.0);
    PK_COMPARE(r.right(), 10.0);
    PK_COMPARE(r.bottom(), 10.0);
    PK_COMPARE(r.width(), 10.0);
    PK_COMPARE(r.height(), 10.0);
    // right() 就是 x + w，任何输入都成立（含负宽）
    PK_COMPARE(PkRectF(5, 5, -3, -3).right(), 2.0);
    PK_COMPARE(PkRectF(5, 5, -3, -3).bottom(), 2.0);
}

void PkRectFCase::rectfSideBySideWithPkRect()
{
    // 把两族并排钉住：**同样的 (0,0,10,10)，right 一个 9 一个 10**。
    // 这是 Krita 里最常见的一格误差来源，两边各自照抄 Qt 才算对齐。
    PK_COMPARE(PkRect(0, 0, 10, 10).right(), 9);
    PK_COMPARE(PkRectF(0, 0, 10, 10).right(), 10.0);
    // 提升之后仍是 10 —— 因为提升走的是 width()（差一已经在 width 里算过了）
    PK_COMPARE(PkRectF(PkRect(0, 0, 10, 10)).right(), 10.0);
}

// ═══ 三谓词 ═══════════════════════════════════════════════════════════════

void PkRectFCase::rectfThreePredicatesUseFloatThresholds()
{
    // 实测表（probe_rectf.cpp §B）：
    //   (0,0,0,0)   isNull=1 isEmpty=1 isValid=0
    //   (0,0,1,1)   isNull=0 isEmpty=0 isValid=1
    //   (0,0,10,0)  isNull=0 isEmpty=1 isValid=0
    //   (0,0,-1,-1) isNull=0 isEmpty=1 isValid=0
    //   (5,5,0,0)   isNull=1 isEmpty=1 isValid=0   ← isNull 与位置无关
    struct { double x, y, w, h; bool n, e, v; } cs[] = {
        { 0, 0,  0,  0, true,  true,  false },
        { 0, 0,  1,  1, false, false, true  },
        { 0, 0, 10,  0, false, true,  false },
        { 0, 0, -1, -1, false, true,  false },
        { 5, 5,  0,  0, true,  true,  false },
    };
    for (const auto &c : cs) {
        const PkRectF r(c.x, c.y, c.w, c.h);
        PK_COMPARE(r.isNull(), c.n);
        PK_COMPARE(r.isEmpty(), c.e);
        PK_COMPARE(r.isValid(), c.v);
    }

    // ⚠ **门槛与 PkRect 完全不同**，抄过来会把整片退化矩形判反：
    //   PkRectF 的 isEmpty 是 `w <= 0`，宽 0.5 的矩形**非空**；
    //   PkRect  的 isEmpty 等价于 `宽 < 1`，宽 0 才空（整数里没有 0.5）。
    PK_VERIFY(!PkRectF(0, 0, 0.5, 0.5).isEmpty());
    PK_VERIFY(PkRectF(0, 0, 0.5, 0.5).isValid());
    // 而 isValid 是严格 `> 0`：宽恰为 0 不算 valid（PkSize(0,0).isValid() 是 true，
    // 别把 Size 那套抄过来 —— 这条与 test_rect.cpp 的同名断言是一对）
    PK_VERIFY(!PkRectF(0, 0, 0, 1).isValid());
    PK_VERIFY(PkSize(0, 0).isValid());
}

void PkRectFCase::rectfPredicatesOnSignedZeroAndSubnormal()
{
    // 实测：(0,0,-0.0,-0.0) isNull=1（-0.0 == 0. 为真）
    PK_VERIFY(PkRectF(0, 0, -0.0, -0.0).isNull());
    PK_VERIFY(PkRectF(0, 0, -0.0, -0.0).isEmpty());
    PK_VERIFY(!PkRectF(0, 0, -0.0, -0.0).isValid());
    // 实测：(0,0,-0.0,1) isNull=0 isEmpty=1 isValid=0
    PK_VERIFY(!PkRectF(0, 0, -0.0, 1).isNull());
    PK_VERIFY(PkRectF(0, 0, -0.0, 1).isEmpty());
    PK_VERIFY(!PkRectF(0, 0, -0.0, 1).isValid());
    // 实测：次正规也算正宽高 —— isEmpty=0 isValid=1
    PK_VERIFY(!PkRectF(0, 0, kSub, kSub).isNull());
    PK_VERIFY(!PkRectF(0, 0, kSub, kSub).isEmpty());
    PK_VERIFY(PkRectF(0, 0, kSub, kSub).isValid());
}

void PkRectFCase::rectfPredicatesAreNotComplementsOnNan()
{
    // ⚠ 实测真 Qt：(0,0,nan,1) 与 (0,0,nan,nan) 的
    // **isEmpty 与 isValid 同时为假** —— NaN 让 `<=` 与 `>` 两个比较都不成立。
    // 把 isValid 写成 `!isEmpty()` 会在这一整片输入上分家。
    for (const PkRectF r : { PkRectF(0, 0, kNaN, 1), PkRectF(0, 0, kNaN, kNaN),
                             PkRectF(0, 0, 1, kNaN) }) {
        PK_VERIFY(!r.isNull());
        PK_VERIFY(!r.isEmpty());
        PK_VERIFY(!r.isValid());
    }
    // inf 那侧则是正常互补：(0,0,inf,inf) valid、(0,0,-inf,1) empty
    PK_VERIFY(PkRectF(0, 0, kInf, kInf).isValid());
    PK_VERIFY(!PkRectF(0, 0, kInf, kInf).isEmpty());
    PK_VERIFY(PkRectF(0, 0, -kInf, 1).isEmpty());
    PK_VERIFY(!PkRectF(0, 0, -kInf, 1).isValid());
}

// ═══ 取值器 ═══════════════════════════════════════════════════════════════

void PkRectFCase::rectfCornersAndSize()
{
    // 实测：topLeft=(0,0) topRight=(10,0) bottomLeft=(0,10) bottomRight=(10,10)
    const PkRectF r(0, 0, 10, 10);
    PK_VERIFY(r.topLeft() == PkPointF(0, 0));
    PK_VERIFY(r.topRight() == PkPointF(10, 0));
    PK_VERIFY(r.bottomLeft() == PkPointF(0, 10));
    PK_VERIFY(r.bottomRight() == PkPointF(10, 10));
    // size() 原样返回 w/h，负宽高照传（实测 (1,2,-3,-4).size() = -3x-4，isValid=0）
    PK_VERIFY(sameD(PkRectF(1, 2, -3, -4).size().width(), -3.0));
    PK_VERIFY(sameD(PkRectF(1, 2, -3, -4).size().height(), -4.0));
    PK_VERIFY(!PkRectF(1, 2, -3, -4).size().isValid());
}

void PkRectFCase::rectfCenterIsHalfWidthNotTruncated()
{
    // 实测：(0,0,10,10).center() = (5,5)
    PK_VERIFY(PkRectF(0, 0, 10, 10).center() == PkPointF(5, 5));
    // ⚠ **不截断**（PkRect 那边向原点侧截断，(0,0,10,10).center().x() == 4）
    PK_COMPARE(PkRectF(0, 0, 11, 11).center().x(), 5.5);
    PK_COMPARE(PkRect(0, 0, 10, 10).center().x(), 4);
    // 公式是 `xp + w/2`（先除后加），不是 (left+right)/2 —— 在极大值上两者分家
    PK_VERIFY(sameD(PkRectF(1e308, 0, 1e308, 1).center().x(), 1e308 + 1e308 / 2));
}

// ═══ 修改器 ═══════════════════════════════════════════════════════════════

void PkRectFCase::rectfSetEdgesKeepOppositeEdge()
{
    // 实测（probe_rectf.cpp §G，底座 (1,2,3,4)）：
    //   setLeft(0)   -> x=0 y=2 w=4 h=4     ← 保右边界，宽度跟着变
    //   setRight(0)  -> x=1 y=2 w=-1 h=4
    //   setTop(0)    -> x=1 y=0 w=3 h=6
    //   setBottom(0) -> x=1 y=2 w=3 h=-2
    const PkRectF base(1, 2, 3, 4);
    { PkRectF r = base; r.setLeft(0);   PK_VERIFY(fieldsAre(r, 0, 2, 4, 4)); }
    { PkRectF r = base; r.setRight(0);  PK_VERIFY(fieldsAre(r, 1, 2, -1, 4)); }
    { PkRectF r = base; r.setTop(0);    PK_VERIFY(fieldsAre(r, 1, 0, 3, 6)); }
    { PkRectF r = base; r.setBottom(0); PK_VERIFY(fieldsAre(r, 1, 2, 3, -2)); }

    // ⚠ 与 PkRect 并排：那边 setLeft 只摆一个坐标，**宽度会变**成另一个值。
    // 两族在这里的语义完全相反，抄错的表现是"矩形被拉走一格"而不是编译错误。
    { PkRect q(1, 2, 3, 4); q.setLeft(0); PK_VERIFY(coordsAre(q, 0, 2, 3, 5)); }
}

void PkRectFCase::rectfSetXSetYAreSetLeftSetTop()
{
    // 实测：setX(0) 与 setLeft(0) 结果逐字相同（x=0 w=4），setY/setTop 同。
    const PkRectF base(1, 2, 3, 4);
    { PkRectF a = base, b = base; a.setX(0); b.setLeft(0);
      PK_VERIFY(fieldsAre(a, 0, 2, 4, 4)); PK_VERIFY(fieldsAre(b, 0, 2, 4, 4)); }
    { PkRectF a = base, b = base; a.setY(0); b.setTop(0);
      PK_VERIFY(fieldsAre(a, 1, 0, 3, 6)); PK_VERIFY(fieldsAre(b, 1, 0, 3, 6)); }
}

void PkRectFCase::rectfSetTopLeftAndBottomRight()
{
    // 实测：setTopLeft(0,0) -> (0,0,4,6)；setBottomRight(0,0) -> (1,2,-1,-2)
    const PkRectF base(1, 2, 3, 4);
    { PkRectF r = base; r.setTopLeft(PkPointF(0, 0));
      PK_VERIFY(fieldsAre(r, 0, 0, 4, 6)); }
    { PkRectF r = base; r.setBottomRight(PkPointF(0, 0));
      PK_VERIFY(fieldsAre(r, 1, 2, -1, -2)); }
}

void PkRectFCase::rectfSetWidthSetHeightAnchorTopLeft()
{
    // 实测：setWidth(9) -> (1,2,9,4)。直接写字段，锚定左上角。
    const PkRectF base(1, 2, 3, 4);
    { PkRectF r = base; r.setWidth(9);  PK_VERIFY(fieldsAre(r, 1, 2, 9, 4)); }
    { PkRectF r = base; r.setHeight(9); PK_VERIFY(fieldsAre(r, 1, 2, 3, 9)); }
    // 负宽高原样存下去，不翻正
    { PkRectF r = base; r.setWidth(-9); PK_VERIFY(fieldsAre(r, 1, 2, -9, 4)); }
}

void PkRectFCase::rectfSetSizeAnchorsTopLeft()
{
    // 实测：setSize(9,9) -> (1,2,9,9)
    PkRectF r(1, 2, 3, 4);
    r.setSize(PkSizeF(9, 9));
    PK_VERIFY(fieldsAre(r, 1, 2, 9, 9));
    // PkSize 的隐式提升也能用
    r.setSize(PkSizeF(PkSize(2, 3)));
    PK_VERIFY(fieldsAre(r, 1, 2, 2, 3));
}

void PkRectFCase::rectfSetRectAndGetRect()
{
    // 实测：setRect(9,8,7,6) -> (9,8,7,6)；(1,2,3,4).getRect = 1 2 3 4
    PkRectF r(1, 2, 3, 4);
    double a, b, c, d;
    r.getRect(&a, &b, &c, &d);
    PK_COMPARE(a, 1.0); PK_COMPARE(b, 2.0); PK_COMPARE(c, 3.0); PK_COMPARE(d, 4.0);
    r.setRect(9, 8, 7, 6);
    PK_VERIFY(fieldsAre(r, 9, 8, 7, 6));
}

void PkRectFCase::rectfSetCoordsAndGetCoords()
{
    // ⚠ getCoords 输出的是**边界**（x1,y1,x2,y2 = xp, yp, xp+w, yp+h），
    // getRect 输出的是 (x,y,w,h)。实测 (1,2,3,4).getCoords = 1 2 4 6。
    const PkRectF r(1, 2, 3, 4);
    double a, b, c, d;
    r.getCoords(&a, &b, &c, &d);
    PK_COMPARE(a, 1.0); PK_COMPARE(b, 2.0); PK_COMPARE(c, 4.0); PK_COMPARE(d, 6.0);
    // 实测：setCoords(9,8,7,6) -> x=9 y=8 w=-2 h=-2（存的是差）
    PkRectF s(1, 2, 3, 4);
    s.setCoords(9, 8, 7, 6);
    PK_VERIFY(fieldsAre(s, 9, 8, -2, -2));
}

// ═══ 平移一族 ═════════════════════════════════════════════════════════════

void PkRectFCase::rectfMoveToKeepsSize()
{
    // 实测：(1,2,3,4).moveTo(9,9) -> (9,9,3,4)。两个重载同结果。
    { PkRectF r(1, 2, 3, 4); r.moveTo(9, 9); PK_VERIFY(fieldsAre(r, 9, 9, 3, 4)); }
    { PkRectF r(1, 2, 3, 4); r.moveTo(PkPointF(9, 9)); PK_VERIFY(fieldsAre(r, 9, 9, 3, 4)); }
}

void PkRectFCase::rectfMoveLeftMoveTopKeepSize()
{
    // 实测：moveLeft(0) -> (0,2,3,4)；moveTop(0) -> (1,0,3,4)。宽高不变。
    { PkRectF r(1, 2, 3, 4); r.moveLeft(0); PK_VERIFY(fieldsAre(r, 0, 2, 3, 4)); }
    { PkRectF r(1, 2, 3, 4); r.moveTop(0);  PK_VERIFY(fieldsAre(r, 1, 0, 3, 4)); }
    // ⚠ 与 setLeft 的对照：同一个实参，move* 保宽高、set* 不保。
    { PkRectF r(1, 2, 3, 4); r.setLeft(0);  PK_VERIFY(fieldsAre(r, 0, 2, 4, 4)); }
}

void PkRectFCase::rectfMoveTopLeftIsTwoMoves()
{
    // 实测：moveTopLeft(0,0) -> (0,0,3,4)（= moveLeft + moveTop，不是 setTopLeft）
    PkRectF r(1, 2, 3, 4);
    r.moveTopLeft(PkPointF(0, 0));
    PK_VERIFY(fieldsAre(r, 0, 0, 3, 4));
    PkRectF s(1, 2, 3, 4);
    s.setTopLeft(PkPointF(0, 0));
    PK_VERIFY(fieldsAre(s, 0, 0, 4, 6));       // 对照：setTopLeft 会改宽高
}

void PkRectFCase::rectfMoveCenterUsesHalfWidth()
{
    // 实测：(1,2,3,4).moveCenter(0,0) -> x=-1.5 y=-2 w=3 h=4
    PkRectF r(1, 2, 3, 4);
    r.moveCenter(PkPointF(0, 0));
    PK_VERIFY(fieldsAre(r, -1.5, -2, 3, 4));
    // ⚠ 与 PkRect 的对照：那边用的是**不带 +1 的跨距** x2-x1，取值不同。
    PkRect q(0, 0, 11, 11);
    q.moveCenter(PkPoint(0, 0));
    PK_VERIFY(coordsAre(q, -5, -5, 5, 5));
    // 浮点版真的能回到中心（整数版会截断）
    PkRectF t(0, 0, 11, 11);
    t.moveCenter(PkPointF(0, 0));
    PK_VERIFY(t.center() == PkPointF(0, 0));
}

void PkRectFCase::rectfTranslateAndTranslated()
{
    // 实测：(1,2,3,4).translated(1,1) -> (2,3,3,4)
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4).translated(1, 1), 2, 3, 3, 4));
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4).translated(PkPointF(1, 1)), 2, 3, 3, 4));
    { PkRectF r(1, 2, 3, 4); r.translate(1, 1); PK_VERIFY(fieldsAre(r, 2, 3, 3, 4)); }
    { PkRectF r(1, 2, 3, 4); r.translate(PkPointF(1, 1)); PK_VERIFY(fieldsAre(r, 2, 3, 3, 4)); }
}

// ═══ adjust ═══════════════════════════════════════════════════════════════

void PkRectFCase::rectfAdjustUsesDeltaOfDeltas()
{
    // ⚠ 实测：(1,2,3,4).adjust(1,1,1,1) -> (2,3,3,4) —— **宽高不变**，
    // 因为宽的增量是 `xp2 - xp1`（两个增量之差）而不是 `+ xp2`。
    { PkRectF r(1, 2, 3, 4); r.adjust(1, 1, 1, 1); PK_VERIFY(fieldsAre(r, 2, 3, 3, 4)); }
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4).adjusted(1, 1, 1, 1), 2, 3, 3, 4));
    // 只动右下：宽高才真的变
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4).adjusted(0, 0, 2, 2), 1, 2, 5, 6));
    // 只动左上：位置与宽高同时变（左边界外扩 = 宽增大）
    PK_VERIFY(fieldsAre(PkRectF(1, 2, 3, 4).adjusted(-1, -1, 0, 0), 0, 1, 4, 5));
    // 能把矩形调成退化的
    PK_VERIFY(PkRectF(0, 0, 2, 2).adjusted(1, 1, -1, -1).isEmpty());
}

// ═══ normalized ═══════════════════════════════════════════════════════════

void PkRectFCase::rectfNormalizedSwapBoundaryIsStrictlyNegative()
{
    // 实测（probe_rectf.cpp §C）：
    //   (0,0,-1,1)   -> x=-1   w=1
    //   (0,0,-0.5,1) -> x=-0.5 w=0.5
    //   (0,0,0,1)    -> x=0    w=0     ← 宽恰为 0 **不交换**
    //   (2,3,-4,-5)  -> x=-2 y=-2 w=4 h=5
    PK_VERIFY(fieldsAre(PkRectF(0, 0, -1, 1).normalized(), -1, 0, 1, 1));
    PK_VERIFY(fieldsAre(PkRectF(0, 0, -0.5, 1).normalized(), -0.5, 0, 0.5, 1));
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 0, 1).normalized(), 0, 0, 0, 1));
    PK_VERIFY(fieldsAre(PkRectF(2, 3, -4, -5).normalized(), -2, -2, 4, 5));
    // ⚠ 条件是 `w < 0`，不是 PkRect 的 `x2 < x1 - 1` —— 后者在浮点上把"宽 -1
    // 以内"的矩形都当成不交换，整片行为都会变。任意小的负宽都交换：
    PK_VERIFY(fieldsAre(PkRectF(0, 0, -kSub, 1).normalized(), -kSub, 0, kSub, 1));
}

void PkRectFCase::rectfNormalizedOnSpecialValues()
{
    // ⚠ 实测：-0.0 **不满足 `< 0`**，所以不交换，且 w 仍是 -0.0（位保留）。
    const PkRectF nz = PkRectF(0, 0, -0.0, 1).normalized();
    PK_VERIFY(fieldsAre(nz, 0.0, 0.0, -0.0, 1.0));
    PK_VERIFY(std::signbit(nz.width()));
    // NaN 原样返回（nan < 0 为假）
    PK_VERIFY(fieldsAre(PkRectF(0, 0, kNaN, 1).normalized(), 0, 0, kNaN, 1));
    // -inf 交换：x = 0 + (-inf) = -inf，w = inf
    PK_VERIFY(fieldsAre(PkRectF(0, 0, -kInf, 1).normalized(), -kInf, 0, kInf, 1));
    // +inf 不交换
    PK_VERIFY(fieldsAre(PkRectF(0, 0, kInf, 1).normalized(), 0, 0, kInf, 1));
    // 极大值上交换后仍是有限量（实测 (1e308,0,-1e308,1) -> x=0 w=1e308）
    PK_VERIFY(fieldsAre(PkRectF(1e308, 0, -1e308, 1).normalized(), 0, 0, 1e308, 1));
}

// ═══ 集合运算 ═════════════════════════════════════════════════════════════

void PkRectFCase::rectfUnitedWithNullIsAsymmetric()
{
    // ⚠ 实测（probe_rectf.cpp §E）：**operator| 不可交换**，判空用 isNull()
    // 且"a 为 null 返回 b"在前 —— 两侧都 null 时永远返回 b。
    const PkRectF nA(0, 0, 0, 0), nB(5, 5, 0, 0), big(0, 0, 10, 10);
    PK_VERIFY(fieldsAre(nA | nB, 5, 5, 0, 0));
    PK_VERIFY(fieldsAre(nB | nA, 0, 0, 0, 0));
    PK_VERIFY(fieldsAre(nA | big, 0, 0, 10, 10));
    PK_VERIFY(fieldsAre(big | nA, 0, 0, 10, 10));
    PK_VERIFY(fieldsAre(big | nB, 0, 0, 10, 10));
    // ⚠ 判空是 isNull() 不是 isEmpty()：宽 0 高 10 的矩形**不是 null**，
    // 于是它真的参与并集（(0,0,0,10) | (0,0,10,10) 不会退化成返回 b）。
    PK_VERIFY(fieldsAre(PkRectF(-5, 0, 0, 10) | big, -5, 0, 15, 10));
}

void PkRectFCase::rectfUnitedNormalizesNegativeDims()
{
    // 实测：(0,0,10,10) | (0,0,-1,-1) = (-1,-1,11,11)
    //       (0,0,10,10) | (5,5,-3,-3) = (0,0,10,10)   ← b 翻正后是 (2,2,3,3)，在内部
    //       (5,5,-3,-3) | (5,5,-3,-3) = (2,2,3,3)
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) | PkRectF(0, 0, -1, -1), -1, -1, 11, 11));
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) | PkRectF(5, 5, -3, -3), 0, 0, 10, 10));
    PK_VERIFY(fieldsAre(PkRectF(5, 5, -3, -3) | PkRectF(5, 5, -3, -3), 2, 2, 3, 3));
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) | PkRectF(20, 20, 5, 5), 0, 0, 25, 25));
    // united 是 | 的别名
    PK_VERIFY((PkRectF(0, 0, 10, 10).united(PkRectF(20, 20, 5, 5)))
              == (PkRectF(0, 0, 10, 10) | PkRectF(20, 20, 5, 5)));
}

void PkRectFCase::rectfIntersectedOnDegenerate()
{
    // ⚠ 实测：**任一侧退化成线（某轴 l == r）就返回 PkRectF()**，
    // 而且判据不是 isNull() —— (0,0,0,10) 不 null，但 x 轴 l==r，& 照样返回空。
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 0, 10) & PkRectF(0, 0, 10, 10), 0, 0, 0, 0));
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 0, 0) & PkRectF(0, 0, 10, 10), 0, 0, 0, 0));
    // 实测：(0,0,10,10) & (5,5,-3,-3) = (2,2,3,3)  ← 负宽高先翻正再求交
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) & PkRectF(5, 5, -3, -3), 2, 2, 3, 3));
    // 实测：(0,0,-1,-1) & (0,0,-1,-1) = (-1,-1,1,1)
    PK_VERIFY(fieldsAre(PkRectF(0, 0, -1, -1) & PkRectF(0, 0, -1, -1), -1, -1, 1, 1));
    // 实测：(0,0,10,10) & (0,0,-1,-1) = (0,0,0,0)  ← 翻正后是 [-1,0]，与 [0,10] 只碰一点
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) & PkRectF(0, 0, -1, -1), 0, 0, 0, 0));
    // 不相交
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) & PkRectF(20, 20, 5, 5), 0, 0, 0, 0));
    // intersected 是 & 的别名
    PK_VERIFY((PkRectF(0, 0, 10, 10).intersected(PkRectF(5, 5, -3, -3)))
              == (PkRectF(0, 0, 10, 10) & PkRectF(5, 5, -3, -3)));
}

void PkRectFCase::rectfIntersectsOnDegenerate()
{
    // 实测：与 & 的判据同构 —— 相邻（只碰一条边）算**不相交**（`l1 >= r2`）。
    PK_VERIFY(PkRectF(0, 0, 10, 10).intersects(PkRectF(5, 5, 1, 1)));
    PK_VERIFY(!PkRectF(0, 0, 10, 10).intersects(PkRectF(10, 0, 5, 5)));
    PK_VERIFY(!PkRectF(0, 0, 10, 10).intersects(PkRectF(0, 0, 0, 0)));
    PK_VERIFY(!PkRectF(0, 0, 10, 10).intersects(PkRectF(0, 0, -1, -1)));
    PK_VERIFY(PkRectF(0, 0, 10, 10).intersects(PkRectF(5, 5, -3, -3)));
    PK_VERIFY(PkRectF(0, 0, -1, -1).intersects(PkRectF(0, 0, -1, -1)));
}

void PkRectFCase::rectfOperatorAssignMatchesNamed()
{
    const PkRectF a(0, 0, 10, 10), b(5, 5, 8, 8);
    { PkRectF r = a; r |= b; PK_VERIFY(r == (a | b)); }
    { PkRectF r = a; r &= b; PK_VERIFY(r == (a & b)); }
    // 退化输入上也一致（转发那一跳自己也可能坏）
    { PkRectF r(0, 0, 0, 0); r |= b; PK_VERIFY(fieldsAre(r, 5, 5, 8, 8)); }
    { PkRectF r(0, 0, 0, 0); r &= b; PK_VERIFY(fieldsAre(r, 0, 0, 0, 0)); }
}

void PkRectFCase::rectfSetOpsOnNanShortCircuit()
{
    // ⚠ 实测（probe_rectf2.cpp）：NaN 让 `l1 == r1` 与 `l1 >= r2` 全部为假，
    // 于是**含 NaN 的输入一路走到最后**，intersects 基本恒真。
    // 这不是我们加的分支，是 `<`/`>`/`==` 在 NaN 上的定义 —— 换成
    // `if (l1 >= r1) return ...` 这类"看起来等价"的写法会在这一整片上分家。
    PK_VERIFY(PkRectF(0, 0, kNaN, 1).intersects(PkRectF(0, 0, 10, 10)));
    PK_VERIFY(PkRectF(kNaN, 0, 1, 1).intersects(PkRectF(0, 0, 10, 10)));
    PK_VERIFY(PkRectF(0, 0, 10, 10).intersects(PkRectF(0, 0, 1, kNaN)));
    // 实测：(0,0,nan,1) & (0,0,10,10) = (0,0,10,1)
    PK_VERIFY(fieldsAre(PkRectF(0, 0, kNaN, 1) & PkRectF(0, 0, 10, 10), 0, 0, 10, 1));
    // 实测：(0,0,10,10) | (0,0,nan,1) = (0,0,10,10)
    PK_VERIFY(fieldsAre(PkRectF(0, 0, 10, 10) | PkRectF(0, 0, kNaN, 1), 0, 0, 10, 10));
    // 实测：(0,0,inf,inf) & (0,0,10,10) = (0,0,10,10)
    PK_VERIFY(fieldsAre(PkRectF(0, 0, kInf, kInf) & PkRectF(0, 0, 10, 10), 0, 0, 10, 10));
}

// ═══ contains ═════════════════════════════════════════════════════════════

void PkRectFCase::rectfContainsPointIsClosedInterval()
{
    // ⚠ 实测：区间是**闭**的 —— (0,0,10,10).contains(10,10) 为**真**。
    // PkRect 那边因为差一，contains(QPoint(10,10)) 是**假**。两族在这里相反。
    const PkRectF r(0, 0, 10, 10);
    PK_VERIFY(r.contains(PkPointF(0, 0)));
    PK_VERIFY(r.contains(PkPointF(10, 10)));
    PK_VERIFY(r.contains(PkPointF(10, 0)));
    PK_VERIFY(r.contains(PkPointF(5, 5)));
    PK_VERIFY(!r.contains(PkPointF(10.0000001, 5)));
    PK_VERIFY(!r.contains(PkPointF(-1, 5)));
    PK_VERIFY(!PkRect(0, 0, 10, 10).contains(PkPoint(10, 10)));   // 对照
    // 两个标量的重载转发到点版（**转发那一跳自己也可能坏**）
    PK_VERIFY(r.contains(10.0, 10.0));
    PK_VERIFY(!r.contains(10.0000001, 5.0));
    // 退化：宽为 0 的矩形连自己边上的点都不含（实测 (3,3,0,5).contains(3,4)=0）
    PK_VERIFY(!PkRectF(3, 3, 0, 5).contains(PkPointF(3, 4)));
}

void PkRectFCase::rectfContainsPointOnNegativeDims()
{
    // 实测：(0,0,-10,-10) 翻正后是 [-10,0]×[-10,0]
    const PkRectF n(0, 0, -10, -10);
    PK_VERIFY(n.contains(PkPointF(0, 0)));
    PK_VERIFY(n.contains(PkPointF(-5, -5)));
    PK_VERIFY(!n.contains(PkPointF(5, 5)));
    PK_VERIFY(!n.contains(PkPointF(10, 10)));
    PK_VERIFY(!n.contains(PkPointF(-1, 5)));
}

void PkRectFCase::rectfContainsRectIsNotIntersects()
{
    // 实测（probe_rectf.cpp §E）：contains 与 intersects 在"相交但不包含"上分家。
    const PkRectF big(0, 0, 10, 10), part(5, 5, 10, 10), inner(2, 2, 3, 3);
    PK_VERIFY(big.contains(big));
    PK_VERIFY(big.contains(inner));
    PK_VERIFY(!big.contains(part));
    PK_VERIFY(big.intersects(part));            // ← 这一对就是判据抄串会露馅的地方
    // 实测：(0,0,10,10).contains((5,5,-3,-3)) = 1（b 翻正后是 (2,2,3,3)）
    PK_VERIFY(big.contains(PkRectF(5, 5, -3, -3)));
    PK_VERIFY(!PkRectF(5, 5, -3, -3).contains(big));
    // 退化侧一律假（实测 (3,3,0,5).contains(自身) = 0）
    PK_VERIFY(!PkRectF(3, 3, 0, 5).contains(PkRectF(3, 3, 0, 5)));
    PK_VERIFY(!big.contains(PkRectF(1, 1, 0, 0)));
    // ⚠ contains **没有 proper 参数**（PkRect 那边有）—— 这条靠编译期：
    // 三个重载各自的实参个数都不含 bool。
    PK_VERIFY(!big.contains(PkRectF(0, 0, 20, 20)));
}

void PkRectFCase::rectfContainsOnNan()
{
    // ⚠ 实测：(0,0,10,10).contains(nan,5) = **1**，contains(inf,5) = 0。
    // 因为排除条件是 `p.x() < l || p.x() > r`，NaN 让两个比较都为假。
    PK_VERIFY(PkRectF(0, 0, 10, 10).contains(PkPointF(kNaN, 5)));
    PK_VERIFY(!PkRectF(0, 0, 10, 10).contains(PkPointF(kInf, 5)));
    PK_VERIFY(!PkRectF(0, 0, 10, 10).contains(PkPointF(-kInf, 5)));
}

// ═══ 取整：toRect vs toAlignedRect ═════════════════════════════════════════

void PkRectFCase::rectfToRectRoundsFourEdges()
{
    // ⚠ 实测（probe_rectf.cpp §D）：**取整发生在四条边上**，不是对 x/y/w/h 各取
    // 一次。内部坐标（不是 x/y/w/h）才看得清这一点：
    //   (-1.5,-1.5,1,1) -> coords(-1,-1,-1,-1)
    //   (-0.5,-0.5,1,1) -> coords( 0, 0, 0, 0)
    //   ( 0.5, 0.5,1,1) -> coords( 1, 1, 1, 1)
    //   ( 2.5, 2.5,1,1) -> coords( 3, 3, 3, 3)
    //   ( 0,0,10,10)    -> coords( 0, 0, 9, 9)
    PK_VERIFY(coordsAre(PkRectF(-1.5, -1.5, 1, 1).toRect(), -1, -1, -1, -1));
    PK_VERIFY(coordsAre(PkRectF(-0.5, -0.5, 1, 1).toRect(), 0, 0, 0, 0));
    PK_VERIFY(coordsAre(PkRectF(0.5, 0.5, 1, 1).toRect(), 1, 1, 1, 1));
    PK_VERIFY(coordsAre(PkRectF(2.5, 2.5, 1, 1).toRect(), 3, 3, 3, 3));
    PK_VERIFY(coordsAre(PkRectF(0, 0, 10, 10).toRect(), 0, 0, 9, 9));
    // ⚠ 这一条把"分开对 w 取整"这个误解钉死：实测 coords(1,0,1,0)。
    // 左边界 qRound(0.49999999999999994) 进位到 1；而 xp+w 在 double 里恰好舍入
    // 成 1.5，qRound 给 2、减 1 得 1。对 w=1 单独取整得不到这个结果。
    PK_VERIFY(coordsAre(PkRectF(0.49999999999999994, 0, 1, 1).toRect(), 1, 0, 1, 0));
    // 退化矩形照样往下传：(0,0,0,0) -> coords(0,0,-1,-1)（正好是 PkRect() 的形状）
    PK_VERIFY(coordsAre(PkRectF(0, 0, 0, 0).toRect(), 0, 0, -1, -1));
    PK_VERIFY(PkRectF(0, 0, 0, 0).toRect().isNull());
    // 负宽高不翻正：(0,0,-1,-1) -> coords(0,0,-2,-2)
    PK_VERIFY(coordsAre(PkRectF(0, 0, -1, -1).toRect(), 0, 0, -2, -2));
}

void PkRectFCase::rectfToAlignedRectExpandsOutward()
{
    // 实测：floor 左上、ceil 右下，向外扩。
    //   (-1.5,-1.5,1,1) -> coords(-2,-2,-1,-1)  即 (x=-2,y=-2,w=2,h=2)
    //   (-0.5,-0.5,1,1) -> coords(-1,-1, 0, 0)
    //   ( 0.5, 0.5,1,1) -> coords( 0, 0, 1, 1)
    //   ( 2.5, 2.5,1,1) -> coords( 2, 2, 3, 3)
    //   (-2.5,0,0.5,1)  -> coords(-3,0,-3,0)
    PK_VERIFY(coordsAre(PkRectF(-1.5, -1.5, 1, 1).toAlignedRect(), -2, -2, -1, -1));
    PK_VERIFY(coordsAre(PkRectF(-0.5, -0.5, 1, 1).toAlignedRect(), -1, -1, 0, 0));
    PK_VERIFY(coordsAre(PkRectF(0.5, 0.5, 1, 1).toAlignedRect(), 0, 0, 1, 1));
    PK_VERIFY(coordsAre(PkRectF(2.5, 2.5, 1, 1).toAlignedRect(), 2, 2, 3, 3));
    PK_VERIFY(coordsAre(PkRectF(-2.5, 0, 0.5, 1).toAlignedRect(), -3, 0, -3, 0));
    // 宽高读数：向外扩之后宽从 1 变 2
    PK_COMPARE(PkRectF(-1.5, -1.5, 1, 1).toAlignedRect().width(), 2);
    PK_COMPARE(PkRectF(-1.5, -1.5, 1, 1).toRect().width(), 1);
    // 零宽也会被扩成 1（实测 (1.2,3.4,0,0) -> coords(1,3,1,3)，即 1x1）
    PK_VERIFY(coordsAre(PkRectF(1.2, 3.4, 0, 0).toAlignedRect(), 1, 3, 1, 3));
    PK_COMPARE(PkRectF(1.2, 3.4, 0, 0).toAlignedRect().width(), 1);
    PK_COMPARE(PkRectF(1.2, 3.4, 0, 0).toRect().width(), 0);
}

void PkRectFCase::rectfToAlignedRectCeilDoesNotBumpOnExactInteger()
{
    // ⚠ 实测：边界恰为整数时 ceil **不进位** —— (0,0,10,10) 原样得到 (0,0,10,10)。
    // 写成 `floor(right) + 1` 会在这里多一格，而这正是 toAlignedRect 的 64 个
    // 调用点（裁剪/缓存矩形）最在意的一格。
    PK_VERIFY(coordsAre(PkRectF(0, 0, 10, 10).toAlignedRect(), 0, 0, 9, 9));
    PK_COMPARE(PkRectF(0, 0, 10, 10).toAlignedRect().width(), 10);
    PK_VERIFY(coordsAre(PkRectF(1, 1, 0.5, 0.5).toAlignedRect(), 1, 1, 1, 1));
    PK_VERIFY(coordsAre(PkRectF(-0.0, -0.0, 1, 1).toAlignedRect(), 0, 0, 0, 0));
}

void PkRectFCase::rectfToRectAndToAlignedRectDiffer()
{
    // 两者在半值上系统性不同 —— 实测四组（§D）逐条对照。
    struct { double x; int r1, r2, a1, a2; } cs[] = {
        { -1.5, -1, -1, -2, -1 },
        { -0.5,  0,  0, -1,  0 },
        {  0.5,  1,  1,  0,  1 },
        {  2.5,  3,  3,  2,  3 },
    };
    for (const auto &c : cs) {
        const PkRectF r(c.x, c.x, 1, 1);
        PK_VERIFY(coordsAre(r.toRect(), c.r1, c.r1, c.r2, c.r2));
        PK_VERIFY(coordsAre(r.toAlignedRect(), c.a1, c.a1, c.a2, c.a2));
        PK_VERIFY(r.toRect() != r.toAlignedRect());
    }
    // 恰好整数边界时两者相同
    PK_VERIFY(PkRectF(0, 0, 10, 10).toRect() == PkRectF(0, 0, 10, 10).toAlignedRect());
}

void PkRectFCase::rectfToAlignedRectDoesNotNormalize()
{
    // ⚠ 实测：(0,0,-1,-1).toAlignedRect() 的内部坐标是 (0,0,-2,-2)（宽 -1），
    // **负宽高原样传给 PkRect 构造**，不翻正。
    PK_VERIFY(coordsAre(PkRectF(0, 0, -1, -1).toAlignedRect(), 0, 0, -2, -2));
    PK_COMPARE(PkRectF(0, 0, -1, -1).toAlignedRect().width(), -1);
    // (0,0,0,0) 同样：coords(0,0,-1,-1)，宽 0
    PK_VERIFY(coordsAre(PkRectF(0, 0, 0, 0).toAlignedRect(), 0, 0, -1, -1));
    // 越界与非有限：两侧都是 UB，实机上编成同一条 cvttsd2si。noFold 把转换压到
    // 运行期（编译期折叠给的是另一个答案，理由见 test_point.cpp 的 noFold）。
    // 这里只断言"不崩、能算出个东西"，取值口径写进 README 覆盖度缺口。
    const PkRect huge = PkRectF(noFold(1e10), 0, 1, 1).toAlignedRect();
    PK_VERIFY(huge.left() == INT_MIN || huge.left() == INT_MAX);
}

// ═══ 提升与相等 ═══════════════════════════════════════════════════════════

void PkRectFCase::rectfFromPkRectUsesWidthNotRight()
{
    // 实测（probe_rectf.cpp §H）：
    //   QRectF(QRect(0,0,10,10)) = (0,0,10,10)
    //   QRectF(QRect())          = (0,0,0,0)     ← 哨兵 (0,0,-1,-1) 的 w/h 是 0
    //   QRectF(QRect(5,5,0,0))   = (5,5,0,0)
    //   QRectF(coords(0,0,-2,-2))= (0,0,-1,-1)   ← 负宽照传
    PK_VERIFY(fieldsAre(PkRectF(PkRect(0, 0, 10, 10)), 0, 0, 10, 10));
    PK_VERIFY(fieldsAre(PkRectF(PkRect()), 0, 0, 0, 0));
    PK_VERIFY(fieldsAre(PkRectF(PkRect(5, 5, 0, 0)), 5, 5, 0, 0));
    { PkRect q; q.setCoords(0, 0, -2, -2);
      PK_VERIFY(fieldsAre(PkRectF(q), 0, 0, -1, -1)); }
    // 提升是隐式的：函数实参、赋值都不用写转换
    const PkRectF implicit = PkRect(1, 2, 3, 4);
    PK_VERIFY(fieldsAre(implicit, 1, 2, 3, 4));
    // ⚠ 往返不是恒等：PkRect(0,0,10,10) → PkRectF → toRect() 回得来，
    // 但 toAlignedRect 在半值上回不来（上面 rectfToRectAndToAlignedRectDiffer）。
    PK_VERIFY(PkRectF(PkRect(0, 0, 10, 10)).toRect() == PkRect(0, 0, 10, 10));
}

void PkRectFCase::rectfEqualityIsFuzzy()
{
    // 实测（probe_rectf.cpp §I）：== 是四个分量各一次 qFuzzyCompare。
    PK_VERIFY(PkRectF(1, 1, 1, 1) == PkRectF(1 + 1e-13, 1, 1, 1));
    PK_VERIFY(!(PkRectF(1, 1, 1, 1) == PkRectF(1 + 1e-11, 1, 1, 1)));
    // ⚠ 任一侧为 0 则**恒 false**（相对误差的右端取 min(|a|,|b|)）——
    // 于是 (0,0,1,1) != (1e-300,0,1,1)，尽管两者肉眼上一样。
    PK_VERIFY(!(PkRectF(0, 0, 1, 1) == PkRectF(1e-300, 0, 1, 1)));
    // 但四个分量全 0 时相等（0 与 0 的差是 0，0 <= 0 成立）
    PK_VERIFY(PkRectF(0, 0, 0, 0) == PkRectF(0, 0, 0, 0));
    PK_VERIFY(PkRectF(0, 0, 1, 1) == PkRectF(0, 0, 1, 1));
    // ⚠ 反直觉的两条，实测确认：inf 与自己**不等**、inf 与 -inf **相等**
    PK_VERIFY(!(PkRectF(kInf, 0, 1, 1) == PkRectF(kInf, 0, 1, 1)));
    PK_VERIFY(PkRectF(kInf, 0, 1, 1) == PkRectF(-kInf, 0, 1, 1));
    // -0.0 与 +0.0 相等（差是 0）
    PK_VERIFY(PkRectF(0, 0, -0.0, 1) == PkRectF(0, 0, 0.0, 1));
    // ⚠ 与 PkRect 的对照：那边是**位相等**，没有模糊比较
    PK_VERIFY(PkRect(1, 2, 3, 4) == PkRect(1, 2, 3, 4));
    // ⚠ 这里踩过一次：`PkRect(0,0,0,0) != PkRect()` 是**假**的 —— (l,t,w,h) 构造
    // 做 +w-1，(0,0,0,0) 的内部坐标就是 (0,0,-1,-1)，与默认矩形逐字相同。
    // 要证明"isNull 不蕴含等于默认矩形"，得挑一个**不在原点**的 null 矩形。
    PK_VERIFY(PkRect(5, 5, 0, 0).isNull());
    PK_VERIFY(PkRect(5, 5, 0, 0) != PkRect());
    // PkRectF 那侧同理：isNull 与位置无关，(5,5,0,0) 是 null 但不等于默认值
    PK_VERIFY(PkRectF(5, 5, 0, 0).isNull());
    PK_VERIFY(PkRectF(5, 5, 0, 0) != PkRectF());
}

void PkRectFCase::rectfInequalityMatchesNegation()
{
    // Qt 把 != 写成四条 `!fuzzy` 的或，而不是 `!(a==b)`。两者在 De Morgan 下
    // 恒等，所以这里断言的是**取值一致**（形态照抄是为了将来公式变了不跑偏）。
    struct { double a, b; } cs[] = {
        { 1.0, 1.0 + 1e-13 }, { 1.0, 1.0 + 1e-11 }, { 0.0, 1e-300 },
        { kInf, kInf }, { kInf, -kInf }, { kNaN, kNaN }, { kNaN, 1.0 }, { -0.0, 0.0 },
    };
    for (const auto &c : cs) {
        const PkRectF x(c.a, 0, 1, 1), y(c.b, 0, 1, 1);
        PK_COMPARE(x != y, !(x == y));
    }
    // NaN 分量：== 为假、!= 为真（fuzzy 在 NaN 上恒假）
    PK_VERIFY(!(PkRectF(kNaN, 0, 1, 1) == PkRectF(kNaN, 0, 1, 1)));
    PK_VERIFY(PkRectF(kNaN, 0, 1, 1) != PkRectF(kNaN, 0, 1, 1));
}

void PkRectFCase::rectfEqualityIsMacroProof()
{
    // PkRectF::operator== 必须写 pkQtFuzzy* 而不是 qFuzzy*：后者在
    // 「pk/test 的垫片先进 TU」那条真实共存路径上是 **#define**，函数体会被
    // 预处理器当场改写到别人的实现上去。探针 TU（rectf_macro_proof.cpp）把那条
    // 路径复现出来并指向一对破坏版实现；oracle/ 覆盖不到这一类预处理期偷换。
    const PkRectFMacroProof p = pkRectFMacroProbe();
    PK_VERIFY(p.sabotagedFuzzyWasVisible);   // 探针没空转
    PK_VERIFY(p.nearIsEqual);
    PK_VERIFY(p.farIsNotEqual);
    PK_VERIFY(p.zeroSideIsNotEqual);
    PK_VERIFY(p.allZeroIsEqual);
    PK_VERIFY(p.infVsInfIsNotEqual);
    PK_VERIFY(p.infVsNegInfIsEqual);
    PK_VERIFY(p.signedZeroIsEqual);
}

// ═══ 跨切面 ═══════════════════════════════════════════════════════════════

void PkRectFCase::rectfNoexceptSurfaceMatchesQt()
{
    // 实测（probe_rectf2.cpp）：getRect / getCoords 没有 noexcept，其余全有。
    // ⚠ 这里必须用**具名变量**：PkPointF/PkRectF 的默认构造里，
    // **PkPointF() 没有 noexcept**（qpoint.h:289 就没有），写成
    // `noexcept(PkRectF().contains(PkPointF()))` 测的是"构造临时点会不会抛"。
    PkRectF r; PkPointF p; PkRectF s;
    PK_VERIFY(noexcept(PkRectF(0, 0, 1, 1)));
    PK_VERIFY(noexcept(r.normalized()));
    PK_VERIFY(noexcept(r | s));
    PK_VERIFY(noexcept(r & s));
    PK_VERIFY(noexcept(r.contains(s)));
    PK_VERIFY(noexcept(r.contains(p)));
    PK_VERIFY(noexcept(r.intersects(s)));
    PK_VERIFY(noexcept(r.toRect()));
    PK_VERIFY(noexcept(r.toAlignedRect()));
    PK_VERIFY(noexcept(r.setRect(0, 0, 0, 0)));
    PK_VERIFY(noexcept(r.setCoords(0, 0, 0, 0)));
    PK_VERIFY(!noexcept(r.getRect(nullptr, nullptr, nullptr, nullptr)));
    PK_VERIFY(!noexcept(r.getCoords(nullptr, nullptr, nullptr, nullptr)));
}

void PkRectFCase::rectfConstexprSurfaceMatchesQt()
{
    // 带 Q_DECL_CONSTEXPR / Q_DECL_RELAXED_CONSTEXPR 的那些必须能在常量表达式里
    // 跑；out-of-line 的七个（normalized / | / & / contains ×2 / intersects /
    // toAlignedRect）**不能**，写成 constexpr 会让替代品比 Qt 多一档能力。
    constexpr PkRectF c(1, 2, 3, 4);
    constexpr bool n = c.isNull(), e = c.isEmpty(), v = c.isValid();
    constexpr double l = c.left(), rr = c.right();
    constexpr PkPointF ct = c.center();
    constexpr PkRect tr = c.toRect();
    constexpr PkRectF ad = c.adjusted(1, 1, 1, 1), tl = c.translated(1, 1);
    constexpr bool eq = (c == c);
    PK_VERIFY(!n); PK_VERIFY(!e); PK_VERIFY(v);
    PK_COMPARE(l, 1.0); PK_COMPARE(rr, 4.0);
    PK_VERIFY(ct == PkPointF(2.5, 4.0));
    PK_VERIFY(coordsAre(tr, 1, 2, 3, 5));
    PK_VERIFY(fieldsAre(ad, 2, 3, 3, 4));
    PK_VERIFY(fieldsAre(tl, 2, 3, 3, 4));
    PK_VERIFY(eq);
    // relaxed constexpr 的 mutator
    constexpr double wAfterSetLeft = [] {
        PkRectF t(1, 2, 3, 4); t.setLeft(0); return t.width();
    }();
    PK_COMPARE(wAfterSetLeft, 4.0);
}

int run_rectf_tests()
{
    PkRectFCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
