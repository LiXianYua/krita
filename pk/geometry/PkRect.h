#ifndef PK_GEOMETRY_PKRECT_H
#define PK_GEOMETRY_PKRECT_H

#include "PkGlobal.h"
#include "PkMargins.h"
#include "PkPoint.h"
#include "PkSize.h"

// ---------------------------------------------------------------------------
// PkRect —— QRect 的零 Qt 替代。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtCore/qrect.h（QT_VERSION_STR "5.15.7"），
// 来源行号标在各项上方；五个 out-of-line 成员（normalized / operator| /
// operator& / contains ×2 / intersects）在 PkRect.cpp，那份的来源与核对方式见
// 该文件顶部。对齐口径：与 Qt 的任何行为差异默认都是缺陷，所以 Qt 那些反直觉
// 的地方也照抄，并在 tests/test_rect.cpp 与 oracle/ 里逐条钉住：
//
//   · **内部存的是四个边界坐标 x1/y1/x2/y2，不是 x/y/w/h。** 这不是实现细节：
//     setLeft/setRight/setWidth/moveTo/adjust 全都直接摆坐标，换一套内部表示
//     之后这一族的语义会整体走样（`setLeft` 在坐标表示下**不保宽度**，
//     在 x/y/w/h 表示下会保）。
//   · **right() == x1 + width() - 1、bottom() 同** —— 差一。QRectF 没有这条
//     （Task 5），两者混用是 Krita 里最常见的一格误差来源。
//   · **默认构造是 (0,0,-1,-1)**（宽高都是 0 且 isNull），不是 (0,0,0,0)。
//   · isNull / isEmpty / isValid **三条公式互不相同**，且
//     **PkRect(0,0,0,0).isValid() == false** —— 与 **PkSize(0,0).isValid()
//     == true** 相反。抄 PkSize 那套过来会把整片退化矩形判反。
//   · normalized() 的交换条件是 **x2 < x1 - 1**（不是 x2 < x1），交换后**不做
//     ±1 修正**，于是宽度从 -1 直接跳到 3。
//   · **operator| 不可交换**：分支顺序是「a 为 null 返回 b」在前，于是两侧都
//     null 时永远返回 b。
//   · 负宽高的矩形参与 | & contains intersects 时内部**先按 width()<0 交换**，
//     所以 (0,0,10,10) & (0,0,-1,-1) 竟然非空。
//   · center() 的加法走 **qint64 中间量**（qrect.h 的注释就写着
//     "cast avoids overflow on addition"），而 moveCenter 用的是**不带 +1 的
//     跨距 x2-x1**，两者不是同一个量。
//   · **getRect / getCoords 恰好没有 noexcept**，而同一对的 setRect / setCoords
//     有。noexcept 是可观察的，这个不对称照抄。
//
// Qt 宏到 C++17 的映射（无行为差异，README 偏离清单里登记）：
//   Q_DECL_CONSTEXPR / Q_DECL_RELAXED_CONSTEXPR → constexpr
//   Q_CORE_EXPORT / Q_DECLARE_TYPEINFO / Q_REQUIRED_RESULT → 去掉
//   qint64 → long long（与 PkSize.cpp 同一处理：R-03 的用量表没点名
//   qint8..quint64 那批 typedef，它们归 R-02）
//
// 明确不实现（Rect 族实测调用点 0，判据①「一项不多」；完整归属表在 README）：
//   · setTopRight / setBottomLeft
//   · moveRight / moveBottom / moveTopRight / moveBottomRight / moveBottomLeft
//   · transposed —— 三形态命中的 5 处接收者全不是 Rect 族
//     （1 处 QTransform::transposed、4 处 Eigen 矩阵）
//   · **unite / intersect**（Qt5 已废弃的两个别名）—— 计划把它们列进"用量 >0
//     必须实现"，实测证伪：`intersect` 唯一命中是 `QSet<int>::intersect`，
//     `unite` 两处分别是 `QSet<int>::unite` 与
//     `KisFilterWeightsApplicator::LinePos::unite`，**Rect 族真实调用点 0**
//   · toCGRect / fromCGRect（Darwin 专有）
//
// R-21 T1 新增（互操作，见 PkMargins.h 头部注释）：marginsAdded(const
// PkMargins&) / marginsRemoved(const PkMargins&) / operator+=(const
// PkMargins&) / operator-=(const PkMargins&)，加自由函数
// operator+/-(const PkRect&, const PkMargins&)。这四个互操作成员本身在
// QRect 侧同样是 0 用量（挡它们的 QMargins 全仓保留范围内 0 用量）——
// 与 PkRect 的构造函数/运算符按 Qt 头文件全集实现是同一类处置，任务定义
// 已批准这条例外，不需要在这里重新论证。
//   · qHash / QDataStream 的 <<>> / QDebug 的 <<（归 R-02 / R-12 / R-08）
// ---------------------------------------------------------------------------

class PkRect
{
public:
    // qrect.h:60 —— ⚠ **(0,0,-1,-1)**：宽高都是 0，且 isNull() 为真。
    // Qt 把这一个的函数体写在类体里，这里挪到下面与其余三个构造并排 ——
    // 取值一字不差，改的只是位置：oracle/run_oracle.sh 的规则三闸门要按
    // **类体里的纯声明**解析出重载清单，类体里混着初始化列表会让那个解析
    // 多一条只为它存在的特例。
    constexpr PkRect() noexcept;
    constexpr PkRect(const PkPoint &topleft, const PkPoint &bottomright) noexcept;
    constexpr PkRect(const PkPoint &topleft, const PkSize &size) noexcept;
    constexpr PkRect(int left, int top, int width, int height) noexcept;

    constexpr inline bool isNull() const noexcept;
    constexpr inline bool isEmpty() const noexcept;
    constexpr inline bool isValid() const noexcept;

    constexpr inline int left() const noexcept;
    constexpr inline int top() const noexcept;
    constexpr inline int right() const noexcept;
    constexpr inline int bottom() const noexcept;
    // qrect.h:73 —— out-of-line，**不是** constexpr。定义在 PkRect.cpp。
    PkRect normalized() const noexcept;

    constexpr inline int x() const noexcept;
    constexpr inline int y() const noexcept;
    constexpr inline void setLeft(int pos) noexcept;
    constexpr inline void setTop(int pos) noexcept;
    constexpr inline void setRight(int pos) noexcept;
    constexpr inline void setBottom(int pos) noexcept;
    constexpr inline void setX(int x) noexcept;
    constexpr inline void setY(int y) noexcept;

