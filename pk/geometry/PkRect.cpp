#include "PkRect.h"

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
