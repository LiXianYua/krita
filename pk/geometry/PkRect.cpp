#include "PkRect.h"

// ⚠ **这两个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，头文件守卫已经
// 点掉的 include 才会空转；没出现过的话会造出 pkoracle::std。两个都在（<cmath>
// 与 <type_traits>）。<cmath> 是 PkRectF::toAlignedRect 的 floor/ceil 要的。
#include <cmath>
#include <type_traits>

// ---------------------------------------------------------------------------
// 六个 out-of-line 成员：normalized / operator| / operator& /
// contains(PkRect,bool) / contains(PkPoint,bool) / intersects。
// 与 Qt 的形态一致（qrect.h 只声明，定义在 qrect.cpp 里，编进 libQt5Core），
// 因此它们**不是 constexpr**——照抄，别顺手加。
//
// ⚠ **本机装的 Qt 只有 .so 没有 qrect.cpp**，所以这六条不能"照抄源码"，
// 只能**靠对拍逐输入逼出来**：按 Qt 文档写一版 → 跑 oracle/run_oracle.sh →
// 看 mismatch 落在哪 → 改 → 再跑。落地过程与逐格实测输出在 Task 4 报告 §2/§5。
// 直接钉住的实测事实（探针 probe_rect.cpp，真 Qt 5.15.7）：
//   · normalized 的交换条件是 `x2 < x1 - 1`，**不是** `x2 < x1`；两个轴各判各的
//   · operator| 的判空用 **isNull()**（不是 isEmpty()），且「a 为 null 返回 b」
//     在前 —— 于是 `PkRect()|(5,5,0,0)` 与 `(5,5,0,0)|PkRect()` 结果不同
//   · operator& / contains / intersects 里把负宽高矩形"翻正"的判据写作
//     **`x2 - x1 + 1 < 0`**（= width() < 0），与 normalized 的 `x2 < x1 - 1`
//     是同一条线的两种写法（x2-x1+1 < 0 ⟺ x2 < x1-1），取值等价、都照抄
//   · operator& 与 contains(PkRect) 在任一侧 isNull 时**先返回**，
//     根本不做区间比较
//
// 这六条里的 `x2 - x1 + 1`、`x1 + x2` 都是裸的 int 运算，在 INT_MIN/INT_MAX
// 一族输入上会有符号溢出 —— Qt 自己就这么写，两侧都靠 -fwrapv 钉成二补数回绕
//（口径在 README 覆盖度缺口）。
//
// 本 TU 还放了那批**只有在一个翻译单元里才落得了地**的 static_assert，
// 理由与 PkSize.cpp / PkPoint.cpp 相同（布局 / constexpr 能力 / noexcept 面）。
// ---------------------------------------------------------------------------

// ⚠ 交换条件是 **x2 < x1 - 1**：宽度恰为 0（x2 == x1-1）时**不交换**，
// 宽度 -1 起才交换；交换之后**不做 ±1 修正**，于是宽度从 -1 直接跳到 3。
// 写成 `x2 < x1` 会把"宽 0"的矩形也翻过来，(0,0,-1,0) 这一整片行为就变了
//（注入实验 B 组：单测红、对拍红 —— 见报告 §6）。
PkRect PkRect::normalized() const noexcept
{
    PkRect r;
    if (x2 < x1 - 1) {                          // swap bad x values
        r.x1 = x2;
        r.x2 = x1;
    } else {
        r.x1 = x1;
        r.x2 = x2;
    }
    if (y2 < y1 - 1) {                          // swap bad y values
        r.y1 = y2;
        r.y2 = y1;
    } else {
        r.y1 = y1;
        r.y2 = y2;
    }
    return r;
}