    constexpr inline void setTopLeft(const PkPoint &p) noexcept;
    constexpr inline void setBottomRight(const PkPoint &p) noexcept;

    constexpr inline PkPoint topLeft() const noexcept;
    constexpr inline PkPoint bottomRight() const noexcept;
    constexpr inline PkPoint topRight() const noexcept;
    constexpr inline PkPoint bottomLeft() const noexcept;
    constexpr inline PkPoint center() const noexcept;

    constexpr inline void moveLeft(int pos) noexcept;
    constexpr inline void moveTop(int pos) noexcept;
    constexpr inline void moveTopLeft(const PkPoint &p) noexcept;
    constexpr inline void moveCenter(const PkPoint &p) noexcept;

    constexpr inline void translate(int dx, int dy) noexcept;
    constexpr inline void translate(const PkPoint &p) noexcept;
    constexpr inline PkRect translated(int dx, int dy) const noexcept;
    constexpr inline PkRect translated(const PkPoint &p) const noexcept;

    constexpr inline void moveTo(int x, int t) noexcept;
    constexpr inline void moveTo(const PkPoint &p) noexcept;

    constexpr inline void setRect(int x, int y, int w, int h) noexcept;
    // qrect.h:115 —— ⚠ **没有 noexcept**（同一对的 setRect 有）。照抄。
    constexpr inline void getRect(int *x, int *y, int *w, int *h) const;

    constexpr inline void setCoords(int x1, int y1, int x2, int y2) noexcept;
    // qrect.h:118 —— ⚠ 同样**没有 noexcept**。
    constexpr inline void getCoords(int *x1, int *y1, int *x2, int *y2) const;

    constexpr inline void adjust(int x1, int y1, int x2, int y2) noexcept;
    constexpr inline PkRect adjusted(int x1, int y1, int x2, int y2) const noexcept;

    constexpr inline PkSize size() const noexcept;
    constexpr inline int width() const noexcept;
    constexpr inline int height() const noexcept;
    constexpr inline void setWidth(int w) noexcept;
    constexpr inline void setHeight(int h) noexcept;
    constexpr inline void setSize(const PkSize &s) noexcept;

    // qrect.h:130-133 —— 前两个 out-of-line（定义在 PkRect.cpp），
    // 后两个是 inline 的复合赋值，转发到前两个。
    PkRect operator|(const PkRect &r) const noexcept;
    PkRect operator&(const PkRect &r) const noexcept;
    inline PkRect &operator|=(const PkRect &r) noexcept;
    inline PkRect &operator&=(const PkRect &r) noexcept;

    // R-21 T1 —— qrect.h:143-146。四个互操作成员，签名吃 PkMargins（整数版）。
    // 真 Qt 5.15.7 探针确认 QRect::marginsAdded/marginsRemoved 吃的是
    // QMargins 不是 QMarginsF（QRectF 那边才是 QMarginsF，见 PkRectF 对应
    // 位置）。前两个 constexpr、后两个转发。
    constexpr inline PkRect marginsAdded(const PkMargins &margins) const noexcept;
    constexpr inline PkRect marginsRemoved(const PkMargins &margins) const noexcept;
    constexpr inline PkRect &operator+=(const PkMargins &margins) noexcept;
    constexpr inline PkRect &operator-=(const PkMargins &margins) noexcept;

    bool contains(const PkRect &r, bool proper = false) const noexcept;
    bool contains(const PkPoint &p, bool proper = false) const noexcept;
    inline bool contains(int x, int y) const noexcept;
    inline bool contains(int x, int y, bool proper) const noexcept;
    inline PkRect united(const PkRect &other) const noexcept;
    inline PkRect intersected(const PkRect &other) const noexcept;
    bool intersects(const PkRect &r) const noexcept;

    friend constexpr inline bool operator==(const PkRect &, const PkRect &) noexcept;
    friend constexpr inline bool operator!=(const PkRect &, const PkRect &) noexcept;

private:
    // qrect.h:161-164。⚠ **四个边界坐标**，顺序也照抄（== 的比较顺序、
    // 布局兼容性都靠它）。
    int x1;
    int y1;
    int x2;
    int y2;
};

// ── inline 成员（qrect.h:184-462）────────────────────────────────────────

constexpr inline PkRect::PkRect() noexcept : x1(0), y1(0), x2(-1), y2(-1) {}

// qrect.h:184-191 —— 三个带参构造：只有 (l,t,w,h) 与 (topLeft,size) 做
// **+w-1**，(topLeft,bottomRight) 直接摆坐标。裸的 int 加法，溢出按 -fwrapv
// 回绕（Qt 自己也是裸加法，README 覆盖度缺口有登记）。
constexpr inline PkRect::PkRect(int aleft, int atop, int awidth, int aheight) noexcept
    : x1(aleft), y1(atop), x2(aleft + awidth - 1), y2(atop + aheight - 1) {}

constexpr inline PkRect::PkRect(const PkPoint &atopLeft, const PkPoint &abottomRight) noexcept
    : x1(atopLeft.x()), y1(atopLeft.y()), x2(abottomRight.x()), y2(abottomRight.y()) {}

constexpr inline PkRect::PkRect(const PkPoint &atopLeft, const PkSize &asize) noexcept
    : x1(atopLeft.x()), y1(atopLeft.y()),
      x2(atopLeft.x() + asize.width() - 1), y2(atopLeft.y() + asize.height() - 1) {}

// qrect.h:193-200 —— **三条公式逐字照抄，一个符号都不能动**：
//   isNull  认的是"两条跨距都恰好是 -1"（= 宽高都恰好 0），**与位置无关**；
//   isEmpty 是"宽或高 <= 0"；isValid 是它的严格取反。
// 于是 (0,0,0,0) 同时是 null、empty、**非 valid**；(0,0,-1,-1) 非 null 但 empty；
// (0,0,10,0) 非 null 但 empty。三者互不蕴含。
constexpr inline bool PkRect::isNull() const noexcept
{ return x2 == x1 - 1 && y2 == y1 - 1; }

constexpr inline bool PkRect::isEmpty() const noexcept
{ return x1 > x2 || y1 > y2; }

constexpr inline bool PkRect::isValid() const noexcept
{ return x1 <= x2 && y1 <= y2; }

constexpr inline int PkRect::left() const noexcept
{ return x1; }

constexpr inline int PkRect::top() const noexcept
{ return y1; }

// qrect.h:208-212 —— ⚠ **差一**：right()/bottom() 返回的是内部坐标 x2/y2，
// 而 x2 == x1 + width() - 1。写成 x1 + width() 会让整套裁剪多一个像素。
constexpr inline int PkRect::right() const noexcept
{ return x2; }

