#include "cases/rect_case.h"
#include "../PkRect.h"

#include <climits>
#include <type_traits>

// PkTestBinder<PkRectCase> 由 pk_test_moc.py 生成，像 Qt moc 输出一样直接
// #include 进本 TU（理由与 test_point.cpp 相同）。
#include "pk_binder_rect_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的实测输出（探针 probe_rect.cpp，链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Core，
// QT_VERSION_STR "5.15.7"，`-DQT_NO_DEBUG`；完整输出在 Task 4 报告 §2），
// 不是"矩形右边界就是 x+w"这类直觉。对齐口径：与 Qt 的任何行为差异默认都是缺陷，
// 所以 Qt 那些反直觉的地方也一起钉住：
//   · **right() == x1 + width() - 1**、bottom() 同 —— 差一，QRectF 那边没有
//   · **QRect() 的内部坐标是 (0,0,-1,-1)**，不是 (0,0,0,0)：默认矩形的
//     isNull() 为真而 x()/y()/width()/height() 全是 0
//   · isNull / isEmpty / isValid **三者互不等价**，且
//     **QRect(0,0,0,0).isValid() == false**（QSize(0,0).isValid() 是 **true**，
//     别把 Size 那套抄过来）
//   · normalized() 的交换条件是 **x2 < x1 - 1**，不是 x2 < x1 ——
//     宽度恰为 0（x2 == x1-1）时**不交换**
//   · **operator| 不可交换**：a 为 null 返回 b，而 b 为 null 时返回的是 *this；
//     两侧都 null 时先命中第一条，于是 `QRect()|(5,5,0,0)` 与
//     `(5,5,0,0)|QRect()` 给出不同的结果
//   · 负宽高的矩形参与 &/| /contains/intersects 时内部**先按 width()<0 交换**，
//     所以 (0,0,10,10) & (0,0,-1,-1) 竟然非空
//   · center() 的加法走 **qint64 中间量**，(INT_MIN+INT_MAX)/2 不回绕
//
// ⚠ 本文件里的量全是 int，所以不涉及 PK_COMPARE 对 double 的模糊比较那条
// R-11 harness 边界（见 test_size.cpp 顶部与 README 覆盖度缺口）。
// ---------------------------------------------------------------------------

namespace {

// 断言内部四坐标。**刻意不走 getCoords()**：那个取值器自己也在被测，
// 拿它当检查手段会让"getCoords 坏了"这一整类缺陷自己把自己藏起来。
// left()/top()/right()/bottom() 是四条各自独立的一行取值器，用它们当地基。
bool coordsAre(const PkRect &r, int x1, int y1, int x2, int y2)
{
    return r.left() == x1 && r.top() == y1 && r.right() == x2 && r.bottom() == y2;
}

} // namespace

// ═══ 构造与布局 ═══════════════════════════════════════════════════════════

void PkRectCase::rectDefaultIsNullSentinel()
{
    // ⚠ 实测真 Qt：默认矩形的内部坐标是 (0,0,-1,-1)，于是 x()/y()/w()/h() 全 0
    // 而 isNull() 为真。Krita 里 `QRect r;` 之后判 isNull()/isEmpty() 的写法靠它。
    const PkRect d;
    PK_VERIFY(coordsAre(d, 0, 0, -1, -1));
    PK_COMPARE(d.x(), 0);
    PK_COMPARE(d.y(), 0);
    PK_COMPARE(d.width(), 0);
    PK_COMPARE(d.height(), 0);
    PK_VERIFY(d.isNull());
    PK_VERIFY(d.isEmpty());
    PK_VERIFY(!d.isValid());
}

void PkRectCase::rectFourConstructors()
{
    // (left, top, width, height) —— x2 = left + width - 1
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10), 0, 0, 9, 9));
    PK_VERIFY(coordsAre(PkRect(5, 5, -3, -3), 5, 5, 1, 1));
    // (topLeft, bottomRight) —— 直接摆坐标，不做 ±1
    PK_VERIFY(coordsAre(PkRect(PkPoint(1, 2), PkPoint(5, 6)), 1, 2, 5, 6));
    // (topLeft, size) —— x2 = left + w - 1
    PK_VERIFY(coordsAre(PkRect(PkPoint(1, 2), PkSize(3, 4)), 1, 2, 3, 5));
    PK_VERIFY(coordsAre(PkRect(PkPoint(1, 2), PkSize(0, 0)), 1, 2, 0, 1));
    PK_VERIFY(coordsAre(PkRect(PkPoint(1, 2), PkSize(-1, -1)), 1, 2, -1, 0));

    // ⚠ (1,2)+0x0 得到的是一个 **isNull** 的矩形（x2==x1-1 且 y2==y1-1），
    // 但它与 PkRect() 不相等 —— isNull 不蕴含"等于默认矩形"。
    PK_VERIFY(PkRect(PkPoint(1, 2), PkSize(0, 0)).isNull());
    PK_VERIFY(PkRect(PkPoint(1, 2), PkSize(0, 0)) != PkRect());

    // 溢出边界：x1 + w - 1 用的是裸的 int 加法（-fwrapv 下按二补数回绕）
    PK_VERIFY(coordsAre(PkRect(INT_MIN, INT_MIN, 1, 1), INT_MIN, INT_MIN, INT_MIN, INT_MIN));
    PK_VERIFY(coordsAre(PkRect(0, 0, INT_MAX, INT_MAX), 0, 0, INT_MAX - 1, INT_MAX - 1));
}