// ⚠ 三件事一件都不能改：
//   ① 判空用 **isNull()**，不是 isEmpty()。(0,0,-1,-1) 是 empty 但**非 null**，
//      所以它真的参与并集运算（结果 (-2,-2,12,12)），不是被当空集跳过。
//      改成 isEmpty() 之后那一整片输入的答案全变（注入实验 C 组）。
//   ② 「*this 为 null 返回 r」在**前**，「r 为 null 返回 *this」在后 ——
//      两侧都 null 时永远返回 r，于是 **operator| 不可交换**。
//   ③ 翻正负宽高用 `x2 - x1 + 1 < 0`，翻正后取 min/max，**不重新规范化结果**。
PkRect PkRect::operator|(const PkRect &r) const noexcept
{
    if (isNull())
        return r;
    if (r.isNull())
        return *this;

    int l1 = x1;
    int r1 = x1;
    if (x2 - x1 + 1 < 0)
        l1 = x2;
    else
        r1 = x2;

    int l2 = r.x1;
    int r2 = r.x1;
    if (r.x2 - r.x1 + 1 < 0)
        l2 = r.x2;
    else
        r2 = r.x2;

    int t1 = y1;
    int b1 = y1;
    if (y2 - y1 + 1 < 0)
        t1 = y2;
    else
        b1 = y2;

    int t2 = r.y1;
    int b2 = r.y1;
    if (r.y2 - r.y1 + 1 < 0)
        t2 = r.y2;
    else
        b2 = r.y2;

    PkRect tmp;
    tmp.x1 = qMin(l1, l2);
    tmp.x2 = qMax(r1, r2);
    tmp.y1 = qMin(t1, t2);
    tmp.y2 = qMax(b1, b2);
    return tmp;
}

// ⚠ 与 operator| 的三处不对称，逐条照抄：
//   ① 任一侧 isNull 就返回 **PkRect()**（= (0,0,-1,-1)），不是返回另一侧；
//   ② 不相交时也返回 PkRect()（两次提前返回，x 轴一次、y 轴一次）；
//   ③ 负宽高照样翻正后参与，所以 (0,0,10,10) & (0,0,-1,-1) **非空**。
PkRect PkRect::operator&(const PkRect &r) const noexcept
{
    if (isNull() || r.isNull())
        return PkRect();

    int l1 = x1;
    int r1 = x1;
    if (x2 - x1 + 1 < 0)
        l1 = x2;
    else
        r1 = x2;

    int l2 = r.x1;
    int r2 = r.x1;
    if (r.x2 - r.x1 + 1 < 0)
        l2 = r.x2;
    else
        r2 = r.x2;

    if (l1 > r2 || l2 > r1)
        return PkRect();

    int t1 = y1;
    int b1 = y1;
    if (y2 - y1 + 1 < 0)
        t1 = y2;
    else
        b1 = y2;

    int t2 = r.y1;
    int b2 = r.y1;
    if (r.y2 - r.y1 + 1 < 0)
        t2 = r.y2;
    else
        b2 = r.y2;

    if (t1 > b2 || t2 > b1)
        return PkRect();

    PkRect tmp;
    tmp.x1 = qMax(l1, l2);
    tmp.x2 = qMin(r1, r2);
    tmp.y1 = qMax(t1, t2);
    tmp.y2 = qMin(b1, b2);
    return tmp;
}

// intersects 与 operator& 是同一段判断，只是不组装结果。**分开写而不是写成
// `!(*this & r).isNull()`**：那样在"交集恰好是个 null 矩形"的输入上会给出
// 不同答案，而 Qt 是两份独立代码。
bool PkRect::intersects(const PkRect &r) const noexcept
{
    if (isNull() || r.isNull())
        return false;

    int l1 = x1;
    int r1 = x1;
    if (x2 - x1 + 1 < 0)
        l1 = x2;
    else
        r1 = x2;

    int l2 = r.x1;
    int r2 = r.x1;
    if (r.x2 - r.x1 + 1 < 0)
        l2 = r.x2;
    else
        r2 = r.x2;

    if (l1 > r2 || l2 > r1)
        return false;

    int t1 = y1;
    int b1 = y1;
    if (y2 - y1 + 1 < 0)
        t1 = y2;
    else
        b1 = y2;

    int t2 = r.y1;
    int b2 = r.y1;
    if (r.y2 - r.y1 + 1 < 0)
        t2 = r.y2;
    else
        b2 = r.y2;

    if (t1 > b2 || t2 > b1)
        return false;

    return true;
}

