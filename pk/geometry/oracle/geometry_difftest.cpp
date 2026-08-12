// geometry_difftest.cpp —— pk/geometry 与真 Qt5 的逐输入对拍。
//
// **这份是 R-03 Task 3..6（Size / Rect / RectF / Transform）要往里加节的骨架。**
// 抄自 `.exec/replacement/R-01.difftest.cpp`（QString ↔ PkString）。
// 骨架的六件事不要改，各族只往「逐 API 对拍」与「输入集」两节里加东西。
//
// ── 输出契约（run_oracle.sh 读这两种行，别的都是给人看的）────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行
// **退出码必须是 0，即使 M>0** —— 已声明的偏离不算失败，退出码只表示"跑完没崩"。
//
// ── 为什么替代品要塞进 namespace pkoracle ──────────────────────────────
// PkGlobal.h 与 Qt 的 qglobal.h 在**同一个全局作用域**里定义了签名完全相同的
// qAbs / qRound / qMin / qMax / qBound / qFuzzyCompare / qFuzzyIsNull —— 两个头
// 直接进同一个 TU 是硬性的重定义错误，对拍根本编不出来。
// 解法：`namespace pkoracle { #include "PkPoint.h" }`。PkGlobal.h 与 PkPoint.h
// **一个 #include 都没有**（只互相包），所以整包干净地落进 pkoracle::，
// 里面的 qRound / qAbs 仍然是替代品自己那份实现，不是 Qt 的。
//   · 若哪天几何头开始 #include <cstring> 之类，必须把那个 include 提到本文件
//     顶部的系统头区，不能让它落进 pkoracle::（那会造出 pkoracle::std）。
//   · 这个写法顺带把「compat 垫片被拉进 -I」查得比 static_assert 还死：垫片一旦
//     命中，`#include <QPoint>` 会先把 PkPoint.h 的 include guard 点掉，
//     namespace 里那次 include 就整个空转，pkoracle::PkPointF 根本不存在 → 编译失败。
//     下面的 #error 与 static_assert 是同一件事的另外两道，三道都留着，成本为零。
//
// ── 跑出 mismatch=0 是警报，不是好消息 ─────────────────────────────────
// 但 R-03 与 R-01 有一处根本不同：PkString 是重写，**PkPoint 是逐字照抄 qpoint.h**，
// 所以"真实差异 = 0"才是正确结果，geometry.deviation 里也就没有一条真实偏离。
// 于是判别力不能再靠"看到差异"来证明，本文件用两件事顶上：
//   ① **canary**：三条故意不相等的比对，走的是与真实 API 完全相同的 rec()/比较
//      原语/tag 构造路径。它们必须出现在输出里 —— 一旦少一条，说明比较管道
//      （bit 比较、计数、tag）被写死或被优化没了，run_oracle.sh 直接 FAIL。
//   ② **注入自证**：往 PkPoint 里塞真 bug，必须产生**未声明**的 tag（报告里有三组）。
//
// ── tag 的两条硬规则（违反了整件事白做，理由见 oracle/README 方法论）────
//   规则一：tag 必须由「触发这次差异的**输入形态**」参与构造，不能是「每个 API
//           一个字面量常量」。R-01 第一版那么写，注入三组真 bug 全部绿灯通过。
//   规则二：tag 的判定谓词**不能比 .deviation 里那句理由宽**。理由说「两侧都失败」，
//           谓词里就必须出现 !ok；理由说「退化矩形」，谓词就不能是「任何矩形」。

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QPoint>
#include <QString>
#include <QtGlobal>
#include <QMessageLogContext>

#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

// 垫片一旦混进 -I，<QPoint> 会解析到 compat/QPoint，而那份是两个 #define。
// 两侧于是解析成同一个类型，跑出来必然零差异且看不出破绽。成本三行。
#if defined(QPoint) || defined(QPointF)
#  error "对拍两侧解析成了同一个类型 —— -I 里混进了 pk/geometry/compat"
#endif

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkPoint.h"
}

using PkPoint  = pkoracle::PkPoint;
using PkPointF = pkoracle::PkPointF;