void PkRectCase::rectLayoutIsFourInts()
{
    // 实测 sizeof(QRect)==16：四个 int，没有额外成员/虚表。
    PK_COMPARE(sizeof(PkRect), sizeof(int) * 4);
    PK_VERIFY(std::is_trivially_copyable<PkRect>::value);
    PK_VERIFY(std::is_standard_layout<PkRect>::value);
    const PkRect r(1, 2, 3, 4);
    PK_VERIFY((std::is_same<decltype(r.left()), int>::value));
    PK_VERIFY((std::is_same<decltype(r.topLeft()), PkPoint>::value));
    PK_VERIFY((std::is_same<decltype(r.size()), PkSize>::value));
}

// ═══ 差一 ═════════════════════════════════════════════════════════════════

void PkRectCase::rectRightBottomAreOffByOne()
{
    // ⚠ **本族最著名的坑**：右/下边界是"最后一个包含在内的像素"，不是 x+w。
    // QRectF 那边没有这个差一（Task 5）。写成 x1+width() 会让整套裁剪逻辑
    // 系统性多一个像素。
    const PkRect r(0, 0, 10, 10);
    PK_COMPARE(r.left(), 0);
    PK_COMPARE(r.top(), 0);
    PK_COMPARE(r.right(), 9);
    PK_COMPARE(r.bottom(), 9);
    PK_COMPARE(r.right(), r.x() + r.width() - 1);
    PK_COMPARE(r.bottom(), r.y() + r.height() - 1);

    // x()/y() 与 left()/top() 是同一个字段的两个名字
    PK_COMPARE(r.x(), r.left());
    PK_COMPARE(r.y(), r.top());
}

void PkRectCase::rectWidthHeightAreSpanPlusOne()
{
    const PkRect r(2, 3, 10, 20);
    PK_COMPARE(r.width(), r.right() - r.left() + 1);
    PK_COMPARE(r.height(), r.bottom() - r.top() + 1);
    PK_COMPARE(r.width(), 10);
    PK_COMPARE(r.height(), 20);

    // 退化：x2 == x1-1 时宽为 0；再小一格宽为负
    PkRect z; z.setCoords(0, 0, -1, 0);
    PK_COMPARE(z.width(), 0);
    PK_COMPARE(z.height(), 1);
    PkRect n; n.setCoords(0, 0, -2, 0);
    PK_COMPARE(n.width(), -1);
}

// ═══ 三谓词 ═══════════════════════════════════════════════════════════════

void PkRectCase::rectThreePredicatesAreIndependent()
{
    // ⚠ 三条公式互不相同（qrect.h:193-200）：
    //   isNull  = x2 == x1-1 && y2 == y1-1     ← "宽和高都恰好是 0"
    //   isEmpty = x1 > x2 || y1 > y2           ← "宽或高 <= 0"
    //   isValid = x1 <= x2 && y1 <= y2         ← isEmpty 的严格取反
    // 逐条钉住实测网格（探针 §A/§B）。
    PK_VERIFY(PkRect(0, 0, 0, 0).isNull());
    PK_VERIFY(PkRect(0, 0, 0, 0).isEmpty());
    PK_VERIFY(!PkRect(0, 0, 0, 0).isValid());

    // 位置不在原点也照样 isNull —— isNull 只看宽高，不看位置
    PK_VERIFY(PkRect(5, 5, 0, 0).isNull());
    PK_VERIFY(coordsAre(PkRect(5, 5, 0, 0), 5, 5, 4, 4));

    PK_VERIFY(!PkRect(0, 0, 1, 1).isNull());
    PK_VERIFY(!PkRect(0, 0, 1, 1).isEmpty());
    PK_VERIFY(PkRect(0, 0, 1, 1).isValid());

    // 宽高都是 -1：**不是 null**（x2 == x1-2），但是 empty
    PK_VERIFY(!PkRect(0, 0, -1, -1).isNull());
    PK_VERIFY(PkRect(0, 0, -1, -1).isEmpty());
    PK_VERIFY(!PkRect(0, 0, -1, -1).isValid());
    PK_VERIFY(coordsAre(PkRect(0, 0, -1, -1), 0, 0, -2, -2));

    // 只有一边退化：不是 null，是 empty
    PK_VERIFY(!PkRect(0, 0, 10, 0).isNull());
    PK_VERIFY(PkRect(0, 0, 10, 0).isEmpty());
    PK_VERIFY(!PkRect(0, 0, 10, 0).isValid());
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 0), 0, 0, 9, -1));

    PK_VERIFY(!PkRect(5, 5, -3, -3).isNull());
    PK_VERIFY(PkRect(5, 5, -3, -3).isEmpty());
    PK_VERIFY(!PkRect(5, 5, -3, -3).isValid());

    // isEmpty 与 isValid 在整个网格上互为严格取反（两条公式互为德摩根）
    const int vs[6] = { -2, -1, 0, 1, 2, 3 };
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            for (int c = 0; c < 6; ++c)
                for (int d = 0; d < 6; ++d) {
                    PkRect r; r.setCoords(vs[a], vs[b], vs[c], vs[d]);
                    PK_VERIFY(r.isEmpty() == !r.isValid());
                }
}