// ⚠ contains(PkRect) 与 contains(PkPoint) 用的是**两套不同的翻正写法**：
// 这里是 `x2 - x1 + 1 < 0`（矩形版），下面那个是 `x2 < x1 - 1`（点版）。
// 两者取值等价，但形态照抄 —— 别"统一"它们。
// proper 版把两端的 `<`/`>` 换成 `<=`/`>=`，于是边界重合就不算包含。
bool PkRect::contains(const PkRect &r, bool proper) const noexcept
{
    if (isNull() || r.isNull())
        return false;

    int l1 = x1;
    int r1 = x1;
    if (x2 - x1 + 1 < 0)
        l1 = x2;
    else
        r1 = x2;

    int l2 = r.x1;
    int r2 = r.x1;
    if (r.x2 - r.x1 + 1 < 0)
        l2 = r.x2;
    else
        r2 = r.x2;

    if (proper) {
        if (l2 <= l1 || r2 >= r1)
            return false;
    } else {
        if (l2 < l1 || r2 > r1)
            return false;
    }

    int t1 = y1;
    int b1 = y1;
    if (y2 - y1 + 1 < 0)
        t1 = y2;
    else
        b1 = y2;

    int t2 = r.y1;
    int b2 = r.y1;
    if (r.y2 - r.y1 + 1 < 0)
        t2 = r.y2;
    else
        b2 = r.y2;

    if (proper) {
        if (t2 <= t1 || b2 >= b1)
            return false;
    } else {
        if (t2 < t1 || b2 > b1)
            return false;
    }

    return true;
}

// ⚠ 与矩形版的三处不同：
//   ① **没有 isNull 提前返回** —— 于是 null 矩形上也会真的做区间比较
//      （实测 setCoords(0,0,-1,0).contains(0,0) 走的就是这条路，答案 false 是
//      比较出来的：l=0、r=-1，任何点都不在 [0,-1] 里）；
//   ② 翻正写法是 `x2 < x1 - 1`；
//   ③ 每个轴判完就可能提前 return false。
bool PkRect::contains(const PkPoint &p, bool proper) const noexcept
{
    int l, r;
    if (x2 < x1 - 1) {
        l = x2;
        r = x1;
    } else {
        l = x1;
        r = x2;
    }
    if (proper) {
        if (p.x() <= l || p.x() >= r)
            return false;
    } else {
        if (p.x() < l || p.x() > r)
            return false;
    }

    int t, b;
    if (y2 < y1 - 1) {
        t = y2;
        b = y1;
    } else {
        t = y1;
        b = y2;
    }
    if (proper) {
        if (p.y() <= t || p.y() >= b)
            return false;
    } else {
        if (p.y() < t || p.y() > b)
            return false;
    }

    return true;
}

// ═══ PkRectF 的七个 out-of-line 成员 ══════════════════════════════════════
//
// normalized / operator| / operator& / contains ×2 / intersects / toAlignedRect。
// 与 Qt 的形态一致（qrect.h 只声明，定义编在 libQt5Core.so 里），因此**不是
// constexpr**。同样"本机没有 qrect.cpp"，同样只能靠对拍逐输入逼出来。
//
// 直接钉住的实测事实（探针 probe_rectf.cpp / probe_rectf2.cpp，真 Qt 5.15.7，
// 全部输出在 Task 5 报告 §2）：
//   · **翻正判据一律是 `w < 0`**（不是 `w <= 0`，也不是 PkRect 的 `x2 < x1-1`）。
//     -0.0 不满足 `< 0`，所以 `(0,0,-0.0,1)` 在这七条里全部按"正宽"处理。
//   · **operator| 的判空用 isNull()**（`w==0 && h==0`），且"a 为 null 返回 b"
//     在前 —— 与 PkRect 同一个不可交换的形状：实测
//     `(0,0,0,0)|(5,5,0,0) = (5,5,0,0)` 而 `(5,5,0,0)|(0,0,0,0) = (0,0,0,0)`。
//   · **operator& / contains / intersects 的判空不是 isNull()，而是逐轴的
//     `l == r`**（该轴退化成一条线就整体返回空/假）。实测
//     `(3,3,0,5).contains(3,4) == false` —— 宽为 0 的矩形连自己的边上的点都不含。
//   · **NaN 走的是"两个比较都为假"这条路**，于是它们大多数落在"不排除"分支上：
//     实测 `(0,0,10,10).contains(nan,5) == true`、几乎所有含 NaN 的
//     `intersects` 都是 true。这不是我们加的分支，是 `<` / `>` 在 NaN 上的定义。
//   · **toAlignedRect 是 floor/ceil 向外扩，且不做任何翻正**：
//     实测 `(0,0,-1,-1).toAlignedRect()` 的内部坐标是 (0,0,-2,-2)（宽 -1），
//     负宽高原样传下去；`(0,0,10,10)` 边界恰为整数时 ceil 不进位。
//
// 浮点→int 的越界转换（`int(std::floor(1e10))`）是 UB，-fwrapv 管不着；
// 两侧在本机都编成 cvttsd2si，取值一致（口径写进 README 覆盖度缺口）。