constexpr inline int PkRect::bottom() const noexcept
{ return y2; }

constexpr inline int PkRect::x() const noexcept
{ return x1; }

constexpr inline int PkRect::y() const noexcept
{ return y1; }

// qrect.h:220-248 —— set* 一族**只动一个坐标，不维持宽高**（这是它们与
// move* 一族的全部区别）。setX/setY 就是 setLeft/setTop 的别名。
constexpr inline void PkRect::setLeft(int pos) noexcept
{ x1 = pos; }

constexpr inline void PkRect::setTop(int pos) noexcept
{ y1 = pos; }

constexpr inline void PkRect::setRight(int pos) noexcept
{ x2 = pos; }

constexpr inline void PkRect::setBottom(int pos) noexcept
{ y2 = pos; }

constexpr inline void PkRect::setTopLeft(const PkPoint &p) noexcept
{ x1 = p.x(); y1 = p.y(); }

constexpr inline void PkRect::setBottomRight(const PkPoint &p) noexcept
{ x2 = p.x(); y2 = p.y(); }

constexpr inline void PkRect::setX(int ax) noexcept
{ x1 = ax; }

constexpr inline void PkRect::setY(int ay) noexcept
{ y1 = ay; }

constexpr inline PkPoint PkRect::topLeft() const noexcept
{ return PkPoint(x1, y1); }

constexpr inline PkPoint PkRect::bottomRight() const noexcept
{ return PkPoint(x2, y2); }

constexpr inline PkPoint PkRect::topRight() const noexcept
{ return PkPoint(x2, y1); }

constexpr inline PkPoint PkRect::bottomLeft() const noexcept
{ return PkPoint(x1, y2); }

// qrect.h:262-263 —— ⚠ **加法在 qint64 上做**（Qt 的原注释：
// "cast avoids overflow on addition"）。写成 (x1+x2)/2 会在
// setCoords(INT_MIN,INT_MIN,INT_MAX,INT_MAX) 这类输入上先溢出再除，答案完全不同
//（实测真 Qt 给 (0,0)）。除法是 C++ 的向 0 截断，所以偶数边长时中心偏向原点侧。
constexpr inline PkPoint PkRect::center() const noexcept
{ return PkPoint(int(((long long)x1 + x2) / 2), int(((long long)y1 + y2) / 2)); }

constexpr inline int PkRect::width() const noexcept
{ return  x2 - x1 + 1; }

constexpr inline int PkRect::height() const noexcept
{ return  y2 - y1 + 1; }

// qrect.h:271-272 —— 按公开 API 组装 PkSize，与 Qt 一致（所以 PkSize 不必给
// 本类开 friend）。退化矩形上照样返回 width()/height() 算出来的值（可能是负数）。
constexpr inline PkSize PkRect::size() const noexcept
{ return PkSize(width(), height()); }

// qrect.h:274-294 —— 平移一族：四个坐标同时挪，宽高天然不变。
constexpr inline void PkRect::translate(int dx, int dy) noexcept
{
    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
}

constexpr inline void PkRect::translate(const PkPoint &p) noexcept
{
    x1 += p.x();
    y1 += p.y();
    x2 += p.x();
    y2 += p.y();
}

constexpr inline PkRect PkRect::translated(int dx, int dy) const noexcept
{ return PkRect(PkPoint(x1 + dx, y1 + dy), PkPoint(x2 + dx, y2 + dy)); }

constexpr inline PkRect PkRect::translated(const PkPoint &p) const noexcept
{ return PkRect(PkPoint(x1 + p.x(), y1 + p.y()), PkPoint(x2 + p.x(), y2 + p.y())); }

// qrect.h:299-313 —— moveTo **先算差值再赋值**，顺序不能换（换了之后
// x2 += ax - x1 里的 x1 已经是新值，宽度就丢了）。
constexpr inline void PkRect::moveTo(int ax, int ay) noexcept
{
    x2 += ax - x1;
    y2 += ay - y1;
    x1 = ax;
    y1 = ay;
}

constexpr inline void PkRect::moveTo(const PkPoint &p) noexcept
{
    x2 += p.x() - x1;
    y2 += p.y() - y1;
    x1 = p.x();
    y1 = p.y();
}

constexpr inline void PkRect::moveLeft(int pos) noexcept
{ x2 += (pos - x1); x1 = pos; }

constexpr inline void PkRect::moveTop(int pos) noexcept
{ y2 += (pos - y1); y1 = pos; }

// qrect.h:333-337 —— moveTopLeft 就是两次 move，**不是** setTopLeft。
constexpr inline void PkRect::moveTopLeft(const PkPoint &p) noexcept
{
    moveLeft(p.x());
    moveTop(p.y());
}

// qrect.h:357-365 —— ⚠ 用的是**跨距 x2-x1**（比 width() 少 1），不是 width()。
// 于是偶数宽的矩形 moveCenter(p) 之后 center() 并不精确回到 p
//（(0,0,10,10).moveCenter(0,0) 得到 coords(-4,-4,5,5)，center() 是 0 —— 恰好回去了；
// 而奇数宽 (0,0,11,11) 得到 coords(-5,-5,5,5)）。照抄，不"修正"。
constexpr inline void PkRect::moveCenter(const PkPoint &p) noexcept
{
    int w = x2 - x1;
    int h = y2 - y1;
    x1 = p.x() - w/2;
    y1 = p.y() - h/2;
    x2 = x1 + w;
    y2 = y1 + h;
}

// qrect.h:367-397 —— getRect/setRect 说的是 (x,y,w,h)，
// getCoords/setCoords 说的是 (x1,y1,x2,y2)。两对差一个 ±1，别混。
constexpr inline void PkRect::getRect(int *ax, int *ay, int *aw, int *ah) const
{
    *ax = x1;
    *ay = y1;
    *aw = x2 - x1 + 1;
    *ah = y2 - y1 + 1;
}

constexpr inline void PkRect::setRect(int ax, int ay, int aw, int ah) noexcept
{
    x1 = ax;
    y1 = ay;
    x2 = (ax + aw - 1);
    y2 = (ay + ah - 1);
}