void PkRectCase::rectIsValidIsNotQSizeIsValid()
{
    // ⚠ **最容易照搬错的一条**：QSize(0,0).isValid() 是 **true**（尺寸允许 0），
    // 而 QRect(0,0,0,0).isValid() 是 **false**（矩形要求 x1<=x2）。
    // 两条语义相反，抄 PkSize 那套过来会把整片退化矩形判反。
    PK_VERIFY(PkSize(0, 0).isValid());
    PK_VERIFY(!PkRect(0, 0, 0, 0).isValid());
    // 反过来，尺寸的 isNull 只认 (0,0)，矩形的 isNull 认的是"宽高都恰好 0"
    PK_VERIFY(!PkSize(5, 0).isNull());
    PK_VERIFY(PkRect(5, 5, 0, 0).isNull());
}

// ═══ 取值器 ═══════════════════════════════════════════════════════════════

void PkRectCase::rectCornersAndSize()
{
    const PkRect r(2, 3, 10, 20);       // coords (2,3,11,22)
    PK_VERIFY(r.topLeft() == PkPoint(2, 3));
    PK_VERIFY(r.topRight() == PkPoint(11, 3));
    PK_VERIFY(r.bottomLeft() == PkPoint(2, 22));
    PK_VERIFY(r.bottomRight() == PkPoint(11, 22));
    PK_VERIFY(r.size() == PkSize(10, 20));

    // 退化矩形上 size() 仍然走 width()/height()，于是拿得到 0x0
    PK_VERIFY(PkRect().size() == PkSize(0, 0));
    PK_VERIFY(PkRect(0, 0, -1, -1).size() == PkSize(-1, -1));
}

void PkRectCase::rectCenterRoundsTowardTopLeft()
{
    // center 是 (x1+x2)/2 的整数截断 —— 偶数边长时**偏向左上**。
    PK_VERIFY(PkRect(2, 3, 10, 20).center() == PkPoint(6, 12));
    PK_VERIFY(PkRect(0, 0, 10, 10).center() == PkPoint(4, 4));
    PK_VERIFY(PkRect(0, 0, 11, 11).center() == PkPoint(5, 5));
    // 负坐标：C++ 的整数除法向 0 截断，所以负半边偏向右下（= 仍然朝 0）
    PK_VERIFY(PkRect(-3, -3, 2, 2).center() == PkPoint(-2, -2));
    PK_VERIFY(PkRect(-4, -4, 3, 3).center() == PkPoint(-3, -3));
}