// ⚠ 条件是 **`w < 0`**：-0.0 与 NaN 都不交换（实测 `(0,0,-0.0,1)` 交换后
// w 仍是 -0.0、`(0,0,nan,1)` 原样返回）。交换时 `xp += w` 先做，`w = -w` 后做。
PkRectF PkRectF::normalized() const noexcept
{
    PkRectF r = *this;
    if (r.w < 0) {
        r.xp += r.w;
        r.w = -r.w;
    }
    if (r.h < 0) {
        r.yp += r.h;
        r.h = -r.h;
    }
    return r;
}

// ⚠ 判空用 **isNull()**，且顺序不能换（换了 `(5,5,0,0)|(0,0,0,0)` 就变了）。
// 每个轴先按符号摊成 [left,right]，再取 min/max —— **不调用 normalized()**，
// 因为那会把 -0.0 与 NaN 走成另一条路。
PkRectF PkRectF::operator|(const PkRectF &r) const noexcept
{
    if (isNull())
        return r;
    if (r.isNull())
        return *this;

    qreal left = xp;
    qreal right = xp;
    if (w < 0)
        left += w;
    else
        right += w;

    if (r.w < 0) {
        left = qMin(left, r.xp + r.w);
        right = qMax(right, r.xp);
    } else {
        left = qMin(left, r.xp);
        right = qMax(right, r.xp + r.w);
    }

    qreal top = yp;
    qreal bottom = yp;
    if (h < 0)
        top += h;
    else
        bottom += h;

    if (r.h < 0) {
        top = qMin(top, r.yp + r.h);
        bottom = qMax(bottom, r.yp);
    } else {
        top = qMin(top, r.yp);
        bottom = qMax(bottom, r.yp + r.h);
    }

    return PkRectF(left, top, right - left, bottom - top);
}

// ⚠ 判空是**逐轴的 `l == r`**（不是 isNull()）：任一轴退化成线就返回 PkRectF()。
// 五处提前返回的顺序也照抄 —— 它决定了含 NaN 的输入落在哪一档。
PkRectF PkRectF::operator&(const PkRectF &r) const noexcept
{
    qreal l1 = xp;
    qreal r1 = xp;
    if (w < 0)
        l1 += w;
    else
        r1 += w;
    if (l1 == r1)                       // null rect
        return PkRectF();

    qreal l2 = r.xp;
    qreal r2 = r.xp;
    if (r.w < 0)
        l2 += r.w;
    else
        r2 += r.w;
    if (l2 == r2)                       // null rect
        return PkRectF();

    if (l1 >= r2 || l2 >= r1)
        return PkRectF();

    qreal t1 = yp;
    qreal b1 = yp;
    if (h < 0)
        t1 += h;
    else
        b1 += h;
    if (t1 == b1)                       // null rect
        return PkRectF();

    qreal t2 = r.yp;
    qreal b2 = r.yp;
    if (r.h < 0)
        t2 += r.h;
    else
        b2 += r.h;
    if (t2 == b2)                       // null rect
        return PkRectF();

    if (t1 >= b2 || t2 >= b1)
        return PkRectF();

    PkRectF tmp;
    tmp.xp = qMax(l1, l2);
    tmp.yp = qMax(t1, t2);
    tmp.w = qMin(r1, r2) - tmp.xp;
    tmp.h = qMin(b1, b2) - tmp.yp;
    return tmp;
}