constexpr inline void PkRect::getCoords(int *xp1, int *yp1, int *xp2, int *yp2) const
{
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

constexpr inline void PkRect::setCoords(int xp1, int yp1, int xp2, int yp2) noexcept
{
    x1 = xp1;
    y1 = yp1;
    x2 = xp2;
    y2 = yp2;
}

// qrect.h:399-408 —— adjusted 走 (topLeft,bottomRight) 构造，所以**不做 ±1**。
constexpr inline PkRect PkRect::adjusted(int xp1, int yp1, int xp2, int yp2) const noexcept
{ return PkRect(PkPoint(x1 + xp1, y1 + yp1), PkPoint(x2 + xp2, y2 + yp2)); }

constexpr inline void PkRect::adjust(int dx1, int dy1, int dx2, int dy2) noexcept
{
    x1 += dx1;
    y1 += dy1;
    x2 += dx2;
    y2 += dy2;
}

// qrect.h:410-420 —— setWidth/setHeight/setSize 都**锚定左上角**改右下坐标。
constexpr inline void PkRect::setWidth(int w) noexcept
{ x2 = (x1 + w - 1); }

constexpr inline void PkRect::setHeight(int h) noexcept
{ y2 = (y1 + h - 1); }

constexpr inline void PkRect::setSize(const PkSize &s) noexcept
{
    x2 = (s.width()  + x1 - 1);
    y2 = (s.height() + y1 - 1);
}

// qrect.h:422-430 —— 两个标量重载都转发到 contains(PkPoint, bool)。
// **仍然各写一条**，因为它们是两个独立的重载（默认实参不同），
// 对拍那边按规则三也各有一条 rec()。
inline bool PkRect::contains(int ax, int ay, bool aproper) const noexcept
{
    return contains(PkPoint(ax, ay), aproper);
}

inline bool PkRect::contains(int ax, int ay) const noexcept
{
    return contains(PkPoint(ax, ay), false);
}

inline PkRect &PkRect::operator|=(const PkRect &r) noexcept
{
    *this = *this | r;
    return *this;
}

inline PkRect &PkRect::operator&=(const PkRect &r) noexcept
{
    *this = *this & r;
    return *this;
}

// qrect.h:482-491 —— marginsAdded 往外扩（左上角减、右下角加），
// marginsRemoved 往里缩（符号相反）。走 (topLeft,bottomRight) 构造，
// 所以**不做 ±1**（与 adjusted 那条同一个理由：目标坐标已经是最终的边界值）。
constexpr inline PkRect PkRect::marginsAdded(const PkMargins &margins) const noexcept
{
    return PkRect(PkPoint(x1 - margins.left(), y1 - margins.top()),
                  PkPoint(x2 + margins.right(), y2 + margins.bottom()));
}

constexpr inline PkRect PkRect::marginsRemoved(const PkMargins &margins) const noexcept
{
    return PkRect(PkPoint(x1 + margins.left(), y1 + margins.top()),
                  PkPoint(x2 - margins.right(), y2 - margins.bottom()));
}

constexpr inline PkRect &PkRect::operator+=(const PkMargins &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

constexpr inline PkRect &PkRect::operator-=(const PkMargins &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

// qrect.h:464-478 —— 自由函数版，转发到 marginsAdded/marginsRemoved。
constexpr inline PkRect operator+(const PkRect &rectangle, const PkMargins &margins) noexcept
{
    return PkRect(PkPoint(rectangle.left() - margins.left(), rectangle.top() - margins.top()),
                  PkPoint(rectangle.right() + margins.right(), rectangle.bottom() + margins.bottom()));
}

constexpr inline PkRect operator+(const PkMargins &margins, const PkRect &rectangle) noexcept
{
    return PkRect(PkPoint(rectangle.left() - margins.left(), rectangle.top() - margins.top()),
                  PkPoint(rectangle.right() + margins.right(), rectangle.bottom() + margins.bottom()));
}

constexpr inline PkRect operator-(const PkRect &lhs, const PkMargins &rhs) noexcept
{
    return PkRect(PkPoint(lhs.left() + rhs.left(), lhs.top() + rhs.top()),
                  PkPoint(lhs.right() - rhs.right(), lhs.bottom() - rhs.bottom()));
}

// qrect.h:444-452 —— intersected/united 是 &/| 的具名别名，**一个字都不加**。
inline PkRect PkRect::intersected(const PkRect &other) const noexcept
{
    return *this & other;
}

inline PkRect PkRect::united(const PkRect &r) const noexcept
{
    return *this | r;
}

// qrect.h:454-462 —— ⚠ 比的是**四个内部坐标**，不是 x/y/w/h。两个都 isNull
// 但位置不同的矩形（(0,0,0,0) 与 (5,5,0,0)）因此**不相等**。
// 比较顺序也照抄（x1,x2,y1,y2），取值上无差别，形态上保持一致。
constexpr inline bool operator==(const PkRect &r1, const PkRect &r2) noexcept
{
    return r1.x1==r2.x1 && r1.x2==r2.x2 && r1.y1==r2.y1 && r1.y2==r2.y2;
}

constexpr inline bool operator!=(const PkRect &r1, const PkRect &r2) noexcept
{
    return r1.x1!=r2.x1 || r1.x2!=r2.x2 || r1.y1!=r2.y1 || r1.y2!=r2.y2;
}

// ---------------------------------------------------------------------------
// PkRectF —— QRectF 的零 Qt 替代。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtCore/qrect.h:511-875（QRectF 那一半），
// 来源行号标在各项上方；七个 out-of-line 成员（normalized / operator| /
// operator& / contains ×2 / intersects / toAlignedRect）在 PkRect.cpp。
//
// ⚠ **它与上面的 PkRect 几乎处处不同，抄错一条整片语义就走样。** 逐条列出
// （每一条都有真 Qt 5.15.7 探针的实测输出撑着）：
//
//   · **内部存的是 xp/yp/w/h（左上角 + 宽高），不是四个边界坐标。** 这是与
//     PkRect 最根本的不对称 —— Qt 自己就是这么不对称的。后果：
//     `setLeft` 在 PkRectF 里**保右边界、改宽度**（`diff = pos - xp; xp += diff;
//     w -= diff;`），在 PkRect 里只摆一个坐标；`setWidth` 在 PkRectF 里就是
//     `w = aw`，在 PkRect 里是 `x2 = x1 + w - 1`。
//   · **right() == xp + w，没有差一**（PkRect 是 x1 + width() - 1）。实测
//     `QRectF(0,0,10,10).right() == 10`，而 `QRect(0,0,10,10).right() == 9`。
//     Krita 里最常见的一格误差就来自这两者混用。
//   · **三谓词的公式与 PkRect 全不一样**：
//       isNull  = `w == 0. && h == 0.`     （PkRect：x2 == x1-1 && y2 == y1-1）
//       isEmpty = `w <= 0. || h <= 0.`     （PkRect：x1 > x2 || y1 > y2，等价于 <1）
//       isValid = `w > 0. && h > 0.`       （PkRect：x1 <= x2 && y1 <= y2，等价于 >=0）
//     实测：`(0,0,-0.0,-0.0)` **isNull=1**（-0.0 == 0. 为真）；
//     `(0,0,5e-324,5e-324)` isValid=**1**（次正规也算正宽高）；
//     `(0,0,nan,1)` isEmpty=**0** 且 isValid=**0**（NaN 让两个比较都为假 ——
//     **三谓词在 NaN 上互不为补**，抄成 `!isValid()` 会错整片）。
//   · **默认构造是 (0.,0.,0.,0.)**，不是 PkRect 的 (0,0,-1,-1) 哨兵。
//   · normalized() 的条件是 **`w < 0`**（PkRect 是 `x2 < x1 - 1`），交换后
//     `xp += w; w = -w;`。实测：`(0,0,-0.0,1).normalized()` **不交换**且
//     w 仍是 -0.0；`(0,0,nan,1)` 原样返回。
//   · **toRect() 不是"对 x/y/w/h 各做一次 qRound"**（那是个流传很广的误解）：
//     它是 `PkRect(PkPoint(qRound(xp), qRound(yp)),
//                  PkPoint(qRound(xp+w)-1, qRound(yp+h)-1))` ——
//     取整发生在**四条边**上，右下角再各减 1 换成 PkRect 的坐标表示。
//   · **toAlignedRect() 是 floor(left)/floor(top)/ceil(right)/ceil(bottom) 向外扩**，
//     与 toRect 常常不同：实测 `(-1.5,-1.5,1,1)` toRect=(-1,-1,1,1) 而
//     toAlignedRect=(-2,-2,2,2)。边界恰为整数时 ceil 不进位
//     （`(0,0,10,10).toAlignedRect()` 就是 (0,0,10,10)）。
//     实测调用点 toAlignedRect **64 次** / toRect **18 次**，都是真调用点。
//   · **operator== 是模糊比较**（四个分量各一次 pkQtFuzzyCompare），不是位相等：
//     实测 `(1,1,1,1)==(1+1e-13,1,1,1)` 为真、`(inf,0,1,1)==(-inf,0,1,1)` 也为真
//     （两侧差为 nan，`nan <= x` 恒假 …… 实为 `qAbs(inf-(-inf))*1e12 <= inf`
//     即 `inf <= inf` 为**真**）。**用 pkQtFuzzy* 而不是 qFuzzy***：后者在共存
//     路径上是 #define，会在预处理期把函数体换掉（tests/rectf_macro_proof.cpp
//     钉住这一条；理由全文在 PkGlobal.h 的 pkQtFuzzyCompare 上方）。
//   · **getRect / getCoords 同样没有 noexcept**，其余成员（含 contains /
//     intersects / operator| / operator& / normalized / toRect / toAlignedRect）
//     全有 —— 真 Qt 5.15.7 实测。
//   · contains 一族**没有 proper 参数**（PkRect 那边有），所以只有三个重载。
//
// 明确不实现（与 PkRect 同一份归属表，Rect 族实测调用点 0；表在 README）：
//   · setTopRight / setBottomLeft
//   · moveRight / moveBottom / moveTopRight / moveBottomRight / moveBottomLeft
//   · transposed —— 三形态命中的 5 处接收者全不是 Rect 族
//   · unite / intersect（Qt5 已废弃的别名）
//   · toCGRect / fromCGRect（Darwin 专有）
//   · qHash / QDataStream 的 <<>> / QDebug 的 <<（归 R-02 / R-12 / R-08）
//
// R-21 T1 新增（互操作）：marginsAdded(const PkMarginsF&) /
// marginsRemoved(const PkMarginsF&) / operator+=(const PkMarginsF&) /
// operator-=(const PkMarginsF&)，加自由函数 operator+/-(const PkRectF&,
// const PkMarginsF&)。**⚠ 吃的是 PkMarginsF 不是 PkMargins**——真探针实测
// `QRectF::marginsAdded` 的签名是 `const QMarginsF&`，与 QRect 那边吃
// `QMargins` 不同（PkMargins→PkMarginsF 有隐式提升，探针
// `rf.marginsAdded(QMargins(...))` 靠这条提升编过，不是重载决议出的第二个
// 签名）。
// ---------------------------------------------------------------------------

class PkRectF
{
public:
    // qrect.h:514 —— ⚠ **(0.,0.,0.,0.)**：与 PkRect 的 (0,0,-1,-1) 哨兵不同。
    // Qt 把函数体写在类体里，这里挪到下面与其余四个构造并排 —— 理由与 PkRect()
    // 那条相同（run_oracle.sh 的规则三闸门按**类体里的纯声明**解析重载清单）。
    constexpr PkRectF() noexcept;
    constexpr PkRectF(const PkPointF &topleft, const PkSizeF &size) noexcept;
    constexpr PkRectF(const PkPointF &topleft, const PkPointF &bottomRight) noexcept;
    constexpr PkRectF(qreal left, qreal top, qreal width, qreal height) noexcept;
    // qrect.h:518 —— **非 explicit**：PkRect 到 PkRectF 是隐式提升，
    // `QRectF r = someQRect;` 这类调用点靠它。
    constexpr PkRectF(const PkRect &rect) noexcept;

    constexpr inline bool isNull() const noexcept;
    constexpr inline bool isEmpty() const noexcept;
    constexpr inline bool isValid() const noexcept;
    // qrect.h:523 —— out-of-line，**不是** constexpr。定义在 PkRect.cpp。
    PkRectF normalized() const noexcept;

    constexpr inline qreal left() const noexcept;
    constexpr inline qreal top() const noexcept;
    constexpr inline qreal right() const noexcept;
    constexpr inline qreal bottom() const noexcept;

    constexpr inline qreal x() const noexcept;
    constexpr inline qreal y() const noexcept;
    constexpr inline void setLeft(qreal pos) noexcept;
    constexpr inline void setTop(qreal pos) noexcept;
    constexpr inline void setRight(qreal pos) noexcept;
    constexpr inline void setBottom(qreal pos) noexcept;
    constexpr inline void setX(qreal pos) noexcept;
    constexpr inline void setY(qreal pos) noexcept;

    constexpr inline PkPointF topLeft() const noexcept;
    constexpr inline PkPointF bottomRight() const noexcept;
    constexpr inline PkPointF topRight() const noexcept;
    constexpr inline PkPointF bottomLeft() const noexcept;
    constexpr inline PkPointF center() const noexcept;

    constexpr inline void setTopLeft(const PkPointF &p) noexcept;
    constexpr inline void setBottomRight(const PkPointF &p) noexcept;

    constexpr inline void moveLeft(qreal pos) noexcept;
    constexpr inline void moveTop(qreal pos) noexcept;
    constexpr inline void moveTopLeft(const PkPointF &p) noexcept;
    constexpr inline void moveCenter(const PkPointF &p) noexcept;

    constexpr inline void translate(qreal dx, qreal dy) noexcept;
    constexpr inline void translate(const PkPointF &p) noexcept;
    constexpr inline PkRectF translated(qreal dx, qreal dy) const noexcept;
    constexpr inline PkRectF translated(const PkPointF &p) const noexcept;

    constexpr inline void moveTo(qreal x, qreal y) noexcept;
    constexpr inline void moveTo(const PkPointF &p) noexcept;

    constexpr inline void setRect(qreal x, qreal y, qreal w, qreal h) noexcept;
    // qrect.h:572 —— ⚠ **没有 noexcept**（同一对的 setRect 有）。与 PkRect 同一处
    // 不对称，真 Qt 5.15.7 实测确认 getRect=0 / setRect=1。
    constexpr inline void getRect(qreal *x, qreal *y, qreal *w, qreal *h) const;

    constexpr inline void setCoords(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    // qrect.h:575 —— ⚠ 同样**没有 noexcept**。
    constexpr inline void getCoords(qreal *x1, qreal *y1, qreal *x2, qreal *y2) const;

    constexpr inline void adjust(qreal x1, qreal y1, qreal x2, qreal y2) noexcept;
    constexpr inline PkRectF adjusted(qreal x1, qreal y1, qreal x2, qreal y2) const noexcept;

    constexpr inline PkSizeF size() const noexcept;
    constexpr inline qreal width() const noexcept;
    constexpr inline qreal height() const noexcept;
    constexpr inline void setWidth(qreal w) noexcept;
    constexpr inline void setHeight(qreal h) noexcept;
    constexpr inline void setSize(const PkSizeF &s) noexcept;

    // qrect.h:587-590 —— 前两个 out-of-line（PkRect.cpp），后两个转发到前两个。
    PkRectF operator|(const PkRectF &r) const noexcept;
    PkRectF operator&(const PkRectF &r) const noexcept;
    inline PkRectF &operator|=(const PkRectF &r) noexcept;
    inline PkRectF &operator&=(const PkRectF &r) noexcept;

    // R-21 T1 —— qrect.h:599-602。四个互操作成员，**签名吃 PkMarginsF**
    // （浮点版），与 PkRect 那边吃 PkMargins（整数版）不同——探针实测确认。
    constexpr inline PkRectF marginsAdded(const PkMarginsF &margins) const noexcept;
    constexpr inline PkRectF marginsRemoved(const PkMarginsF &margins) const noexcept;
    constexpr inline PkRectF &operator+=(const PkMarginsF &margins) noexcept;
    constexpr inline PkRectF &operator-=(const PkMarginsF &margins) noexcept;

    // qrect.h:592-594 —— ⚠ **没有 proper 参数**（PkRect 那边有）。
    bool contains(const PkRectF &r) const noexcept;
    bool contains(const PkPointF &p) const noexcept;
    inline bool contains(qreal x, qreal y) const noexcept;
    inline PkRectF united(const PkRectF &other) const noexcept;
    inline PkRectF intersected(const PkRectF &other) const noexcept;
    bool intersects(const PkRectF &r) const noexcept;

    friend constexpr inline bool operator==(const PkRectF &, const PkRectF &) noexcept;
    friend constexpr inline bool operator!=(const PkRectF &, const PkRectF &) noexcept;

    constexpr inline PkRect toRect() const noexcept;
    // qrect.h:613 —— out-of-line（qrect.cpp），**不是** constexpr。
    PkRect toAlignedRect() const noexcept;

private:
    // qrect.h:621-624。⚠ **左上角 + 宽高**，与 PkRect 的四坐标表示不同。
    qreal xp;
    qreal yp;
    qreal w;
    qreal h;
};

// ── PkRectF inline 成员（qrect.h:644-875）────────────────────────────────

constexpr inline PkRectF::PkRectF() noexcept : xp(0.), yp(0.), w(0.), h(0.) {}

// qrect.h:644-663 —— 四个带参构造。⚠ 只有 (topLeft,bottomRight) 那一个做减法
// （`w = br.x() - tl.x()`），(l,t,w,h) 与 (topLeft,size) 直接摆四个字段，
// PkRect 那边的 **+w-1** 在这里一个都没有。
constexpr inline PkRectF::PkRectF(qreal aleft, qreal atop, qreal awidth, qreal aheight) noexcept
    : xp(aleft), yp(atop), w(awidth), h(aheight) {}

constexpr inline PkRectF::PkRectF(const PkPointF &atopLeft, const PkSizeF &asize) noexcept
    : xp(atopLeft.x()), yp(atopLeft.y()), w(asize.width()), h(asize.height()) {}

constexpr inline PkRectF::PkRectF(const PkPointF &atopLeft, const PkPointF &abottomRight) noexcept
    : xp(atopLeft.x()), yp(atopLeft.y()),
      w(abottomRight.x() - atopLeft.x()), h(abottomRight.y() - atopLeft.y()) {}

// qrect.h:660-663 —— ⚠ 走的是 PkRect 的 **x()/y()/width()/height()**（差一已经
//在 width() 里算过了），不是 left()/right()。实测
// `PkRectF(PkRect(0,0,10,10)).right() == 10` 而 `PkRect(0,0,10,10).right() == 9`。
constexpr inline PkRectF::PkRectF(const PkRect &r) noexcept
    : xp(r.x()), yp(r.y()), w(r.width()), h(r.height()) {}

// qrect.h:670-679 —— **三条公式逐字照抄**。Qt 在这两条上关掉了 -Wfloat-equal
// （`w == 0.` 是有意的浮点相等），我们没有那套 pragma 宏，行为一致。
// ⚠ 三者在 NaN 上**互不为补**：w=nan 时 isEmpty 与 isValid 同时为假。
constexpr inline bool PkRectF::isNull() const noexcept
{ return w == 0. && h == 0.; }

constexpr inline bool PkRectF::isEmpty() const noexcept
{ return w <= 0. || h <= 0.; }

constexpr inline bool PkRectF::isValid() const noexcept
{ return w > 0. && h > 0.; }

// qrect.h:525-528 —— ⚠ **没有差一**：right() 就是 xp + w。
constexpr inline qreal PkRectF::left() const noexcept
{ return xp; }

constexpr inline qreal PkRectF::top() const noexcept
{ return yp; }

constexpr inline qreal PkRectF::right() const noexcept
{ return xp + w; }

constexpr inline qreal PkRectF::bottom() const noexcept
{ return yp + h; }

constexpr inline qreal PkRectF::x() const noexcept
{ return xp; }

constexpr inline qreal PkRectF::y() const noexcept
{ return yp; }

// qrect.h:687-697 —— ⚠ set* 一族在这里**保住对边、改宽高**（PkRect 那边是
// 只摆一个坐标）。setLeft 走 `diff` 中间量而不是 `w = w + xp - pos`：
// 浮点下两者不等价（(xp+diff)-xp 与 xp+(diff-...) 的舍入不同），照抄。
constexpr inline void PkRectF::setLeft(qreal pos) noexcept
{ qreal diff = pos - xp; xp += diff; w -= diff; }

constexpr inline void PkRectF::setRight(qreal pos) noexcept
{ w = pos - xp; }

constexpr inline void PkRectF::setTop(qreal pos) noexcept
{ qreal diff = pos - yp; yp += diff; h -= diff; }

constexpr inline void PkRectF::setBottom(qreal pos) noexcept
{ h = pos - yp; }

// qrect.h:536-537 —— setX/setY 是 setLeft/setTop 的别名（**不是**直接写 xp：
// 它们跟着改宽高）。这与 PkRect 的 setX/setLeft 关系一致，但语义完全不同。
constexpr inline void PkRectF::setX(qreal pos) noexcept
{ setLeft(pos); }

constexpr inline void PkRectF::setY(qreal pos) noexcept
{ setTop(pos); }

constexpr inline void PkRectF::setTopLeft(const PkPointF &p) noexcept
{ setLeft(p.x()); setTop(p.y()); }

constexpr inline void PkRectF::setBottomRight(const PkPointF &p) noexcept
{ setRight(p.x()); setBottom(p.y()); }

constexpr inline PkPointF PkRectF::topLeft() const noexcept
{ return PkPointF(xp, yp); }

constexpr inline PkPointF PkRectF::bottomRight() const noexcept
{ return PkPointF(xp + w, yp + h); }

constexpr inline PkPointF PkRectF::topRight() const noexcept
{ return PkPointF(xp + w, yp); }

constexpr inline PkPointF PkRectF::bottomLeft() const noexcept
{ return PkPointF(xp, yp + h); }

// qrect.h:711-712 —— ⚠ **`xp + w/2`**（先除后加），不是 `(left+right)/2`。
// 浮点下两者的舍入不同，且 PkRect 那边走的是 qint64 中间量 —— 三种写法互不等价。
constexpr inline PkPointF PkRectF::center() const noexcept
{ return PkPointF(xp + w/2, yp + h/2); }

// qrect.h:714-718 —— move* 一族只摆位置，宽高天然不变。
constexpr inline void PkRectF::moveLeft(qreal pos) noexcept
{ xp = pos; }

constexpr inline void PkRectF::moveTop(qreal pos) noexcept
{ yp = pos; }

constexpr inline void PkRectF::moveTopLeft(const PkPointF &p) noexcept
{ moveLeft(p.x()); moveTop(p.y()); }

// qrect.h:738-739 —— ⚠ 用的是 **w/2**（真的一半），PkRect 那边用的是不带 +1 的
// 跨距 x2-x1。实测 `(1,2,3,4).moveCenter(0,0)` → x=-1.5 y=-2。
constexpr inline void PkRectF::moveCenter(const PkPointF &p) noexcept
{ xp = p.x() - w/2; yp = p.y() - h/2; }

constexpr inline qreal PkRectF::width() const noexcept
{ return w; }

constexpr inline qreal PkRectF::height() const noexcept
{ return h; }

constexpr inline PkSizeF PkRectF::size() const noexcept
{ return PkSizeF(w, h); }

// qrect.h:750-772 —— translate / moveTo 只动 xp/yp。
constexpr inline void PkRectF::translate(qreal dx, qreal dy) noexcept
{
    xp += dx;
    yp += dy;
}

constexpr inline void PkRectF::translate(const PkPointF &p) noexcept
{
    xp += p.x();
    yp += p.y();
}

constexpr inline void PkRectF::moveTo(qreal ax, qreal ay) noexcept
{
    xp = ax;
    yp = ay;
}

constexpr inline void PkRectF::moveTo(const PkPointF &p) noexcept
{
    xp = p.x();
    yp = p.y();
}

constexpr inline PkRectF PkRectF::translated(qreal dx, qreal dy) const noexcept
{ return PkRectF(xp + dx, yp + dy, w, h); }

constexpr inline PkRectF PkRectF::translated(const PkPointF &p) const noexcept
{ return PkRectF(xp + p.x(), yp + p.y(), w, h); }

// qrect.h:783-813 —— getRect/setRect 说的是 (x,y,w,h)、getCoords/setCoords 说的
// 是 (x1,y1,x2,y2)。⚠ 与 PkRect 不同的是这里**差的是加减法而不是 ±1**：
// getCoords 输出 `xp + w`，setCoords 存 `xp2 - xp1`。
constexpr inline void PkRectF::getRect(qreal *ax, qreal *ay, qreal *aaw, qreal *aah) const
{
    *ax = this->xp;
    *ay = this->yp;
    *aaw = this->w;
    *aah = this->h;
}

constexpr inline void PkRectF::setRect(qreal ax, qreal ay, qreal aaw, qreal aah) noexcept
{
    this->xp = ax;
    this->yp = ay;
    this->w = aaw;
    this->h = aah;
}

constexpr inline void PkRectF::getCoords(qreal *xp1, qreal *yp1, qreal *xp2, qreal *yp2) const
{
    *xp1 = xp;
    *yp1 = yp;
    *xp2 = xp + w;
    *yp2 = yp + h;
}

constexpr inline void PkRectF::setCoords(qreal xp1, qreal yp1, qreal xp2, qreal yp2) noexcept
{
    xp = xp1;
    yp = yp1;
    w = xp2 - xp1;
    h = yp2 - yp1;
}

// qrect.h:815-819 —— ⚠ 宽高的增量是 **xp2 - xp1**（两个增量之差），
// 不是 PkRect 那种"四个坐标各加各的"。写成 `w += xp2` 会让 adjust(1,1,1,1)
// 变成放大而不是平移（实测真 Qt：`(1,2,3,4).adjust(1,1,1,1)` → (2,3,3,4)，宽高不变）。
constexpr inline void PkRectF::adjust(qreal xp1, qreal yp1, qreal xp2, qreal yp2) noexcept
{ xp += xp1; yp += yp1; w += xp2 - xp1; h += yp2 - yp1; }

constexpr inline PkRectF PkRectF::adjusted(qreal xp1, qreal yp1, qreal xp2, qreal yp2) const noexcept
{ return PkRectF(xp + xp1, yp + yp1, w + xp2 - xp1, h + yp2 - yp1); }

// qrect.h:821-831 —— ⚠ 直接写字段，**锚定的是左上角**（与 PkRect 语义相同，
// 但那边要算 x2 = x1 + w - 1）。
constexpr inline void PkRectF::setWidth(qreal aw) noexcept
{ this->w = aw; }

constexpr inline void PkRectF::setHeight(qreal ah) noexcept
{ this->h = ah; }

constexpr inline void PkRectF::setSize(const PkSizeF &s) noexcept
{
    w = s.width();
    h = s.height();
}

// qrect.h:833-836 —— 转发到 contains(PkPointF)。**仍然自己一条 rec()**（规则三）。
inline bool PkRectF::contains(qreal ax, qreal ay) const noexcept
{
    return contains(PkPointF(ax, ay));
}

inline PkRectF &PkRectF::operator|=(const PkRectF &r) noexcept
{
    *this = *this | r;
    return *this;
}

inline PkRectF &PkRectF::operator&=(const PkRectF &r) noexcept
{
    *this = *this & r;
    return *this;
}

// qrect.h:895-915 —— marginsAdded/marginsRemoved 走 (topLeft,size) 构造：
// 左上角按符号各减/各加，宽高各按两侧之和加/减——与 PkRect 那边走
// (topLeft,bottomRight) 构造、直接摆右下坐标不是同一个形状（PkRectF 内部
// 存的本来就是 xp/yp/w/h，不需要先算出右下角再摆回去）。
constexpr inline PkRectF PkRectF::marginsAdded(const PkMarginsF &margins) const noexcept
{
    return PkRectF(PkPointF(xp - margins.left(), yp - margins.top()),
                   PkSizeF(w + margins.left() + margins.right(), h + margins.top() + margins.bottom()));
}

constexpr inline PkRectF PkRectF::marginsRemoved(const PkMarginsF &margins) const noexcept
{
    return PkRectF(PkPointF(xp + margins.left(), yp + margins.top()),
                   PkSizeF(w - margins.left() - margins.right(), h - margins.top() - margins.bottom()));
}

constexpr inline PkRectF &PkRectF::operator+=(const PkMarginsF &margins) noexcept
{
    *this = marginsAdded(margins);
    return *this;
}

constexpr inline PkRectF &PkRectF::operator-=(const PkMarginsF &margins) noexcept
{
    *this = marginsRemoved(margins);
    return *this;
}

// qrect.h:877-891 —— 自由函数版，转发到 marginsAdded/marginsRemoved。
constexpr inline PkRectF operator+(const PkRectF &lhs, const PkMarginsF &rhs) noexcept
{
    return PkRectF(PkPointF(lhs.left() - rhs.left(), lhs.top() - rhs.top()),
                   PkSizeF(lhs.width() + rhs.left() + rhs.right(), lhs.height() + rhs.top() + rhs.bottom()));
}

constexpr inline PkRectF operator+(const PkMarginsF &lhs, const PkRectF &rhs) noexcept
{
    return PkRectF(PkPointF(rhs.left() - lhs.left(), rhs.top() - lhs.top()),
                   PkSizeF(rhs.width() + lhs.left() + lhs.right(), rhs.height() + lhs.top() + lhs.bottom()));
}

constexpr inline PkRectF operator-(const PkRectF &lhs, const PkMarginsF &rhs) noexcept
{
    return PkRectF(PkPointF(lhs.left() + rhs.left(), lhs.top() + rhs.top()),
                   PkSizeF(lhs.width() - rhs.left() - rhs.right(), lhs.height() - rhs.top() - rhs.bottom()));
}

inline PkRectF PkRectF::intersected(const PkRectF &r) const noexcept
{
    return *this & r;
}

inline PkRectF PkRectF::united(const PkRectF &r) const noexcept
{
    return *this | r;
}

// qrect.h:860-870 —— ⚠ **模糊比较**，四个分量各一次；PkRect 那边是整数精确相等。
// **用 pkQtFuzzyCompare 而不是 qFuzzyCompare**：后者在「pk/test 的垫片先进 TU」
// 那条真实共存路径上是 #define，会在**预处理期**把这里的函数体换成 pk/test 的
// 实现（阈值相同但零侧分支不同）。对拍覆盖不到这类偷换 —— 编译行里根本没有那份
// 垫片；钉住它的是 tests/rectf_macro_proof.cpp。
// != **不是** ==(的取反)：它是四条 `!fuzzy` 的或，与 Qt 逐字一致（NaN 上两者
// 可以同时为真/假，取反写法会分家）。
constexpr inline bool operator==(const PkRectF &r1, const PkRectF &r2) noexcept
{
    return pkQtFuzzyCompare(r1.xp, r2.xp) && pkQtFuzzyCompare(r1.yp, r2.yp)
           && pkQtFuzzyCompare(r1.w, r2.w) && pkQtFuzzyCompare(r1.h, r2.h);
}

constexpr inline bool operator!=(const PkRectF &r1, const PkRectF &r2) noexcept
{
    return !pkQtFuzzyCompare(r1.xp, r2.xp) || !pkQtFuzzyCompare(r1.yp, r2.yp)
           || !pkQtFuzzyCompare(r1.w, r2.w) || !pkQtFuzzyCompare(r1.h, r2.h);
}

// qrect.h:872-875 —— ⚠ **不是"对 x/y/w/h 各做一次 qRound"**。取整发生在四条边
// 上：左上角 qRound(xp)/qRound(yp)，右下角 qRound(xp+w)-1 / qRound(yp+h)-1
// （-1 是换成 PkRect 的坐标表示）。走的是 (topLeft,bottomRight) 构造，所以
// 构造那边不会再减一次。实测 `(0.49999999999999994,0,1,1).toRect()` 的内部坐标
// 是 (1,0,1,0)：qRound 在左边界进位到 1，而 xp+w 在 double 里恰好舍入成 1.5、
// qRound 给 2、减 1 得 1 —— 分开对 w 取整得不到这个结果。
constexpr inline PkRect PkRectF::toRect() const noexcept
{
    return PkRect(PkPoint(qRound(xp), qRound(yp)),
                  PkPoint(qRound(xp + w) - 1, qRound(yp + h) - 1));
}

#endif // PK_GEOMETRY_PKRECT_H