void PkRectCase::rectCenterUsesInt64Intermediate()
{
    // ⚠ qrect.h:262-263 把 x1 强转成 qint64 之后再加 x2，注释写着
    // "cast avoids overflow on addition"。写成 (x1+x2)/2 在下面这组输入上
    // 会先溢出再除，答案完全不同。
    PkRect r; r.setCoords(INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    PK_VERIFY(r.center() == PkPoint(0, 0));

    PkRect r2; r2.setCoords(INT_MAX - 1, INT_MAX - 1, INT_MAX, INT_MAX);
    PK_VERIFY(r2.center() == PkPoint(INT_MAX - 1, INT_MAX - 1));

    PkRect r3; r3.setCoords(INT_MIN, INT_MIN, INT_MIN + 1, INT_MIN + 1);
    PK_VERIFY(r3.center() == PkPoint(INT_MIN + 1, INT_MIN + 1));
}

// ═══ 修改器 ═══════════════════════════════════════════════════════════════

void PkRectCase::rectSetEdges()
{
    // 四个 setter 各自只动一个坐标，**不维持宽高**（与 move* 一族的区别）。
    { PkRect r(2, 3, 10, 20); r.setLeft(100);
      PK_VERIFY(coordsAre(r, 100, 3, 11, 22)); PK_COMPARE(r.width(), -88); }
    { PkRect r(2, 3, 10, 20); r.setTop(-7);
      PK_VERIFY(coordsAre(r, 2, -7, 11, 22)); }
    { PkRect r(2, 3, 10, 20); r.setRight(-100);
      PK_VERIFY(coordsAre(r, 2, 3, -100, 22)); PK_COMPARE(r.width(), -101); }
    { PkRect r(2, 3, 10, 20); r.setBottom(0);
      PK_VERIFY(coordsAre(r, 2, 3, 11, 0)); }
}

void PkRectCase::rectSetXSetY()
{
    // setX/setY 是 setLeft/setTop 的别名（写的是同一个字段），同样不保宽高。
    PkRect r(2, 3, 10, 20);
    r.setX(7);
    r.setY(8);
    PK_VERIFY(coordsAre(r, 7, 8, 11, 22));
    PK_COMPARE(r.width(), 5);
    PK_COMPARE(r.height(), 15);
}

void PkRectCase::rectSetTopLeftAndBottomRight()
{
    { PkRect r(2, 3, 10, 20); r.setTopLeft(PkPoint(-1, -2));
      PK_VERIFY(coordsAre(r, -1, -2, 11, 22)); }
    { PkRect r(2, 3, 10, 20); r.setBottomRight(PkPoint(-1, -2));
      PK_VERIFY(coordsAre(r, 2, 3, -1, -2)); PK_VERIFY(r.isEmpty()); }
}

void PkRectCase::rectSetWidthSetHeightAnchorTopLeft()
{
    // setWidth 改的是 x2（x2 = x1 + w - 1），左上角不动 —— 这正是"内部存四个
    // 边界坐标"必须与 Qt 一致的理由：存 x/y/w/h 的实现在这里语义一样，但
    // setLeft/setRight 那几条会分家。
    { PkRect r(0, 0, 10, 10); r.setWidth(0);
      PK_VERIFY(coordsAre(r, 0, 0, -1, 9)); PK_COMPARE(r.width(), 0); PK_VERIFY(r.isEmpty()); }
    { PkRect r(0, 0, 10, 10); r.setWidth(-5);
      PK_VERIFY(coordsAre(r, 0, 0, -6, 9)); PK_COMPARE(r.width(), -5); }
    { PkRect r(2, 3, 10, 20); r.setHeight(1);
      PK_VERIFY(coordsAre(r, 2, 3, 11, 3)); PK_COMPARE(r.height(), 1); }
}

void PkRectCase::rectSetSizeAnchorsTopLeft()
{
    PkRect r(2, 3, 10, 20);
    r.setSize(PkSize(0, 0));
    PK_VERIFY(coordsAre(r, 2, 3, 1, 2));
    PK_VERIFY(r.isNull());
    PK_VERIFY(r.size() == PkSize(0, 0));
}

void PkRectCase::rectSetRectAndGetRect()
{
    // setRect 吃的是 (x,y,w,h)，与 setCoords 吃 (x1,y1,x2,y2) 是两码事。
    PkRect r(0, 0, 1, 1);
    r.setRect(4, 5, 0, 0);
    PK_VERIFY(coordsAre(r, 4, 5, 3, 4));
    PK_VERIFY(r.isNull());

    int x = -1, y = -1, w = -1, h = -1;
    PkRect(2, 3, 10, 20).getRect(&x, &y, &w, &h);
    PK_COMPARE(x, 2); PK_COMPARE(y, 3); PK_COMPARE(w, 10); PK_COMPARE(h, 20);
}

void PkRectCase::rectSetCoordsAndGetCoords()
{
    PkRect r(0, 0, 1, 1);
    r.setCoords(4, 5, 3, 4);
    PK_VERIFY(coordsAre(r, 4, 5, 3, 4));
    // setCoords(4,5,3,4) 与 setRect(4,5,0,0) 落到同一处 —— 前者直接摆坐标
    PK_VERIFY(r.isNull());

    int a = -1, b = -1, c = -1, d = -1;
    PkRect(2, 3, 10, 20).getCoords(&a, &b, &c, &d);
    PK_COMPARE(a, 2); PK_COMPARE(b, 3); PK_COMPARE(c, 11); PK_COMPARE(d, 22);
}

// ═══ 平移一族 ═════════════════════════════════════════════════════════════

void PkRectCase::rectMoveToKeepsSize()
{
    { PkRect r(2, 3, 10, 20); r.moveTo(0, 0);
      PK_VERIFY(coordsAre(r, 0, 0, 9, 19)); PK_VERIFY(r.size() == PkSize(10, 20)); }
    { PkRect r(2, 3, 10, 20); r.moveTo(PkPoint(-5, -6));
      PK_VERIFY(coordsAre(r, -5, -6, 4, 13)); PK_VERIFY(r.size() == PkSize(10, 20)); }
}

void PkRectCase::rectMoveLeftMoveTopKeepSize()
{
    { PkRect r(2, 3, 10, 20); r.moveLeft(-2);
      PK_VERIFY(coordsAre(r, -2, 3, 7, 22)); PK_COMPARE(r.width(), 10); }
    { PkRect r(2, 3, 10, 20); r.moveTop(-3);
      PK_VERIFY(coordsAre(r, 2, -3, 11, 16)); PK_COMPARE(r.height(), 20); }
}

void PkRectCase::rectMoveTopLeftIsTwoMoves()
{
    PkRect r(2, 3, 10, 20);
    r.moveTopLeft(PkPoint(9, 9));
    PK_VERIFY(coordsAre(r, 9, 9, 18, 28));
    PK_VERIFY(r.size() == PkSize(10, 20));
}

void PkRectCase::rectMoveCenterUsesHalfSpan()
{
    // ⚠ moveCenter 用的是 **x2-x1**（跨距，比 width() 少 1），再整数除 2。
    // 偶数宽时新的左上角落在 p - span/2，于是中心并不精确回到 p。
    { PkRect r(0, 0, 10, 10); r.moveCenter(PkPoint(0, 0));
      PK_VERIFY(coordsAre(r, -4, -4, 5, 5)); }
    { PkRect r(0, 0, 11, 11); r.moveCenter(PkPoint(0, 0));
      PK_VERIFY(coordsAre(r, -5, -5, 5, 5)); }
    // 退化矩形：span = -1，-1/2 在 C++ 里是 0（向 0 截断）
    { PkRect r(0, 0, 0, 0); r.moveCenter(PkPoint(4, 4));
      PK_VERIFY(coordsAre(r, 4, 4, 3, 3)); PK_VERIFY(r.isNull()); }
}

void PkRectCase::rectTranslateAndTranslated()
{
    { PkRect r(0, 0, 10, 10); r.translate(3, -4);
      PK_VERIFY(coordsAre(r, 3, -4, 12, 5)); }
    { PkRect r(0, 0, 10, 10); r.translate(PkPoint(3, -4));
      PK_VERIFY(coordsAre(r, 3, -4, 12, 5)); }
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10).translated(3, -4), 3, -4, 12, 5));
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10).translated(PkPoint(3, -4)), 3, -4, 12, 5));
    // translated 不改原件
    const PkRect r0(0, 0, 10, 10);
    (void)r0.translated(3, -4);
    PK_VERIFY(coordsAre(r0, 0, 0, 9, 9));
}