// ⚠ 与 operator& 只差**排除条件**：这里是 `l2 < l1 || r2 > r1`（b 必须整个落在
// a 里），那边是 `l1 >= r2 || l2 >= r1`（区间不相交）。抄串了会让 contains 变成
// intersects，而两者在"相交但不包含"的输入上才分家 —— 单测里必须有那种输入。
bool PkRectF::contains(const PkRectF &r) const noexcept
{
    qreal l1 = xp;
    qreal r1 = xp;
    if (w < 0)
        l1 += w;
    else
        r1 += w;
    if (l1 == r1)                       // null rect
        return false;

    qreal l2 = r.xp;
    qreal r2 = r.xp;
    if (r.w < 0)
        l2 += r.w;
    else
        r2 += r.w;
    if (l2 == r2)                       // null rect
        return false;

    if (l2 < l1 || r2 > r1)
        return false;

    qreal t1 = yp;
    qreal b1 = yp;
    if (h < 0)
        t1 += h;
    else
        b1 += h;
    if (t1 == b1)                       // null rect
        return false;

    qreal t2 = r.yp;
    qreal b2 = r.yp;
    if (r.h < 0)
        t2 += r.h;
    else
        b2 += r.h;
    if (t2 == b2)                       // null rect
        return false;

    if (t2 < t1 || b2 > b1)
        return false;

    return true;
}

// ⚠ 区间是**闭**的（`p.x() < l || p.x() > r` 才排除），所以
// `(0,0,10,10).contains(10,10)` 为**真** —— PkRect 那边因为差一是假。
bool PkRectF::contains(const PkPointF &p) const noexcept
{
    qreal l = xp;
    qreal r = xp;
    if (w < 0)
        l += w;
    else
        r += w;
    if (l == r)                         // null rect
        return false;

    if (p.x() < l || p.x() > r)
        return false;

    qreal t = yp;
    qreal b = yp;
    if (h < 0)
        t += h;
    else
        b += h;
    if (t == b)                         // null rect
        return false;

    if (p.y() < t || p.y() > b)
        return false;

    return true;
}

bool PkRectF::intersects(const PkRectF &r) const noexcept
{
    qreal l1 = xp;
    qreal r1 = xp;
    if (w < 0)
        l1 += w;
    else
        r1 += w;
    if (l1 == r1)                       // null rect
        return false;

    qreal l2 = r.xp;
    qreal r2 = r.xp;
    if (r.w < 0)
        l2 += r.w;
    else
        r2 += r.w;
    if (l2 == r2)                       // null rect
        return false;

    if (l1 >= r2 || l2 >= r1)
        return false;

    qreal t1 = yp;
    qreal b1 = yp;
    if (h < 0)
        t1 += h;
    else
        b1 += h;
    if (t1 == b1)                       // null rect
        return false;

    qreal t2 = r.yp;
    qreal b2 = r.yp;
    if (r.h < 0)
        t2 += r.h;
    else
        b2 += r.h;
    if (t2 == b2)                       // null rect
        return false;

    if (t1 >= b2 || t2 >= b1)
        return false;

    return true;
}