static_assert(!std::is_same<QPoint,  PkPoint >::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QPointF, PkPointF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QPointF) == sizeof(PkPointF), "两侧布局不一致");

// ═══ 计数与记录 ════════════════════════════════════════════════════════════

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;   // "<api> <tag>" -> count
static long g_printed = 0;

static void rec(const char *api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    if (same) return;
    ++g_mismatch;
    ++g_tags[std::string(api) + " " + tag];
    if (g_printed < 40) {           // 明细只打前 40 条，run_oracle.sh 不读它
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api, tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

// ═══ 比较原语 ══════════════════════════════════════════════════════════════

// **位精确**比较 double：`==` 会把 +0/-0 判等、把 NaN 判不等，两者都不是我们要的。
// 几何类型里零号的符号位是真的会传播出去的（一元 +/-、manhattanLength、
// transposed 都保号），用 `==` 比等于把这一整类差异永久豁免。
static bool same_double(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    // NaN 的位模式两侧可以不同（载荷不保证），都是 NaN 就算同
    return (a != a) && (b != b);
}

static std::string dstr(double d)
{
    std::uint64_t b; std::memcpy(&b, &d, sizeof b);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g(0x%016llx)", d, (unsigned long long)b);
    return buf;
}
static std::string istr(long long v) { return std::to_string(v); }
static std::string bstr(bool b) { return b ? "true" : "false"; }

static std::string qstr(const QPoint &p)   { return "(" + istr(p.x()) + "," + istr(p.y()) + ")"; }
static std::string qstr(const PkPoint &p)  { return "(" + istr(p.x()) + "," + istr(p.y()) + ")"; }
static std::string qstr(const QPointF &p)  { return "(" + dstr(p.x()) + "," + dstr(p.y()) + ")"; }
static std::string qstr(const PkPointF &p) { return "(" + dstr(p.x()) + "," + dstr(p.y()) + ")"; }

static bool same_pt(const QPoint &q, const PkPoint &p)
{ return q.x() == p.x() && q.y() == p.y(); }
static bool same_ptf(const QPointF &q, const PkPointF &p)
{ return same_double(q.x(), p.x()) && same_double(q.y(), p.y()); }

// ═══ tag 谓词：**全部由输入的形态算出来**，没有一个是字面量常量 ═══════════

static bool nonFinite(double d) { return std::isinf(d) || std::isnan(d); }
static bool signedZero(double d) { return d == 0.0 && std::signbit(d); }
static bool subnormal(double d)
{ return d != 0.0 && !nonFinite(d) && std::fabs(d) < DBL_MIN; }
// 落在 int 值域之外 → int(d) 是 UB，两侧靠同一条指令给同一个答案
static bool outOfIntRange(double d)
{ return nonFinite(d) || !(d >= -2147483648.0 && d <= 2147483647.0); }
// 小数部分恰好是 ±0.5 —— qRound 的取整方向在这里才有分歧（负半值向 +∞）
static bool halfBoundary(double d)
{
    if (nonFinite(d) || std::fabs(d) > 1e15) return false;
    const double f = std::fabs(d - std::floor(d));
    return f == 0.5;
}
// int(d + 0.5) 的经典边界：比 0.5 小一个 ulp 却因为加法舍入而进位
static bool nearHalfUlp(double d)
{
    if (nonFinite(d)) return false;
    const double f = std::fabs(d - std::floor(d));
    return f != 0.5 && std::fabs(f - 0.5) < 1e-15;
}
static bool intExtremum(int v) { return v == INT_MIN || v == INT_MAX; }
static bool addOverflows(int a, int b)
{ long long s = (long long)a + b; return s < INT_MIN || s > INT_MAX; }
static bool subOverflows(int a, int b)
{ long long s = (long long)a - b; return s < INT_MIN || s > INT_MAX; }
static bool mulOverflows(int a, int b)
{ long long s = (long long)a * b; return s < INT_MIN || s > INT_MAX; }
// |x| 本身就溢出（qAbs(INT_MIN) 回绕），或者两个绝对值相加溢出
static bool manhattanOverflows(int x, int y)
{
    if (x == INT_MIN || y == INT_MIN) return true;
    long long s = (long long)std::llabs((long long)x) + std::llabs((long long)y);
    return s > INT_MAX;
}

// 两个 double 里挑最"特殊"的那种形态命名 —— 优先级从最能解释差异的往下排
static std::string shapeOfD(double a, double b)
{
    if (nonFinite(a) || nonFinite(b)) return "nonfinite";
    if (signedZero(a) || signedZero(b)) return "signed-zero";
    if (subnormal(a) || subnormal(b)) return "subnormal";
    if (a == 0.0 || b == 0.0) return "zero";
    if (std::fabs(a) > 1e300 || std::fabs(b) > 1e300) return "huge";
    return "finite";
}
static std::string shapeOfD(double a) { return shapeOfD(a, a); }

static std::string shapeOfI(int a, int b)
{
    if (intExtremum(a) || intExtremum(b)) return "int-extremum";
    if (a == 0 && b == 0) return "origin";
    return "ordinary";
}

// ═══ Point 族：逐 API 对拍 ═════════════════════════════════════════════════
//
// 每个 rec() 的 tag 都由**这一次的实参**算出来。加新 API 时照这个形状写：
// 先想清楚"这个 API 会在什么输入形态上跟 Qt 分家"，把那个形态做成谓词，
// 让它参与 tag；想不出来就写 shapeOfD/shapeOfI 那种通用形态分流 —— 但**绝不能**
// 写成一个与输入无关的常量（那等于把这个 API 整个退出对拍）。

static void cmp_point_unary(int x, int y)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")";
    const std::string sh = shapeOfI(x, y);

    rec("x", q.x() == p.x(), sh, in, istr(q.x()), istr(p.x()));
    rec("y", q.y() == p.y(), sh, in, istr(q.y()), istr(p.y()));
    rec("isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));
    rec("transposed", same_pt(q.transposed(), p.transposed()), sh, in,
        qstr(q.transposed()), qstr(p.transposed()));

    // manhattanLength 唯一会分家的形态是溢出（qAbs(INT_MIN) 回绕、两绝对值相加溢出）
    rec("manhattanLength", q.manhattanLength() == p.manhattanLength(),
        manhattanOverflows(x, y) ? "int-overflow" : "in-range",
        in, istr(q.manhattanLength()), istr(p.manhattanLength()));

    rec("operator-unary", same_pt(-q, -p),
        (x == INT_MIN || y == INT_MIN) ? "int-min-negation" : sh,
        in, qstr(-q), qstr(-p));
    rec("operator+unary", same_pt(+q, +p), sh, in, qstr(+q), qstr(+p));

    // setX/setY 与 rx()/ry()：出参一起比（rx 返回的是**引用**，改了要看得见）
    { QPoint q2 = q; PkPoint p2 = p; q2.setX(y); p2.setX(y); q2.setY(x); p2.setY(x);
      rec("setX/setY", same_pt(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPoint q2 = q; PkPoint p2 = p; q2.rx() += 1; p2.rx() += 1; q2.ry() -= 1; p2.ry() -= 1;
      rec("rx/ry", same_pt(q2, p2),
          (x == INT_MAX || y == INT_MIN) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }
}

static void cmp_point_binary(int ax, int ay, int bx, int by)
{
    const QPoint  qa(ax, ay), qb(bx, by);
    const PkPoint pa(ax, ay), pb(bx, by);
    const std::string in = "(" + istr(ax) + "," + istr(ay) + ")|("
                         + istr(bx) + "," + istr(by) + ")";

    rec("operator+", same_pt(qa + qb, pa + pb),
        (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow"
                                                       : shapeOfI(ax, bx),
        in, qstr(qa + qb), qstr(pa + pb));
    rec("operator-", same_pt(qa - qb, pa - pb),
        (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow"
                                                       : shapeOfI(ax, bx),
        in, qstr(qa - qb), qstr(pa - pb));
    rec("operator==", (qa == qb) == (pa == pb), shapeOfI(ax, bx),
        in, bstr(qa == qb), bstr(pa == pb));
    rec("operator!=", (qa != qb) == (pa != pb), shapeOfI(ax, bx),
        in, bstr(qa != qb), bstr(pa != pb));
    { QPoint q2 = qa; PkPoint p2 = pa; q2 += qb; p2 += pb;
      rec("operator+=", same_pt(q2, p2),
          (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow" : shapeOfI(ax, bx),
          in, qstr(q2), qstr(p2)); }
    { QPoint q2 = qa; PkPoint p2 = pa; q2 -= qb; p2 -= pb;
      rec("operator-=", same_pt(q2, p2),
          (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow" : shapeOfI(ax, bx),
          in, qstr(q2), qstr(p2)); }

    // dotProduct：静态成员，不防溢出（乘法与加法都可能溢出）
    rec("dotProduct", QPoint::dotProduct(qa, qb) == PkPoint::dotProduct(pa, pb),
        (mulOverflows(ax, bx) || mulOverflows(ay, by)
         || addOverflows((int)((long long)ax * bx), (int)((long long)ay * by)))
            ? "int-overflow" : shapeOfI(ax, bx),
        in, istr(QPoint::dotProduct(qa, qb)), istr(PkPoint::dotProduct(pa, pb)));
}

// QPoint 的缩放：**三个重载走三条不同的路**（float 与 double 用不同精度做
// `v*f + 0.5`，int 根本不取整），所以三条都要单独对拍。
static void cmp_point_scale_double(int x, int y, double f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*" + dstr(f);
    // 分歧只可能出在"乘出来的实数落在哪"：半值边界（取整方向）、越出 int 值域
    //（int(d) 是 UB）、非有限。其余落 in-range。
    const double px = (double)x * f, py = (double)y * f;
    const char *tag = (nonFinite(px) || nonFinite(py))            ? "nonfinite-product"
                    : (outOfIntRange(px) || outOfIntRange(py))    ? "product-out-of-int-range"
                    : (halfBoundary(px) || halfBoundary(py))      ? "half-boundary"
                    : (nearHalfUlp(px) || nearHalfUlp(py))        ? "near-half-ulp"
                                                                  : "in-range";
    rec("operator*(double)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(double,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(double)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }

    const double dx = (double)x / f, dy = (double)y / f;
    const char *dtag = (f == 0.0)                                 ? "divisor-zero"
                     : (nonFinite(dx) || nonFinite(dy))           ? "nonfinite-quotient"
                     : (outOfIntRange(dx) || outOfIntRange(dy))   ? "quotient-out-of-int-range"
                     : (halfBoundary(dx) || halfBoundary(dy))     ? "half-boundary"
                     : (nearHalfUlp(dx) || nearHalfUlp(dy))       ? "near-half-ulp"
                                                                  : "in-range";
    rec("operator/", same_pt(q / f, p / f), dtag, in, qstr(q / f), qstr(p / f));
    { QPoint q2 = q; PkPoint p2 = p; q2 /= f; p2 /= f;
      rec("operator/=", same_pt(q2, p2), dtag, in, qstr(q2), qstr(p2)); }
}

static void cmp_point_scale_float(int x, int y, float f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*f" + dstr(f);
    // ⚠ 按 **float** 精度算，不是 double —— 这正是两个重载存在的理由
    const float px = (float)x * f, py = (float)y * f;
    const char *tag = (nonFinite(px) || nonFinite(py))         ? "nonfinite-product"
                    : (outOfIntRange(px) || outOfIntRange(py)) ? "product-out-of-int-range"
                    : (halfBoundary(px) || halfBoundary(py))   ? "half-boundary"
                                                               : "in-range";
    rec("operator*(float)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(float,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(float)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_point_scale_int(int x, int y, int f)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")*i" + istr(f);
    const char *tag = (mulOverflows(x, f) || mulOverflows(y, f)) ? "int-overflow" : "in-range";
    rec("operator*(int)", same_pt(q * f, p * f), tag, in, qstr(q * f), qstr(p * f));
    rec("operator*(int,rev)", same_pt(f * q, f * p), tag, in, qstr(f * q), qstr(f * p));
    { QPoint q2 = q; PkPoint p2 = p; q2 *= f; p2 *= f;
      rec("operator*=(int)", same_pt(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_pointf_unary(double x, double y)
{
    const QPointF  q(x, y);
    const PkPointF p(x, y);
    const std::string in = "(" + dstr(x) + "," + dstr(y) + ")";
    const std::string sh = shapeOfD(x, y);

    rec("F::x", same_double(q.x(), p.x()), sh, in, dstr(q.x()), dstr(p.x()));
    rec("F::y", same_double(q.y(), p.y()), sh, in, dstr(q.y()), dstr(p.y()));

    // isNull 用 qIsNull（d == 0.0）：-0.0 算 null、次正规数不算。零号与次正规
    // 正是这条唯一会分家的地方，所以让它们参与 tag。
    rec("F::isNull", q.isNull() == p.isNull(),
        (signedZero(x) || signedZero(y)) ? "signed-zero"
        : (subnormal(x) || subnormal(y)) ? "subnormal" : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));

    rec("F::transposed", same_ptf(q.transposed(), p.transposed()), sh, in,
        qstr(q.transposed()), qstr(p.transposed()));
    rec("F::manhattanLength", same_double(q.manhattanLength(), p.manhattanLength()),
        sh, in, dstr(q.manhattanLength()), dstr(p.manhattanLength()));

    rec("F::operator-unary", same_ptf(-q, -p), sh, in, qstr(-q), qstr(-p));
    rec("F::operator+unary", same_ptf(+q, +p), sh, in, qstr(+q), qstr(+p));

    // toPoint 走 qRound。分歧形态：半值边界（取整方向）、越出 int 值域（UB）、
    // 差一个 ulp 的半值（int(d+0.5) 的进位）。
    rec("F::toPoint", same_pt(q.toPoint(), p.toPoint()),
        (outOfIntRange(x) || outOfIntRange(y)) ? "out-of-int-range"
        : (halfBoundary(x) || halfBoundary(y)) ? "half-boundary"
        : (nearHalfUlp(x) || nearHalfUlp(y))   ? "near-half-ulp"
        : (subnormal(x) || subnormal(y))       ? "subnormal" : "in-range",
        in, qstr(q.toPoint()), qstr(p.toPoint()));

    { QPointF q2 = q; PkPointF p2 = p; q2.setX(y); p2.setX(y); q2.setY(x); p2.setY(x);
      rec("F::setX/setY", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = q; PkPointF p2 = p; q2.rx() += 1.0; p2.rx() += 1.0;
      q2.ry() -= 1.0; p2.ry() -= 1.0;
      rec("F::rx/ry", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_pointf_binary(double ax, double ay, double bx, double by)
{
    const QPointF  qa(ax, ay), qb(bx, by);
    const PkPointF pa(ax, ay), pb(bx, by);
    const std::string in = "(" + dstr(ax) + "," + dstr(ay) + ")|("
                         + dstr(bx) + "," + dstr(by) + ")";
    const std::string sh = shapeOfD(ax, bx);

    rec("F::operator+", same_ptf(qa + qb, pa + pb), sh, in, qstr(qa + qb), qstr(pa + pb));
    rec("F::operator-", same_ptf(qa - qb, pa - pb), sh, in, qstr(qa - qb), qstr(pa - pb));

    // ⚠ QPointF::operator== 是**模糊比较**，逐分量二选一：任一侧恰好是 0 走
    // 绝对阈值 fuzzyIsNull，否则走相对阈值 fuzzyCompare。两条分支上的分歧根因
    // 完全不同，所以走了哪条分支必须参与 tag —— 否则一条分支的偏离会把另一条
    // 一起罩住（规则二踩过的坑）。
    const bool zeroBranchX = (ax == 0.0 || bx == 0.0);
    const bool zeroBranchY = (ay == 0.0 || by == 0.0);
    const std::string eqtag =
        (nonFinite(ax) || nonFinite(bx) || nonFinite(ay) || nonFinite(by)) ? "nonfinite"
      : (zeroBranchX && zeroBranchY)                                       ? "zero-branch-both"
      : (zeroBranchX || zeroBranchY)                                       ? "zero-branch-mixed"
      : (subnormal(ax) || subnormal(bx) || subnormal(ay) || subnormal(by)) ? "subnormal"
                                                                           : "relative-branch";
    rec("F::operator==", (qa == qb) == (pa == pb), eqtag, in, bstr(qa == qb), bstr(pa == pb));
    rec("F::operator!=", (qa != qb) == (pa != pb), eqtag, in, bstr(qa != qb), bstr(pa != pb));

    { QPointF q2 = qa; PkPointF p2 = pa; q2 += qb; p2 += pb;
      rec("F::operator+=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = qa; PkPointF p2 = pa; q2 -= qb; p2 -= pb;
      rec("F::operator-=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    rec("F::dotProduct",
        same_double(QPointF::dotProduct(qa, qb), PkPointF::dotProduct(pa, pb)),
        (nonFinite(ax) || nonFinite(ay) || nonFinite(bx) || nonFinite(by)) ? "nonfinite"
        : (signedZero(ax) || signedZero(ay) || signedZero(bx) || signedZero(by)) ? "signed-zero"
        : (subnormal(ax) || subnormal(ay) || subnormal(bx) || subnormal(by)) ? "subnormal"
        : (std::fabs(ax) > 1e150 || std::fabs(bx) > 1e150) ? "overflow-prone" : "finite",
        in, dstr(QPointF::dotProduct(qa, qb)), dstr(PkPointF::dotProduct(pa, pb)));
}

static void cmp_pointf_scale(double x, double y, double c)
{
    const QPointF  q(x, y);
    const PkPointF p(x, y);
    const std::string in = "(" + dstr(x) + "," + dstr(y) + ")*" + dstr(c);
    const std::string sh = shapeOfD(x, c);

    rec("F::operator*", same_ptf(q * c, p * c), sh, in, qstr(q * c), qstr(p * c));
    rec("F::operator*(rev)", same_ptf(c * q, c * p), sh, in, qstr(c * q), qstr(c * p));
    rec("F::operator/", same_ptf(q / c, p / c),
        (c == 0.0) ? "divisor-zero" : sh, in, qstr(q / c), qstr(p / c));
    { QPointF q2 = q; PkPointF p2 = p; q2 *= c; p2 *= c;
      rec("F::operator*=", same_ptf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QPointF q2 = q; PkPointF p2 = p; q2 /= c; p2 /= c;
      rec("F::operator/=", same_ptf(q2, p2), (c == 0.0) ? "divisor-zero" : sh,
          in, qstr(q2), qstr(p2)); }
}

// PkPoint → PkPointF 的隐式提升（Krita 里大量调用点靠这条），以及混合运算。
static void cmp_promotion(int x, int y, double cx, double cy)
{
    const QPoint  qi(x, y);
    const PkPoint pi(x, y);
    const QPointF  qf = qi;      // 隐式
    const PkPointF pf = pi;
    const std::string in = "(" + istr(x) + "," + istr(y) + ")+("
                         + dstr(cx) + "," + dstr(cy) + ")";
    const std::string sh = shapeOfI(x, y);
    rec("F::fromPoint", same_ptf(qf, pf), sh, in, qstr(qf), qstr(pf));
    rec("F::mixedAdd", same_ptf(QPointF(cx, cy) + qi, PkPointF(cx, cy) + pi),
        shapeOfD(cx, cy), in, qstr(QPointF(cx, cy) + qi), qstr(PkPointF(cx, cy) + pi));
    // 往返：提升再 toPoint 必须回到原点（除非 int 值域边界上的 double 表示丢精度）
    rec("F::roundTrip", same_pt(qf.toPoint(), pf.toPoint()), sh, in,
        qstr(qf.toPoint()), qstr(pf.toPoint()));
}

// ═══ canary：证明比较管道是活的 ════════════════════════════════════════════
//
// 走的是与真实 API 完全相同的 rec() 与比较原语。三条都必须出现在 DIFFTAG 里，
// 少一条就说明对应的那段管道死了：
//   int-value               —— 整数比较与计数还活着
//   double-bits-signed-zero —— same_double 真的是**位**比较，没退化成 `==`
//                              （`==` 会把 +0.0 与 -0.0 判等，这条就消失）
//   nan-vs-number           —— NaN 侧的判等分支没有把一切都判成"同"
static void run_canaries()
{
    const double nan_ = std::nan("");
    rec("canary", same_pt(QPoint(1, 2), PkPoint(1, 3)), "int-value",
        "deliberate", qstr(QPoint(1, 2)), qstr(PkPoint(1, 3)));
    rec("canary", same_double(0.0, -0.0), "double-bits-signed-zero",
        "deliberate", dstr(0.0), dstr(-0.0));
    rec("canary", same_double(nan_, 1.0), "nan-vs-number",
        "deliberate", dstr(nan_), dstr(1.0));
    // 反向自证：这两条**必须**判成"同"，否则 same_double 过严（NaN 两侧判不等
    // 会让整份对拍在任何含 NaN 的输入上刷假差异）。它们不产生 tag。
    rec("canary-negative", same_double(nan_, nan_), "nan-vs-nan",
        "deliberate", dstr(nan_), dstr(nan_));
    rec("canary-negative", same_double(1.5, 1.5), "identical",
        "deliberate", dstr(1.5), dstr(1.5));
}

// ═══ 输入集 ════════════════════════════════════════════════════════════════

// ── Group 1：手挑对抗用例（钉住已知形态；覆盖度的主力是 Group 2）──────────
static const double kHandD[] = {
    // 零号两侧
    0.0, -0.0,
    // qRound 的取整方向：半值
    0.5, -0.5, 1.5, -1.5, 2.5, -2.5, 3.5, -3.5,
    // int(d+0.5) 的经典边界
    0.49999999999999994, -0.49999999999999994, 0.5000000000000001,
    // fuzzy 阈值两侧
    1e-12, -1e-12, 1e-13, 1e-11, 1e-300, 2e-12,
    // 次正规 / 极小 / 极大
    5e-324, -5e-324, 1e-323, 2.2250738585072014e-308, 1e308, -1e308, 1.7976931348623157e308,
    // 特值
    INFINITY, -INFINITY, NAN,
    // int 值域边界（toPoint 的 UB 分界）
    2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 4294967296.0,
    // 普通值
    1.0, -1.0, 2.0, -2.0, 0.25, -0.25, 3.0, -3.0, 1.0 + 1e-13, 1.0 + 1e-11,
};
static const int kHandI[] = {
    0, 1, -1, 2, -2, 3, -3, 5, -5, 7, -7,
    46340, -46340, 46341, -46341,          // 平方接近 INT_MAX 的分界
    1000000, -1000000, 65536, -65536,
    INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1, INT_MAX / 2, INT_MIN / 2,
};

// ── Group 2：token 全组合（覆盖度的主力）──────────────────────────────────
// Point 是二元的：21 个标量 token → 21² = 441 个点 → 两点组合 441² = 194 481 次
// 比对/API。带标量参数的 API 做三层（点的两层 + 参数一层）：四层再乘参数会到
// 千万量级、跑几分钟（R-01 踩过）。
static const double kTokD[] = {
    0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.5, -2.5, 3.0, -3.0,
    0.49999999999999994, 1e-12, 1e-13, 1e-300, 5e-324, 1e308, -1e308,
    INFINITY, NAN,
};
static const int kTokI[] = {
    0, 1, -1, 2, -2, 3, -3, 5, -5, 7, -7, 46341, -46341,
    1000000, -1000000, 65536, INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1,
};
// 缩放参数（第三层）
static const double kFacD[] = {
    0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.0, -2.0, 3.0, 0.25,
    0.49999999999999994, 1e-300, 1e308, INFINITY, -INFINITY, NAN,
    1.5, -1.5, 2147483648.0, 1e-12,
};
static const float kFacF[] = {
    0.0f, -0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 3.0f, 0.25f,
    0.49999997f,                       // ⚠ 按 float 精度会进位，提升到 double 不会
    1e-30f, 1e30f, INFINITY, -INFINITY, NAN, 1.5f, -1.5f, 2147483648.0f, 1e-12f,
};
static const int kFacI[] = { 0, 1, -1, 2, -2, 3, 65536, -65536, INT_MAX, INT_MIN };

template <typename T, std::size_t N>
static constexpr int countOf(const T (&)[N]) { return (int)N; }

int main()
{
    // 把 Qt 的运行期警告吞掉：本对拍**故意**大量喂退化输入，Qt 在某些路径上会
    // 往 stderr 刷警告。run_oracle.sh 用 2>&1 收输出，不吞的话失败时那几行
    // tail 只剩噪音。抄这份骨架时保留这一段（Rect/Transform 那几节真会触发）。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});

    run_canaries();

    const int nHandD = countOf(kHandD), nHandI = countOf(kHandI);
    const int nTokD = countOf(kTokD), nTokI = countOf(kTokI);
    const int nFacD = countOf(kFacD), nFacF = countOf(kFacF), nFacI = countOf(kFacI);

    // ── Group 1：手挑用例，两两配对喂给二元 API ──
    for (int i = 0; i < nHandD; ++i) {
        const double x = kHandD[i], y = kHandD[(i + 1) % nHandD];
        const double bx = kHandD[(i + 3) % nHandD], by = kHandD[(i + 7) % nHandD];
        cmp_pointf_unary(x, y);
        cmp_pointf_binary(x, y, bx, by);
        for (int f = 0; f < nFacD; ++f) cmp_pointf_scale(x, y, kFacD[f]);
    }
    for (int i = 0; i < nHandI; ++i) {
        const int x = kHandI[i], y = kHandI[(i + 1) % nHandI];
        cmp_point_unary(x, y);
        cmp_point_binary(x, y, kHandI[(i + 3) % nHandI], kHandI[(i + 7) % nHandI]);
        for (int f = 0; f < nFacD; ++f) cmp_point_scale_double(x, y, kFacD[f]);
        for (int f = 0; f < nFacF; ++f) cmp_point_scale_float(x, y, kFacF[f]);
        for (int f = 0; f < nFacI; ++f) cmp_point_scale_int(x, y, kFacI[f]);
        for (int d = 0; d < nHandD; ++d)
            cmp_promotion(x, y, kHandD[d], kHandD[(d + 5) % nHandD]);
    }

    // ── Group 2a：一元 API 走 token² 个点 ──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            cmp_pointf_unary(kTokD[a], kTokD[b]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            cmp_point_unary(kTokI[a], kTokI[b]);

    // ── Group 2b：二元 API 走 (token²)² 组两点组合 ──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int c = 0; c < nTokD; ++c)
                for (int d = 0; d < nTokD; ++d)
                    cmp_pointf_binary(kTokD[a], kTokD[b], kTokD[c], kTokD[d]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int c = 0; c < nTokI; ++c)
                for (int d = 0; d < nTokI; ++d)
                    cmp_point_binary(kTokI[a], kTokI[b], kTokI[c], kTokI[d]);

    // ── Group 2c：带标量参数的 API 做三层（点两层 + 参数一层）──
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_pointf_scale(kTokD[a], kTokD[b], kFacD[f]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b) {
            for (int f = 0; f < nFacD; ++f) cmp_point_scale_double(kTokI[a], kTokI[b], kFacD[f]);
            for (int f = 0; f < nFacF; ++f) cmp_point_scale_float(kTokI[a], kTokI[b], kFacF[f]);
            for (int f = 0; f < nFacI; ++f) cmp_point_scale_int(kTokI[a], kTokI[b], kFacI[f]);
        }

    // ── Group 2d：跨类型（提升 + 混合运算）三层 ──
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int d = 0; d < nTokD; ++d)
                cmp_promotion(kTokI[a], kTokI[b], kTokD[d], kTokD[(d + 3) % nTokD]);

    for (const auto &kv : g_tags)
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 已声明的偏离不算失败；判定在 run_oracle.sh
}