// ═══ adjust ═══════════════════════════════════════════════════════════════

void PkRectCase::rectAdjustAndAdjusted()
{
    { PkRect r(0, 0, 10, 10); r.adjust(1, 2, -3, -4);
      PK_VERIFY(coordsAre(r, 1, 2, 6, 5)); }
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10).adjusted(1, 2, -3, -4), 1, 2, 6, 5));
    // adjusted 不改原件
    const PkRect r0(0, 0, 10, 10);
    (void)r0.adjusted(1, 2, -3, -4);
    PK_VERIFY(coordsAre(r0, 0, 0, 9, 9));
}

void PkRectCase::rectAdjustedCanGoDegenerate()
{
    // 收缩到宽 0（恰好变成 isNull），再收一格变成负宽
    PK_VERIFY(coordsAre(PkRect(0, 0, 1, 1).adjusted(0, 0, -1, -1), 0, 0, -1, -1));
    PK_VERIFY(PkRect(0, 0, 1, 1).adjusted(0, 0, -1, -1).isNull());
    PK_VERIFY(coordsAre(PkRect(0, 0, 1, 1).adjusted(0, 0, -2, -2), 0, 0, -2, -2));
    PK_VERIFY(!PkRect(0, 0, 1, 1).adjusted(0, 0, -2, -2).isNull());
    PK_VERIFY(PkRect(0, 0, 1, 1).adjusted(0, 0, -2, -2).isEmpty());
}

// ═══ normalized ═══════════════════════════════════════════════════════════

void PkRectCase::rectNormalizedSwapBoundaryIsX1MinusOne()
{
    // ⚠ **交换条件是 x2 < x1 - 1，不是 x2 < x1**。也就是说宽度恰为 0
    //（x2 == x1-1）时**不交换**，宽度 -1 起才交换；而且交换之后**不做 ±1 修正**，
    // 于是宽度从 -1 直接跳到 3（探针 §D 逐格实测）。
    { PkRect r; r.setCoords(0, 0, 0, 0);
      PK_VERIFY(coordsAre(r.normalized(), 0, 0, 0, 0)); PK_COMPARE(r.normalized().width(), 1); }
    { PkRect r; r.setCoords(0, 0, -1, 0);       // width 0 —— 不交换
      PK_VERIFY(coordsAre(r.normalized(), 0, 0, -1, 0)); PK_COMPARE(r.normalized().width(), 0); }
    { PkRect r; r.setCoords(0, 0, -2, 0);       // width -1 —— 交换
      PK_VERIFY(coordsAre(r.normalized(), -2, 0, 0, 0)); PK_COMPARE(r.normalized().width(), 3); }
    { PkRect r; r.setCoords(0, 0, -3, 0);
      PK_VERIFY(coordsAre(r.normalized(), -3, 0, 0, 0)); PK_COMPARE(r.normalized().width(), 4); }
    { PkRect r; r.setCoords(0, 0, -4, 0);
      PK_VERIFY(coordsAre(r.normalized(), -4, 0, 0, 0)); PK_COMPARE(r.normalized().width(), 5); }
    // 两个轴各自独立判断
    { PkRect r; r.setCoords(0, 0, 0, -3);
      PK_VERIFY(coordsAre(r.normalized(), 0, -3, 0, 0)); }

    PK_VERIFY(coordsAre(PkRect(0, 0, -1, -1).normalized(), -2, -2, 0, 0));
    PK_VERIFY(coordsAre(PkRect(5, 5, -3, -3).normalized(), 1, 1, 5, 5));
}

void PkRectCase::rectNormalizedLeavesNullAlone()
{
    // null 矩形的两条跨距都恰好是 -1，正好落在"不交换"那一侧
    PK_VERIFY(coordsAre(PkRect().normalized(), 0, 0, -1, -1));
    PK_VERIFY(PkRect().normalized().isNull());
    PK_VERIFY(coordsAre(PkRect(5, 5, 0, 0).normalized(), 5, 5, 4, 4));
    PK_VERIFY(PkRect(5, 5, 0, 0).normalized().isNull());
}

// ═══ 集合运算 ═════════════════════════════════════════════════════════════