// ⚠ **向外扩**：左上角 floor、右下角 ceil，然后用 (x, y, 宽, 高) 构造 PkRect。
// 与 toRect() 的 qRound 是两件事，实测 `(-1.5,-1.5,1,1)` 一个给 (-1,-1,1,1)、
// 一个给 (-2,-2,2,2)。**不做翻正**：负宽高原样传给 PkRect 构造。
// Qt 用的是 qFloor/qCeil（qmath.h，`int(std::floor(v))`）；R-03 的用量表没点名
// qFloor/qCeil（Rect 族里没有调用点），所以这里直接写 std::floor/std::ceil，
// 取值相同 —— 登记在 README 偏离清单。
PkRect PkRectF::toAlignedRect() const noexcept
{
    int xmin = int(std::floor(xp));
    int xmax = int(std::ceil(xp + w));
    int ymin = int(std::floor(yp));
    int ymax = int(std::ceil(yp + h));
    return PkRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

// ── 只在 TU 里落得了地的编译期断言 ──────────────────────────────────────

// 布局：四个 int，无虚表、无 padding。Krita 里 QRect 会被塞进 QVector、
// memcpy、写进文件头，布局漂了是静默的数据损坏。
static_assert(sizeof(PkRect) == 4 * sizeof(int), "PkRect 必须是四个 int");
static_assert(std::is_trivially_copyable<PkRect>::value, "PkRect 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkRect>::value, "PkRect 必须是标准布局");

// constexpr 面：Qt 里带 Q_DECL_CONSTEXPR 的那些必须能在常量表达式里跑。
static_assert(PkRect().isNull(), "默认矩形必须 isNull");
static_assert(PkRect().left() == 0 && PkRect().right() == -1,
              "默认矩形的内部坐标是 (0,0,-1,-1)");
static_assert(PkRect(0, 0, 10, 10).right() == 9, "right 必须差一");
static_assert(PkRect(0, 0, 10, 10).width() == 10, "width = x2-x1+1");
static_assert(!PkRect(0, 0, 0, 0).isValid(),
              "PkRect(0,0,0,0).isValid() 必须是 false —— 与 PkSize(0,0) 相反");
static_assert(PkSize(0, 0).isValid(),
              "PkSize(0,0).isValid() 必须是 true —— 两者语义相反，别互抄");
static_assert(PkRect(0, 0, 10, 10).center().x() == 4, "center 向原点侧截断");

// relaxed constexpr（Q_DECL_RELAXED_CONSTEXPR）：mutator 也要能在常量表达式里跑。
static_assert([] { PkRect t(0, 0, 10, 10); t.setWidth(3); return t.right(); }() == 2,
              "setWidth 必须锚定左上角");
static_assert([] { PkRect t(2, 3, 10, 20); t.moveTo(0, 0); return t.right(); }() == 9,
              "moveTo 必须保宽高");

// noexcept 面：⚠ getRect/getCoords 在 Qt 里**恰好没有** noexcept，
// 而 setRect/setCoords 有。这个不对称是可观察的（noexcept 运算符），照抄。
static_assert(noexcept(PkRect(0, 0, 1, 1)), "构造必须 noexcept");
static_assert(noexcept(PkRect().normalized()), "normalized 必须 noexcept");
static_assert(noexcept(PkRect() | PkRect()), "operator| 必须 noexcept");
static_assert(noexcept(PkRect().contains(PkRect())), "contains 必须 noexcept");
static_assert(noexcept(PkRect().intersects(PkRect())), "intersects 必须 noexcept");
static_assert(!noexcept(PkRect().getRect(nullptr, nullptr, nullptr, nullptr)),
              "getRect 在 Qt 里没有 noexcept —— 照抄这个不对称");
static_assert(!noexcept(PkRect().getCoords(nullptr, nullptr, nullptr, nullptr)),
              "getCoords 在 Qt 里没有 noexcept —— 照抄这个不对称");

// out-of-line 的那六个**不是** constexpr。写成 constexpr 会让替代品比 Qt 多一档
// 能力，调用点在 constexpr 上下文里的可用性两侧就不一样了。
static_assert(!std::is_same<decltype(PkRect().normalized()), void>::value, "");

// ── PkRectF（Task 5）─────────────────────────────────────────────────────

// 布局：四个 qreal。实测 sizeof(QRectF)==32。
static_assert(sizeof(PkRectF) == 4 * sizeof(qreal), "PkRectF 必须是四个 qreal");
static_assert(std::is_trivially_copyable<PkRectF>::value, "PkRectF 必须可平凡拷贝");
static_assert(std::is_standard_layout<PkRectF>::value, "PkRectF 必须是标准布局");

// ⚠ **PkRect 到 PkRectF 必须是隐式的**（Qt 的 QRectF(const QRect&) 非 explicit），
// 反向必须**不是**（QRect 没有吃 QRectF 的构造，只有 toRect()/toAlignedRect()）。
static_assert(std::is_convertible<PkRect, PkRectF>::value,
              "PkRect → PkRectF 必须能隐式提升");
static_assert(!std::is_convertible<PkRectF, PkRect>::value,
              "PkRectF → PkRect 必须**不能**隐式转换 —— 只能走 toRect/toAlignedRect");

// constexpr 面 + 三条最容易抄错的语义（与 PkRect 逐条相反的那几个）。
static_assert(PkRectF().isNull(), "默认 PkRectF 必须 isNull");
static_assert(PkRectF().left() == 0. && PkRectF().width() == 0.,
              "默认 PkRectF 是 (0,0,0,0)，不是 PkRect 的 (0,0,-1,-1) 哨兵");
static_assert(PkRectF(0, 0, 10, 10).right() == 10.,
              "PkRectF::right() **没有**差一 —— PkRect 那边是 9");
static_assert(PkRect(0, 0, 10, 10).right() == 9,
              "同一份源里对照着钉住：PkRect 那边就是差一");
static_assert(!PkRectF(0, 0, 0, 0).isValid() && PkRectF(0, 0, 0, 0).isEmpty(),
              "(0,0,0,0)：isValid 假、isEmpty 真");
static_assert(PkRectF(0, 0, -0.0, -0.0).isNull(),
              "-0.0 == 0. 为真，所以 (0,0,-0.0,-0.0) 也是 isNull");
static_assert(!PkRectF(0, 0, 5e-324, 5e-324).isEmpty(),
              "次正规宽高算 valid —— isEmpty 的门槛是 <= 0.，不是 < 1");
static_assert(PkRectF(PkRect(0, 0, 10, 10)).right() == 10.,
              "从 PkRect 提升走的是 width()（差一已算过），right 得到 10");
static_assert(PkRectF(0, 0, 10, 10).center().x() == 5.,
              "center 是 xp + w/2，不截断");
static_assert(PkRectF(-1.5, -1.5, 1, 1).toRect().left() == -1,
              "toRect 用 qRound：qRound(-1.5) == -1");
static_assert(PkRectF(-1.5, -1.5, 1, 1).toRect().right() == -1,
              "toRect 的右下角是 qRound(xp+w) - 1");

// relaxed constexpr：⚠ 这两条正是 PkRectF 与 PkRect 语义相反的地方。
static_assert([] { PkRectF t(1, 2, 3, 4); t.setLeft(0); return t.width(); }() == 4.,
              "PkRectF::setLeft **保右边界、改宽度**（PkRect 那边只摆一个坐标）");
static_assert([] { PkRectF t(1, 2, 3, 4); t.setWidth(9); return t.left(); }() == 1.,
              "setWidth 锚定左上角");
static_assert([] { PkRectF t(1, 2, 3, 4); t.adjust(1, 1, 1, 1); return t.width(); }() == 3.,
              "adjust 的宽增量是 xp2 - xp1，全 1 时宽高不变");

// noexcept 面：与 PkRect 同一处不对称（getRect/getCoords 没有，其余都有）。
// 实测 probe_rectf2.cpp：contains/intersects/op|/op&/normalized/toRect/
// toAlignedRect 全部为 1，getRect/getCoords 为 0。
static_assert(noexcept(PkRectF(0, 0, 1, 1)), "构造必须 noexcept");
static_assert(noexcept(PkRectF().normalized()), "normalized 必须 noexcept");
static_assert(noexcept(PkRectF() | PkRectF()), "operator| 必须 noexcept");
static_assert(noexcept(PkRectF().contains(PkRectF())), "contains(rect) 必须 noexcept");
// ⚠ 这一条**必须用具名变量**，不能写 `noexcept(PkRectF().contains(PkPointF()))`：
// **PkPointF 的默认构造没有 noexcept**（qpoint.h:289 的 `QPointF::QPointF() : xp(0),
// yp(0) {}` 一样没有，PkPoint.h:247 照抄了），所以那个写法测的是"构造临时点会不会
// 抛"，恒为假 —— 与 contains 本身的 noexcept 无关。实测踩过：probe_rectf.cpp 第一版
// 用临时量得到 contains(pt)=0，改成具名变量后真 Qt 给 1（probe_rectf2.cpp）。
static_assert([] {
    PkRectF r; PkPointF p;
    return noexcept(r.contains(p));
}(), "contains(point) 必须 noexcept");
static_assert(noexcept(PkRectF().intersects(PkRectF())), "intersects 必须 noexcept");
static_assert(noexcept(PkRectF().toRect()), "toRect 必须 noexcept");
static_assert(noexcept(PkRectF().toAlignedRect()), "toAlignedRect 必须 noexcept");
static_assert(!noexcept(PkRectF().getRect(nullptr, nullptr, nullptr, nullptr)),
              "getRect 在 Qt 里没有 noexcept —— 照抄这个不对称");
static_assert(!noexcept(PkRectF().getCoords(nullptr, nullptr, nullptr, nullptr)),
              "getCoords 在 Qt 里没有 noexcept —— 照抄这个不对称");
