#ifndef PK_GEOMETRY_PKRECT_H
#define PK_GEOMETRY_PKRECT_H

#include "PkGlobal.h"
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
//   · marginsAdded / marginsRemoved / operator±=(QMargins) / operator±(QMargins)
//     —— QMargins 实测 2 次/1 文件，本来就不在 R-03 交付范围
//   · toCGRect / fromCGRect（Darwin 专有）
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

#endif // PK_GEOMETRY_PKRECT_H