void PkRectCase::rectUnitedWithNullIsAsymmetric()
{
    // ⚠ **operator| 不可交换。** 分支顺序是「a 为 null 返回 b」在前，
    // 「b 为 null 返回 a」在后，于是两侧都 null 时永远返回 b。
    PK_VERIFY(coordsAre(PkRect() | PkRect(5, 5, 0, 0), 5, 5, 4, 4));
    PK_VERIFY(coordsAre(PkRect(5, 5, 0, 0) | PkRect(), 0, 0, -1, -1));
    PK_VERIFY((PkRect() | PkRect(5, 5, 0, 0)) != (PkRect(5, 5, 0, 0) | PkRect()));

    // 非 null 的一侧原样返回（**不 normalize**）
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) | PkRect(5, 5, 0, 0), 0, 0, 9, 9));
    PK_VERIFY(coordsAre(PkRect(5, 5, -3, -3) | PkRect(), 5, 5, 1, 1));
    // 判空用的是 isNull() 而**不是** isEmpty()：(0,0,-1,-1) 是 empty 但非 null，
    // 所以它真的参与并集运算而不是被当成空集跳过。
    PK_VERIFY(coordsAre(PkRect(0, 0, 1, 1) | PkRect(0, 0, -1, -1), -2, -2, 0, 0));
}

void PkRectCase::rectUnitedNormalizesNegativeDims()
{
    // 负宽高的一侧在**内部**按 width()<0 交换左右，再取 min/max
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) | PkRect(0, 0, -1, -1), -2, -2, 9, 9));
    PK_VERIFY(coordsAre(PkRect(0, 0, -1, -1) | PkRect(0, 0, -1, -1), -2, -2, 0, 0));
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) | PkRect(20, 20, 5, 5), 0, 0, 24, 24));
    PK_VERIFY(coordsAre(PkRect(5, 5, -3, -3) | PkRect(20, 20, 5, 5), 1, 1, 24, 24));
    // united 就是 operator|
    PK_VERIFY(PkRect(0, 0, 10, 10).united(PkRect(20, 20, 5, 5))
              == (PkRect(0, 0, 10, 10) | PkRect(20, 20, 5, 5)));
}

void PkRectCase::rectIntersectedOnDegenerate()
{
    // 任一侧 isNull 直接给默认矩形（**不是**"给空交集"那么简单：结果是 (0,0,-1,-1)）
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(), 0, 0, -1, -1));
    PK_VERIFY(coordsAre(PkRect() & PkRect(0, 0, 10, 10), 0, 0, -1, -1));
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(5, 5, 0, 0), 0, 0, -1, -1));

    // ⚠ 负宽高的矩形**竟然相交**：内部先交换，(0,0,-1,-1) 变成 [-2,0]
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(0, 0, -1, -1), 0, 0, 0, 0));
    PK_VERIFY(coordsAre(PkRect(0, 0, -1, -1) & PkRect(0, 0, -1, -1), -2, -2, 0, 0));
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(5, 5, -3, -3), 1, 1, 5, 5));

    // 完全不重叠 → 默认矩形
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(20, 20, 5, 5), 0, 0, -1, -1));
    PK_VERIFY(PkRect(0, 0, 10, 10).intersected(PkRect(5, 5, 20, 20))
              == (PkRect(0, 0, 10, 10) & PkRect(5, 5, 20, 20)));
    PK_VERIFY(coordsAre(PkRect(0, 0, 10, 10) & PkRect(5, 5, 20, 20), 5, 5, 9, 9));
}

void PkRectCase::rectIntersectsOnDegenerate()
{
    PK_VERIFY(!PkRect().intersects(PkRect()));
    PK_VERIFY(!PkRect(0, 0, 10, 10).intersects(PkRect(5, 5, 0, 0)));
    PK_VERIFY(!PkRect(0, 0, 10, 10).intersects(PkRect(20, 20, 5, 5)));
    // ⚠ 负宽高的矩形相交为真（与 & 的非空结果一致）
    PK_VERIFY(PkRect(0, 0, 10, 10).intersects(PkRect(0, 0, -1, -1)));
    PK_VERIFY(PkRect(0, 0, -1, -1).intersects(PkRect(0, 0, -1, -1)));
    PK_VERIFY(PkRect(0, 0, 10, 10).intersects(PkRect(5, 5, -3, -3)));
    PK_VERIFY(!PkRect(0, 0, -1, -1).intersects(PkRect(5, 5, -3, -3)));
    PK_VERIFY(PkRect(0, 0, 1, 1).intersects(PkRect(0, 0, 10, 10)));
}

void PkRectCase::rectOperatorAssignMatchesNamed()
{
    { PkRect a(0, 0, 10, 10); a |= PkRect(20, 20, 5, 5);
      PK_VERIFY(coordsAre(a, 0, 0, 24, 24)); }
    { PkRect b(0, 0, 10, 10); b &= PkRect(5, 5, 20, 20);
      PK_VERIFY(coordsAre(b, 5, 5, 9, 9)); }
    // |= 与 | 走同一条路（复合赋值就是 `*this = *this | r`），
    // 所以它同样不可交换、同样在 null 上偏心。
    { PkRect c(5, 5, 0, 0); c |= PkRect();
      PK_VERIFY(coordsAre(c, 0, 0, -1, -1)); }
}

// ═══ contains ═════════════════════════════════════════════════════════════

void PkRectCase::rectContainsPointBoundary()
{
    const PkRect r(0, 0, 10, 10);       // coords (0,0,9,9)
    PK_VERIFY(r.contains(PkPoint(0, 0)));
    PK_VERIFY(r.contains(PkPoint(9, 9)));
    PK_VERIFY(!r.contains(PkPoint(10, 10)));    // ⚠ 右下角是 9 不是 10
    PK_VERIFY(!r.contains(PkPoint(-1, 0)));
    PK_VERIFY(!r.contains(PkPoint(0, 10)));
    PK_VERIFY(r.contains(PkPoint(0, 9)));
    PK_VERIFY(r.contains(PkPoint(9, 0)));

    // proper：边界上的点不算
    PK_VERIFY(!r.contains(PkPoint(0, 0), true));
    PK_VERIFY(!r.contains(PkPoint(9, 9), true));
    PK_VERIFY(r.contains(PkPoint(5, 5), true));

    // (int,int) 与 (int,int,bool) 两个重载与 QPoint 版一致
    PK_VERIFY(r.contains(9, 9));
    PK_VERIFY(!r.contains(10, 10));
    PK_VERIFY(r.contains(5, 5, true));
    PK_VERIFY(!r.contains(0, 0, true));
}

void PkRectCase::rectContainsPointOnNegativeDims()
{
    // contains(point) 内部按 x2 < x1-1 交换（与 normalized 同一条边界）
    PkRect n; n.setCoords(10, 10, 0, 0);
    PK_VERIFY(n.contains(PkPoint(5, 5)));
    PK_VERIFY(n.contains(PkPoint(10, 10)));
    PK_VERIFY(n.contains(PkPoint(5, 5), true));

    // x2 == x1-1（宽 0）落在"不交换"一侧，于是 l=0、r=-1，任何点都不在里面
    PkRect m; m.setCoords(0, 0, -1, 0);
    PK_VERIFY(!m.contains(PkPoint(0, 0)));
    PK_VERIFY(!m.contains(PkPoint(-1, 0)));
}

void PkRectCase::rectContainsRect()
{
    // 任一侧 isNull 直接 false（**在做区间比较之前**就返回了）
    PK_VERIFY(!PkRect(0, 0, 10, 10).contains(PkRect()));
    PK_VERIFY(!PkRect(0, 0, 10, 10).contains(PkRect(5, 5, 0, 0)));
    PK_VERIFY(!PkRect().contains(PkRect(0, 0, 10, 10)));

    // 自包含为真、proper 为假
    PK_VERIFY(PkRect(0, 0, 10, 10).contains(PkRect(0, 0, 10, 10)));
    PK_VERIFY(!PkRect(0, 0, 10, 10).contains(PkRect(0, 0, 10, 10), true));

    PK_VERIFY(PkRect(0, 0, 10, 10).contains(PkRect(0, 0, 1, 1)));
    PK_VERIFY(!PkRect(0, 0, 1, 1).contains(PkRect(0, 0, 10, 10)));
    PK_VERIFY(!PkRect(0, 0, 10, 10).contains(PkRect(20, 20, 5, 5)));

    // ⚠ 负宽高：contains 里的交换判据是 `x2 - x1 + 1 < 0`（= width()<0），
    // 与 normalized/contains(point) 的 `x2 < x1 - 1` 是**同一条线**的两种写法。
    PK_VERIFY(PkRect(0, 0, 10, 10).contains(PkRect(5, 5, -3, -3)));
    PK_VERIFY(PkRect(0, 0, 10, 10).contains(PkRect(5, 5, -3, -3), true));
    PK_VERIFY(PkRect(0, 0, -1, -1).contains(PkRect(0, 0, 1, 1)));
    PK_VERIFY(!PkRect(0, 0, -1, -1).contains(PkRect(0, 0, 10, 10)));
    PK_VERIFY(PkRect(0, 0, -1, -1).contains(PkRect(0, 0, -1, -1)));
}

// ═══ 运算符与溢出 ═════════════════════════════════════════════════════════

void PkRectCase::rectEquality()
{
    // == 比的是四个**内部坐标**，不是 x/y/w/h（两者在退化矩形上不等价）
    PkRect a; a.setCoords(0, 0, -1, -1);
    PK_VERIFY(PkRect() == a);
    // 两个都 isNull 但位置不同 —— 不相等
    PK_VERIFY(!(PkRect(0, 0, 0, 0) == PkRect(5, 5, 0, 0)));
    PK_VERIFY(PkRect(0, 0, 0, 0) != PkRect(5, 5, 0, 0));
    PK_VERIFY(PkRect(0, 0, 10, 10) != PkRect(0, 0, 10, 11));
    PK_VERIFY(!(PkRect(0, 0, 10, 10) != PkRect(0, 0, 10, 10)));
}

void PkRectCase::rectSpanOverflowWraps()
{
    // width() 是裸的 x2-x1+1，在极端坐标上溢出回绕（-fwrapv 钉死成二补数）。
    // 实测真 Qt 给同样的值 —— 这不是"我们的 bug"，是照抄了 Qt 的 UB。
    PkRect r; r.setCoords(0, 0, INT_MAX, INT_MAX);
    PK_COMPARE(r.width(), INT_MIN);
    PK_COMPARE(r.height(), INT_MIN);

    PkRect s; s.setCoords(INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    PK_COMPARE(s.width(), 0);
    PK_COMPARE(s.height(), 0);
}

// ═══ 跨切面 ═══════════════════════════════════════════════════════════════

void PkRectCase::rectNoexceptSurfaceMatchesQt()
{
    PkRect r(0, 0, 10, 10);
    const PkRect c(0, 0, 10, 10);
    int a = 0, b = 0, x = 0, y = 0;

    PK_VERIFY(noexcept(PkRect()));
    PK_VERIFY(noexcept(PkRect(0, 0, 1, 1)));
    PK_VERIFY(noexcept(c.isNull()));
    PK_VERIFY(noexcept(c.left()));
    PK_VERIFY(noexcept(c.normalized()));
    PK_VERIFY(noexcept(c.center()));
    PK_VERIFY(noexcept(r.setLeft(0)));
    PK_VERIFY(noexcept(r.setRect(0, 0, 0, 0)));
    PK_VERIFY(noexcept(r.setCoords(0, 0, 0, 0)));
    PK_VERIFY(noexcept(c | c));
    PK_VERIFY(noexcept(c & c));
    PK_VERIFY(noexcept(c.contains(c)));
    PK_VERIFY(noexcept(c.contains(0, 0)));
    PK_VERIFY(noexcept(c.contains(0, 0, true)));
    PK_VERIFY(noexcept(c.intersects(c)));
    PK_VERIFY(noexcept(c.united(c)));
    PK_VERIFY(noexcept(c.intersected(c)));
    PK_VERIFY(noexcept(c == c));
    PK_VERIFY(noexcept(r |= c));
    PK_VERIFY(noexcept(r.setSize(PkSize(1, 1))));

    // ⚠ **contains(PkPoint) 这条要连 PkPoint 的构造一起看。**
    // qpoint.h:56 的 `QPoint(int,int)` **没有 noexcept**（qsize.h 的 QSize 有），
    // 于是 `contains(QPoint(0,0))` 这个**完整表达式**在真 Qt 上是 noexcept==false，
    // 而换成具名实参就是 true —— 差别全在实参构造那一跳，不在 contains 自己。
    // 实测真 Qt 5.15.7（探针 probe_noexcept.cpp）：
    //   QPoint(0,0)=0  c.contains(QPoint(0,0))=0  c.contains(p)=1
    //   r.moveCenter(QPoint(0,0))=0  r.setSize(QSize(1,1))=1
    // ⚠ 别按"contains 标了 noexcept，所以整个表达式也是"去推 —— `noexcept(expr)`
    // 看的是**整个表达式**，实参构造那一跳算在里面。
    const PkPoint p0(0, 0);
    PK_VERIFY(!noexcept(PkPoint(0, 0)));
    PK_VERIFY(!noexcept(c.contains(PkPoint(0, 0))));
    PK_VERIFY(noexcept(c.contains(p0)));
    PK_VERIFY(!noexcept(r.moveCenter(PkPoint(0, 0))));
    PK_VERIFY(noexcept(r.moveCenter(p0)));

    // ⚠ **getRect / getCoords 在 Qt 里恰好没有标 noexcept**（qrect.h:115、:118），
    // 而同一对的 setRect / setCoords 标了。照抄这个不对称 —— noexcept 是可观察的。
    PK_VERIFY(!noexcept(c.getRect(&a, &b, &x, &y)));
    PK_VERIFY(!noexcept(c.getCoords(&a, &b, &x, &y)));
}

void PkRectCase::rectConstexprSurfaceMatchesQt()
{
    // qrect.h 里绝大多数是 Q_DECL_CONSTEXPR，但 normalized/operator|/operator&/
    // contains/intersects 是 out-of-line 的**非** constexpr 函数（照抄）。
    //
    // 写法上刻意把两件事拆开：用 `constexpr` **变量**接住取值 —— 初始化式必须在
    // 编译期算得出来（constexpr 面掉了就编不过），而**取值本身用运行期断言比**
    //（写成 static_assert 的话，实现是空壳时会变成编译错误而不是一条红测试，
    // "先跑测试确认失败"那一步就拿不到可读的红态输出）。
    constexpr PkRect r(0, 0, 10, 10);
    constexpr int cLeft = r.left();
    constexpr int cRight = r.right();
    constexpr int cWidth = r.width();
    constexpr bool cNull = r.isNull();
    constexpr bool cValid = r.isValid();
    constexpr int cCenterX = r.center().x();
    constexpr int cAdjLeft = r.adjusted(1, 1, -1, -1).left();
    constexpr int cTrLeft = r.translated(1, 1).left();
    constexpr bool cDefaultNull = PkRect().isNull();
    // relaxed constexpr（C++14 起 constexpr 函数体里可以赋值）：Qt 用的是
    // Q_DECL_RELAXED_CONSTEXPR，这两条能编说明那批 mutator 也保住了 constexpr。
    constexpr int cSetWidth = [] { PkRect t(0, 0, 10, 10); t.setWidth(3); return t.right(); }();
    constexpr int cMoveTo = [] { PkRect t(0, 0, 10, 10); t.moveTo(5, 5); return t.left(); }();

    PK_COMPARE(cLeft, 0);
    PK_COMPARE(cRight, 9);
    PK_COMPARE(cWidth, 10);
    PK_VERIFY(!cNull);
    PK_VERIFY(cValid);
    PK_COMPARE(cCenterX, 4);
    PK_COMPARE(cAdjLeft, 1);
    PK_COMPARE(cTrLeft, 1);
    PK_VERIFY(cDefaultNull);
    PK_COMPARE(cSetWidth, 2);
    PK_COMPARE(cMoveTo, 5);
}

int run_rect_tests()
{
    PkRectCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
