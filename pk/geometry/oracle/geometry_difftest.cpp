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
//   规则三：**每一个已实现的重载都要有自己的 rec()**，一个都不许合并、一个都不许漏。
//           Task 3 复评实测：cmp_sizef_scaled 少写了 `SF::scale(w,h)` 那一条 rec，
//           于是把 PkSizeF::scale(qreal,qreal,mode) 整个改坏（永远走
//           IgnoreAspectRatio）之后，**93 630 039 次比对一条都没红、run_oracle.sh
//           退出码 0 放行**。转发链「A 转发到 B、B 有 rec」不算覆盖 —— 坏掉的正
//           可能是 A 那一跳。合并也不行（`setWidth/setHeight` 挤一条 rec 时，
//           tag 说不清是哪个重载分的家）。
//           **落地方式**：每族写完对拍后列一张「已实现重载 vs 对应 rec()」对照表
//           （逐条对着头文件的声明数，不是对着记忆数），有一格空的就是漏了。
//           Task 3 的那张表在 <scratchpad>/R-03/sdd/task-3-report.md「修复轮 1」。
//           **已知残留（Task 2 的 Point 族，有意不动）**：`setX/setY`、`rx/ry`、
//           `F::setX/setY`、`F::rx/ry` 四条仍是两个重载挤一条 rec。它们不是覆盖
//           漏洞（两个 mutator 在同一次往返里都被调到，任一个坏掉 same_pt 就分家），
//           只是归因粗；不拆是为了让「Point 族 total=35 569 662」这条 Task 2 基线
//           保持逐字不变，好继续当"我没动到 Point"的自证。**新写的族一律按规则三
//           拆到底**（Size 族已拆）。

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QPoint>
#include <QSize>
#include <QRect>
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
#include <set>
#include <string>
#include <type_traits>
#include <vector>

// 垫片一旦混进 -I，<QPoint> 会解析到 compat/QPoint，而那份是两个 #define。
// 两侧于是解析成同一个类型，跑出来必然零差异且看不出破绽。成本三行。
#if defined(QPoint) || defined(QPointF) || defined(QSize) || defined(QSizeF) \
    || defined(QRect) || defined(QRectF)
#  error "对拍两侧解析成了同一个类型 —— -I 里混进了 pk/geometry/compat"
#endif

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkPoint.h"
#include "PkSize.h"
// ⚠ **PkSize.cpp 也要进来**：两个 scaled(const Pk*&, mode) 是 out-of-line 的
//（照 Qt 的形态，QSize::scaled 定义在 qsize.cpp 里）。libpkgeometry.a 里那份
// 定义的是 `::PkSize::scaled`，而本 TU 需要的是 `pkoracle::PkSize::scaled`
// —— 两个不同的符号，链不上（第一版就是这么炸的）。
// 纪律与 namespace 那条完全一样：**PkSize.cpp 里的系统头必须在上面的系统头区
// 里已经出现过**（它只 #include <type_traits>，上面有），否则会造出 pkoracle::std。
// 顺带好处：PkSize.cpp 里那批 static_assert 也在这个 TU 里编一遍。
#include "PkSize.cpp"
#include "PkRect.h"
// ⚠ 同理 **PkRect.cpp 也要进来**：normalized / operator| / operator& /
// contains ×2 / intersects 六个是 out-of-line 的（照 Qt 的形态，实现在 qrect.cpp
// 里）。libpkgeometry.a 里那份定义的是 `::PkRect::normalized`，本 TU 需要的是
// `pkoracle::PkRect::normalized` —— 两个不同的符号，链不上。
// 纪律同上：PkRect.cpp 只 #include <type_traits>（上面的系统头区已有）。
#include "PkRect.cpp"
}

using PkPoint  = pkoracle::PkPoint;
using PkPointF = pkoracle::PkPointF;
using PkSize   = pkoracle::PkSize;
using PkSizeF  = pkoracle::PkSizeF;
using PkRect   = pkoracle::PkRect;

static_assert(!std::is_same<QPoint,  PkPoint >::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QPointF, PkPointF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QPointF) == sizeof(PkPointF), "两侧布局不一致");
static_assert(!std::is_same<QSize,  PkSize >::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QSizeF, PkSizeF>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QSizeF) == sizeof(PkSizeF), "两侧布局不一致");
// ⚠ 枚举也必须是两个不同的类型（替代品那份落在 pkoracle::Qt 里）。若哪天
// 替代品改成 `using Qt::AspectRatioMode = ::Qt::AspectRatioMode` 之类的转发，
// 下面这条会立刻炸 —— 那种写法等于把对拍的 mode 一侧接到真 Qt 上，白比。
static_assert(!std::is_same<Qt::AspectRatioMode,
                            pkoracle::Qt::AspectRatioMode>::value,
              "AspectRatioMode 两侧解析成了同一个类型");
static_assert((int)Qt::IgnoreAspectRatio == (int)pkoracle::Qt::IgnoreAspectRatio
              && (int)Qt::KeepAspectRatio == (int)pkoracle::Qt::KeepAspectRatio
              && (int)Qt::KeepAspectRatioByExpanding
                     == (int)pkoracle::Qt::KeepAspectRatioByExpanding,
              "AspectRatioMode 的枚举取值两侧不一致");
static_assert(!std::is_same<QRect, PkRect>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(sizeof(QRect) == sizeof(PkRect), "两侧布局不一致");

// ═══ 计数与记录 ════════════════════════════════════════════════════════════

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;   // "<api> <tag>" -> count
static long g_printed = 0;

// 规则三的**机器闸门**（Task 4 新增）。规则三是「每个已实现的重载都要有自己的
// rec()」，Task 3 之前只能靠人手列对照表 —— 而 Task 3 复评实测过：漏一个重载，
// 93 630 039 次比对一条都不红、run_oracle.sh exit=0 放行。
//
// 做法：rec() 顺手把见过的 api 名收进这个集合，程序末尾按 `APISEEN <name>`
// 逐行打出来；仓库里放一份人维护的期望清单（oracle/api_seen.expected 与
// oracle/rect_api.map），run_oracle.sh 做三向核对：
//   ① APISEEN 集合 == api_seen.expected（多一条少一条都 FAIL）
//   ② PkRect.h 类体里的每一条声明都在 rect_api.map 里有一行（**加了重载却
//      没写 rec() 会在这里被抓**——这正是规则三原来漏掉的那一半）
//   ③ rect_api.map 里的每个标签都真的出现在 APISEEN 里
// 多打的 APISEEN 行不影响既有 stdout 契约：契约只规定 DIFF/DIFFTAG 两种行有意义。
static std::set<std::string> g_apis;

static void rec(const char *api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    g_apis.insert(api);
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
// dotProduct 都保号），用 `==` 比等于把这一整类差异永久豁免。
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

static std::string qstr(const QSize &s)   { return istr(s.width()) + "x" + istr(s.height()); }
static std::string qstr(const PkSize &s)  { return istr(s.width()) + "x" + istr(s.height()); }
static std::string qstr(const QSizeF &s)  { return dstr(s.width()) + "x" + dstr(s.height()); }
static std::string qstr(const PkSizeF &s) { return dstr(s.width()) + "x" + dstr(s.height()); }

static bool same_sz(const QSize &q, const PkSize &p)
{ return q.width() == p.width() && q.height() == p.height(); }
static bool same_szf(const QSizeF &q, const PkSizeF &p)
{ return same_double(q.width(), p.width()) && same_double(q.height(), p.height()); }

// ── Rect ─────────────────────────────────────────────────────────────────
// ⚠ 矩形一律按**四个内部坐标**打印与比较，不按 x/y/w/h：后者在退化矩形上是
// 多对一的（(0,0,0,0) 与 (5,5,0,0) 的 x/y/w/h 里 w/h 都是 0，但它们不相等），
// 用 x/y/w/h 比会把一整类差异静默豁免。
// 取值走 left/top/right/bottom 四条**各自独立的一行取值器**而不是 getCoords：
// 拿 getCoords 当比较手段的话，"getCoords 自己坏了"就检测不出来了。
static std::string qstr(const QRect &r)
{ return "[" + istr(r.left()) + "," + istr(r.top()) + ","
              + istr(r.right()) + "," + istr(r.bottom()) + "]"; }
static std::string qstr(const PkRect &r)
{ return "[" + istr(r.left()) + "," + istr(r.top()) + ","
              + istr(r.right()) + "," + istr(r.bottom()) + "]"; }

static bool same_rect(const QRect &q, const PkRect &p)
{
    return q.left() == p.left() && q.top() == p.top()
        && q.right() == p.right() && q.bottom() == p.bottom();
}

// 两侧都用 setCoords 摆坐标建矩形：只有这样才够得到 (l,t,w,h) 构造不出来的
// 形态（x2 == x1-1 的"宽恰为 0"、x2 < x1-1 的"需要交换"）。setCoords 自己
// 也有一条 rec()，坏了会单独现形。
static QRect mkQ(int a, int b, int c, int d) { QRect r; r.setCoords(a, b, c, d); return r; }
static PkRect mkP(int a, int b, int c, int d) { PkRect r; r.setCoords(a, b, c, d); return r; }

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

// ── shapeOf*：通用形态分流（**Task 3–6 照抄这个约定**）────────────────────
//
// **约定：一个 API 的 shape 必须由它「全部参与分量」算出，取其中最特殊的那种
// 形态命名 —— 不是只取第一对分量。** 所以签名是 initializer_list 而不是两个
// 标量：调用点被迫把参与的分量一个不落地列出来，漏一个是看得见的，
// 而 `shapeOfD(ax, bx)` 那种写法漏掉 ay/by 是看不见的。
//
// 为什么这条是硬要求（Task 2 复评实测）：`F::operator+` 的四个分量里只往
// shapeOfD 喂了 x 那一对，注入「只在 **y** 是 NaN 时出错」的缺陷后，8 036 次
// 差异被贴成 `F::operator+ finite` —— 与真实根因完全相反的标签。今天白名单是
// 空的所以不影响检出，可一旦有人往 .deviation 写下 `<api> finite` 这样一行，
// 「y 分量上的非有限缺陷」这一整片就被永久白名单化，**正是规则二要防的形状**。
// Rect（二元 8 个分量）/ Transform（二元 18 个分量）照抄会成倍恶化。
//
// 优先级从最能解释差异的往下排。新增形态往前插时想清楚它是不是比下面几条更
// 能解释根因 —— 排序就是"这次差异该归咎于谁"的判断。
static std::string shapeOfD(std::initializer_list<double> vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    return "finite";
}

static std::string shapeOfI(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v != 0) return "ordinary";
    return "origin";
}

// 同时吃整数与浮点分量的 API（跨类型运算）：两族各取最特殊形态，拼起来。
// 只取其中一族会把另一族的根因整片盖掉 —— 与上面那条是同一个道理。
static std::string shapeOfMixed(std::initializer_list<int> is,
                                std::initializer_list<double> ds)
{ return shapeOfI(is) + "+" + shapeOfD(ds); }

// ═══ Point 族：逐 API 对拍 ═════════════════════════════════════════════════
//
// 每个 rec() 的 tag 都由**这一次的实参**算出来。加新 API 时照这个形状写：
// 先想清楚"这个 API 会在什么输入形态上跟 Qt 分家"，把那个形态做成谓词，
// 让它参与 tag；想不出来就写 shapeOfD/shapeOfI 那种通用形态分流 —— 但**绝不能**
// 写成一个与输入无关的常量（那等于把这个 API 整个退出对拍）。

// 无输入的 API（默认构造），口径同 cmp_size_constants。只跑一次。
// **Task 4 修复轮补**：M-4 把 PkPoint.h 接进规则三的机器闸门之后，
// `PkPoint()` / `PkPointF()` 两条声明在 point_api.map 里找不到任何标签 ——
// 也就是说这两个默认构造**从来没被对拍比过**（Task 2 漏了，Size 族有、Point
// 族没有）。闸门刚上线就抓到了一个真实缺口，这两条 rec 是补上它。
static void cmp_point_constants()
{
    rec("defaultCtor", same_pt(QPoint(), PkPoint()), "no-input",
        "QPoint()", qstr(QPoint()), qstr(PkPoint()));
    rec("F::defaultCtor", same_ptf(QPointF(), PkPointF()), "no-input",
        "QPointF()", qstr(QPointF()), qstr(PkPointF()));
}

static void cmp_point_unary(int x, int y)
{
    const QPoint  q(x, y);
    const PkPoint p(x, y);
    const std::string in = "(" + istr(x) + "," + istr(y) + ")";
    const std::string sh = shapeOfI({x, y});

    rec("x", q.x() == p.x(), sh, in, istr(q.x()), istr(p.x()));
    rec("y", q.y() == p.y(), sh, in, istr(q.y()), istr(p.y()));
    rec("isNull", q.isNull() == p.isNull(), sh, in, bstr(q.isNull()), bstr(p.isNull()));

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
    // ⚠ **四个分量都参与**，不是只取 x 那一对（约定见 shapeOfI 上方）。
    const std::string sh = shapeOfI({ax, ay, bx, by});

    rec("operator+", same_pt(qa + qb, pa + pb),
        (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow" : sh,
        in, qstr(qa + qb), qstr(pa + pb));
    rec("operator-", same_pt(qa - qb, pa - pb),
        (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow" : sh,
        in, qstr(qa - qb), qstr(pa - pb));
    rec("operator==", (qa == qb) == (pa == pb), sh,
        in, bstr(qa == qb), bstr(pa == pb));
    rec("operator!=", (qa != qb) == (pa != pb), sh,
        in, bstr(qa != qb), bstr(pa != pb));
    { QPoint q2 = qa; PkPoint p2 = pa; q2 += qb; p2 += pb;
      rec("operator+=", same_pt(q2, p2),
          (addOverflows(ax, bx) || addOverflows(ay, by)) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }
    { QPoint q2 = qa; PkPoint p2 = pa; q2 -= qb; p2 -= pb;
      rec("operator-=", same_pt(q2, p2),
          (subOverflows(ax, bx) || subOverflows(ay, by)) ? "int-overflow" : sh,
          in, qstr(q2), qstr(p2)); }

    // dotProduct：静态成员，不防溢出（乘法与加法都可能溢出）
    rec("dotProduct", QPoint::dotProduct(qa, qb) == PkPoint::dotProduct(pa, pb),
        (mulOverflows(ax, bx) || mulOverflows(ay, by)
         || addOverflows((int)((long long)ax * bx), (int)((long long)ay * by)))
            ? "int-overflow" : sh,
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
    const std::string sh = shapeOfD({x, y});

    rec("F::x", same_double(q.x(), p.x()), sh, in, dstr(q.x()), dstr(p.x()));
    rec("F::y", same_double(q.y(), p.y()), sh, in, dstr(q.y()), dstr(p.y()));

    // isNull 用 qIsNull（d == 0.0）：-0.0 算 null、次正规数不算。零号与次正规
    // 正是这条唯一会分家的地方，所以让它们参与 tag。
    rec("F::isNull", q.isNull() == p.isNull(),
        (signedZero(x) || signedZero(y)) ? "signed-zero"
        : (subnormal(x) || subnormal(y)) ? "subnormal" : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));

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
    // ⚠ **四个分量都参与**，不是只取 x 那一对（约定见 shapeOfD 上方；只喂 x 时
    // 「只在 y 上出错」的缺陷会被贴成 finite —— 复评实测抓到 8 036 次错标）。
    const std::string sh = shapeOfD({ax, ay, bx, by});

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
        : (std::fabs(ax) > 1e150 || std::fabs(ay) > 1e150
           || std::fabs(bx) > 1e150 || std::fabs(by) > 1e150) ? "overflow-prone" : "finite",
        in, dstr(QPointF::dotProduct(qa, qb)), dstr(PkPointF::dotProduct(pa, pb)));
}

static void cmp_pointf_scale(double x, double y, double c)
{
    const QPointF  q(x, y);
    const PkPointF p(x, y);
    const std::string in = "(" + dstr(x) + "," + dstr(y) + ")*" + dstr(c);
    // 参与分量是 x、y 与标量 c 三个，一个都不能漏（约定见 shapeOfD 上方）。
    const std::string sh = shapeOfD({x, y, c});

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
    // fromPoint / roundTrip 只有 (x,y) 参与；mixedAdd 四个分量都参与，所以它
    // 用跨族的 shapeOfMixed —— 只喂 (cx,cy) 会把 int 侧的根因整片盖掉。
    const std::string sh = shapeOfI({x, y});
    rec("F::fromPoint", same_ptf(qf, pf), sh, in, qstr(qf), qstr(pf));
    rec("F::mixedAdd", same_ptf(QPointF(cx, cy) + qi, PkPointF(cx, cy) + pi),
        shapeOfMixed({x, y}, {cx, cy}), in,
        qstr(QPointF(cx, cy) + qi), qstr(PkPointF(cx, cy) + pi));
    // 往返：提升再 toPoint 必须回到原点（除非 int 值域边界上的 double 表示丢精度）
    rec("F::roundTrip", same_pt(qf.toPoint(), pf.toPoint()), sh, in,
        qstr(qf.toPoint()), qstr(pf.toPoint()));
}

// ═══ Size 族：逐 API 对拍 ══════════════════════════════════════════════════
//
// 与 Point 族的三处结构性不同，直接决定了这一节的 tag 怎么设计：
//   ① **三条谓词的分界线互不相同**（isNull 只认 0、isEmpty 的门槛是 `<1`／浮点
//      `<=0.`、isValid 是 `>=0`）。所以 0 / 1 / 负 是三个独立的根因，
//      不能压成 shapeOf* 里的一档 —— 各谓词用各自的边界谓词做 tag。
//   ② **scaled/scale 多了一个 mode 参数**，而三种 mode 走的是三条不同的分支
//      （Ignore 直接返回目标；Keep/Expand 只差比较方向）。mode 是输入的一部分，
//      必须参与 tag（规则一），否则一种 mode 上的偏离会把另外两种一起罩住。
//   ③ 整数版 scaled 内部有 **qint64 中间量**再窄回 int，浮点版没有。
//      "窄化会不会回绕"是整数版独有的根因，单独一档。
static const Qt::AspectRatioMode kQtModes[3] = {
    Qt::IgnoreAspectRatio, Qt::KeepAspectRatio, Qt::KeepAspectRatioByExpanding };
static const pkoracle::Qt::AspectRatioMode kPkModes[3] = {
    pkoracle::Qt::IgnoreAspectRatio, pkoracle::Qt::KeepAspectRatio,
    pkoracle::Qt::KeepAspectRatioByExpanding };
static const char *kModeName[3] = { "ignore", "keep", "expand" };

// Size 版的通用形态分流。与 shapeOfI/shapeOfD 分开：Point 那两个把 0 与负数
// 归成同一档（"ordinary"），而尺寸的语义里这两者是完全不同的东西。
static std::string shapeOfSizeI(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v < 0) return "negative-dim";
    for (int v : vs) if (v == 0) return "zero-dim";
    for (int v : vs) if (v == 1) return "unit-dim";      // isEmpty 的 `<1` 边界
    return "ordinary";
}

static std::string shapeOfSizeD(std::initializer_list<double> vs)
{
    for (double v : vs) if (nonFinite(v)) return "nonfinite";
    for (double v : vs) if (signedZero(v)) return "signed-zero";
    for (double v : vs) if (subnormal(v)) return "subnormal";
    for (double v : vs) if (v == 0.0) return "zero-dim";
    for (double v : vs) if (v < 0.0) return "negative-dim";
    for (double v : vs) if (std::fabs(v) > 1e300) return "huge";
    return "finite";
}

// 无输入的 API（默认构造）：**只有一种输入形态**，所以 tag 退化成常量是这条
// 规则的边界情形而不是违反 —— 规则一禁的是"有输入却不让输入参与 tag"。
// 只跑一次。
static void cmp_size_constants()
{
    rec("S::defaultCtor", same_sz(QSize(), PkSize()), "no-input",
        "QSize()", qstr(QSize()), qstr(PkSize()));
    rec("SF::defaultCtor", same_szf(QSizeF(), PkSizeF()), "no-input",
        "QSizeF()", qstr(QSizeF()), qstr(PkSizeF()));
    rec("S::defaultPredicates",
        QSize().isNull() == PkSize().isNull()
        && QSize().isEmpty() == PkSize().isEmpty()
        && QSize().isValid() == PkSize().isValid(),
        "no-input", "QSize()",
        bstr(QSize().isNull()) + bstr(QSize().isEmpty()) + bstr(QSize().isValid()),
        bstr(PkSize().isNull()) + bstr(PkSize().isEmpty()) + bstr(PkSize().isValid()));
    rec("SF::defaultPredicates",
        QSizeF().isNull() == PkSizeF().isNull()
        && QSizeF().isEmpty() == PkSizeF().isEmpty()
        && QSizeF().isValid() == PkSizeF().isValid(),
        "no-input", "QSizeF()",
        bstr(QSizeF().isNull()) + bstr(QSizeF().isEmpty()) + bstr(QSizeF().isValid()),
        bstr(PkSizeF().isNull()) + bstr(PkSizeF().isEmpty()) + bstr(PkSizeF().isValid()));
}

static void cmp_size_unary(int w, int h)
{
    const QSize  q(w, h);
    const PkSize p(w, h);
    const std::string in = istr(w) + "x" + istr(h);
    const std::string sh = shapeOfSizeI({w, h});
    const bool extremum = intExtremum(w) || intExtremum(h);

    rec("S::width", q.width() == p.width(), sh, in, istr(q.width()), istr(p.width()));
    rec("S::height", q.height() == p.height(), sh, in, istr(q.height()), istr(p.height()));

    // 三条谓词的边界各不相同，各用各的（把它们压成同一个 tag 就等于允许
    // 「isValid 抄了 isEmpty 的门槛」这类偏离藏在同一片白名单下）。
    rec("S::isNull", q.isNull() == p.isNull(),
        extremum ? std::string("int-extremum") : (w == 0 || h == 0) ? std::string("zero-dim") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("S::isEmpty", q.isEmpty() == p.isEmpty(),
        extremum ? std::string("int-extremum")
                 : (w <= 1 || h <= 1) ? std::string("empty-boundary") : sh,
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("S::isValid", q.isValid() == p.isValid(),
        extremum ? std::string("int-extremum")
                 : (w <= 0 || h <= 0) ? std::string("valid-boundary") : sh,
        in, bstr(q.isValid()), bstr(p.isValid()));

    // 规则三：一个重载一条 rec（原来 setWidth 与 setHeight 挤在一条里，
    // rwidth/rheight 同样 —— 拆开之后哪个重载分的家在 DIFFTAG 里直接看得见）。
    { QSize q2 = q; PkSize p2 = p; q2.setWidth(h); p2.setWidth(h);
      rec("S::setWidth", same_sz(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.setHeight(w); p2.setHeight(w);
      rec("S::setHeight", same_sz(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.rwidth() += 1; p2.rwidth() += 1;
      rec("S::rwidth", same_sz(q2, p2),
          (w == INT_MAX) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    { QSize q2 = q; PkSize p2 = p; q2.rheight() -= 1; p2.rheight() -= 1;
      rec("S::rheight", same_sz(q2, p2),
          (h == INT_MIN) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
}

static void cmp_size_binary(int aw, int ah, int bw, int bh)
{
    const QSize  qa(aw, ah), qb(bw, bh);
    const PkSize pa(aw, ah), pb(bw, bh);
    const std::string in = istr(aw) + "x" + istr(ah) + "|" + istr(bw) + "x" + istr(bh);
    // ⚠ **四个分量都参与**（约定见 shapeOfI 上方那段）。
    const std::string sh = shapeOfSizeI({aw, ah, bw, bh});

    rec("S::operator+", same_sz(qa + qb, pa + pb),
        (addOverflows(aw, bw) || addOverflows(ah, bh)) ? std::string("int-overflow") : sh,
        in, qstr(qa + qb), qstr(pa + pb));
    rec("S::operator-", same_sz(qa - qb, pa - pb),
        (subOverflows(aw, bw) || subOverflows(ah, bh)) ? std::string("int-overflow") : sh,
        in, qstr(qa - qb), qstr(pa - pb));
    rec("S::operator==", (qa == qb) == (pa == pb), sh, in, bstr(qa == qb), bstr(pa == pb));
    rec("S::operator!=", (qa != qb) == (pa != pb), sh, in, bstr(qa != qb), bstr(pa != pb));
    { QSize q2 = qa; PkSize p2 = pa; q2 += qb; p2 += pb;
      rec("S::operator+=", same_sz(q2, p2),
          (addOverflows(aw, bw) || addOverflows(ah, bh)) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    { QSize q2 = qa; PkSize p2 = pa; q2 -= qb; p2 -= pb;
      rec("S::operator-=", same_sz(q2, p2),
          (subOverflows(aw, bw) || subOverflows(ah, bh)) ? std::string("int-overflow") : sh,
          in, qstr(q2), qstr(p2)); }
    // expandedTo 是 qMax 逐分量：唯一会分家的形态是"两侧相等/一侧更大"的选择，
    // 由分量形态解释即可。
    rec("S::expandedTo", same_sz(qa.expandedTo(qb), pa.expandedTo(pb)), sh,
        in, qstr(qa.expandedTo(qb)), qstr(pa.expandedTo(pb)));
}

// QSize 的缩放：只有 qreal 一个重载（不像 QPoint 有 float/double/int 三个），
// 但除法那两条在 Qt 里带 Q_ASSERT —— 对拍用 -DQT_NO_DEBUG 编（= Krita 发布构建
// 的形态，理由见 run_oracle.sh 与 README 偏离清单），断言整条编译掉。
static void cmp_size_scale(int w, int h, double c)
{
    const QSize  q(w, h);
    const PkSize p(w, h);
    const std::string in = istr(w) + "x" + istr(h) + "*" + dstr(c);
    const double pw = (double)w * c, ph = (double)h * c;
    const std::string mtag =
          (nonFinite(pw) || nonFinite(ph))         ? "nonfinite-product"
        : (outOfIntRange(pw) || outOfIntRange(ph)) ? "product-out-of-int-range"
        : (halfBoundary(pw) || halfBoundary(ph))   ? "half-boundary"
        : (nearHalfUlp(pw) || nearHalfUlp(ph))     ? "near-half-ulp"
                                                    : "in-range";
    rec("S::operator*", same_sz(q * c, p * c), mtag, in, qstr(q * c), qstr(p * c));
    rec("S::operator*(rev)", same_sz(c * q, c * p), mtag, in, qstr(c * q), qstr(c * p));
    { QSize q2 = q; PkSize p2 = p; q2 *= c; p2 *= c;
      rec("S::operator*=", same_sz(q2, p2), mtag, in, qstr(q2), qstr(p2)); }

    const double dw = (double)w / c, dh = (double)h / c;
    const std::string dtag =
          (c == 0.0)                               ? "divisor-zero"
        : (nonFinite(dw) || nonFinite(dh))         ? "nonfinite-quotient"
        : (outOfIntRange(dw) || outOfIntRange(dh)) ? "quotient-out-of-int-range"
        : (halfBoundary(dw) || halfBoundary(dh))   ? "half-boundary"
        : (nearHalfUlp(dw) || nearHalfUlp(dh))     ? "near-half-ulp"
                                                    : "in-range";
    rec("S::operator/", same_sz(q / c, p / c), dtag, in, qstr(q / c), qstr(p / c));
    { QSize q2 = q; PkSize p2 = p; q2 /= c; p2 /= c;
      rec("S::operator/=", same_sz(q2, p2), dtag, in, qstr(q2), qstr(p2)); }
}

// scaled/scale：**mode 参与 tag**，且分支形态（退化源 / qint64 窄化 / 负分量）
// 各自成一档 —— 这三条分支在 Qt 里走的是完全不同的代码，混成一个 tag 等于
// 把三片行为一起白名单化（规则二）。
static void cmp_size_scaled(int sw, int sh, int tw, int th, int mi)
{
    const QSize  qs(sw, sh), qt(tw, th);
    const PkSize ps(sw, sh), pt(tw, th);
    const std::string in = istr(sw) + "x" + istr(sh) + "->" + istr(tw) + "x" + istr(th);

    // qint64 中间量是否越出 int 值域（越出就要靠窄化回绕，是独立的根因）
    bool narrows = false;
    if (sw != 0 && sh != 0) {
        const long long rw = (long long)th * (long long)sw / (long long)sh;
        const long long rh = (long long)tw * (long long)sh / (long long)sw;
        narrows = rw < INT_MIN || rw > INT_MAX || rh < INT_MIN || rh > INT_MAX;
    }
    const std::string branch =
          (mi == 0)                  ? "ignore-returns-target"
        : (sw == 0 || sh == 0)       ? "degenerate-source"
        : narrows                    ? "int64-narrowing"
        : (sw < 0 || sh < 0 || tw < 0 || th < 0) ? "negative-dim"
        : (intExtremum(sw) || intExtremum(sh) || intExtremum(tw) || intExtremum(th))
                                     ? "int-extremum"
                                     : "ordinary";
    const std::string tag = std::string(kModeName[mi]) + "/" + branch;

    rec("S::scaled(size)", same_sz(qs.scaled(qt, kQtModes[mi]), ps.scaled(pt, kPkModes[mi])),
        tag, in, qstr(qs.scaled(qt, kQtModes[mi])), qstr(ps.scaled(pt, kPkModes[mi])));
    rec("S::scaled(w,h)",
        same_sz(qs.scaled(tw, th, kQtModes[mi]), ps.scaled(tw, th, kPkModes[mi])),
        tag, in, qstr(qs.scaled(tw, th, kQtModes[mi])), qstr(ps.scaled(tw, th, kPkModes[mi])));
    { QSize q2 = qs; PkSize p2 = ps;
      q2.scale(qt, kQtModes[mi]); p2.scale(pt, kPkModes[mi]);
      rec("S::scale(size)", same_sz(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    { QSize q2 = qs; PkSize p2 = ps;
      q2.scale(tw, th, kQtModes[mi]); p2.scale(tw, th, kPkModes[mi]);
      rec("S::scale(w,h)", same_sz(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_unary(double w, double h)
{
    const QSizeF  q(w, h);
    const PkSizeF p(w, h);
    const std::string in = dstr(w) + "x" + dstr(h);
    const std::string sh = shapeOfSizeD({w, h});

    rec("SF::width", same_double(q.width(), p.width()), sh, in, dstr(q.width()), dstr(p.width()));
    rec("SF::height", same_double(q.height(), p.height()), sh, in, dstr(q.height()), dstr(p.height()));

    // isNull 用 qIsNull（== 0.0）：-0.0 算 null、次正规不算 —— 与 isEmpty 的
    // `<= 0.`、isValid 的 `>= 0.` 三条门槛互不相同，各用各的 tag。
    rec("SF::isNull", q.isNull() == p.isNull(),
        (signedZero(w) || signedZero(h)) ? std::string("signed-zero")
        : (subnormal(w) || subnormal(h)) ? std::string("subnormal")
        : (w == 0.0 || h == 0.0)         ? std::string("zero-dim") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("SF::isEmpty", q.isEmpty() == p.isEmpty(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-dim")
        : (w <= 0.0 || h <= 0.0)         ? std::string("empty-boundary") : sh,
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("SF::isValid", q.isValid() == p.isValid(),
        (std::isnan(w) || std::isnan(h)) ? std::string("nan-dim")
        : (w <= 0.0 || h <= 0.0)         ? std::string("valid-boundary") : sh,
        in, bstr(q.isValid()), bstr(p.isValid()));

    // toSize 走 qRound：半值方向、越出 int 值域、差一个 ulp 的半值是三个根因。
    rec("SF::toSize", same_sz(q.toSize(), p.toSize()),
        (outOfIntRange(w) || outOfIntRange(h)) ? std::string("out-of-int-range")
        : (halfBoundary(w) || halfBoundary(h)) ? std::string("half-boundary")
        : (nearHalfUlp(w) || nearHalfUlp(h))   ? std::string("near-half-ulp")
        : (subnormal(w) || subnormal(h))       ? std::string("subnormal")
                                                : std::string("in-range"),
        in, qstr(q.toSize()), qstr(p.toSize()));

    // 规则三：一个重载一条 rec（拆分理由同整数版）。
    { QSizeF q2 = q; PkSizeF p2 = p; q2.setWidth(h); p2.setWidth(h);
      rec("SF::setWidth", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.setHeight(w); p2.setHeight(w);
      rec("SF::setHeight", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.rwidth() += 1.0; p2.rwidth() += 1.0;
      rec("SF::rwidth", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = q; PkSizeF p2 = p; q2.rheight() -= 1.0; p2.rheight() -= 1.0;
      rec("SF::rheight", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_binary(double aw, double ah, double bw, double bh)
{
    const QSizeF  qa(aw, ah), qb(bw, bh);
    const PkSizeF pa(aw, ah), pb(bw, bh);
    const std::string in = dstr(aw) + "x" + dstr(ah) + "|" + dstr(bw) + "x" + dstr(bh);
    const std::string sh = shapeOfSizeD({aw, ah, bw, bh});

    rec("SF::operator+", same_szf(qa + qb, pa + pb), sh, in, qstr(qa + qb), qstr(pa + pb));
    rec("SF::operator-", same_szf(qa - qb, pa - pb), sh, in, qstr(qa - qb), qstr(pa - pb));

    // ⚠ QSizeF::operator== 是**两个分量各一次 qFuzzyCompare，没有零分支**
    //（与 QPointF 不同，那边任一侧为 0 时改走 fuzzyIsNull）。qFuzzyCompare 的
    // 右端取 qMin(|a|,|b|)，所以"恰好一侧是 0"与"两侧都是 0"是两个截然不同的
    // 结局（前者恒 false、后者恒 true），必须分成两个 tag。
    const bool zeroW = (aw == 0.0 || bw == 0.0), bothZeroW = (aw == 0.0 && bw == 0.0);
    const bool zeroH = (ah == 0.0 || bh == 0.0), bothZeroH = (ah == 0.0 && bh == 0.0);
    const std::string eqtag =
          (nonFinite(aw) || nonFinite(bw) || nonFinite(ah) || nonFinite(bh)) ? "nonfinite"
        : ((zeroW && !bothZeroW) || (zeroH && !bothZeroH))                   ? "one-side-zero"
        : (bothZeroW || bothZeroH)                                           ? "both-zero"
        : (subnormal(aw) || subnormal(bw) || subnormal(ah) || subnormal(bh)) ? "subnormal"
                                                                             : "relative-branch";
    rec("SF::operator==", (qa == qb) == (pa == pb), eqtag, in, bstr(qa == qb), bstr(pa == pb));
    rec("SF::operator!=", (qa != qb) == (pa != pb), eqtag, in, bstr(qa != qb), bstr(pa != pb));

    { QSizeF q2 = qa; PkSizeF p2 = pa; q2 += qb; p2 += pb;
      rec("SF::operator+=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QSizeF q2 = qa; PkSizeF p2 = pa; q2 -= qb; p2 -= pb;
      rec("SF::operator-=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }

    // expandedTo 是 qMax（`(a<b)?b:a`），**NaN 上不可交换、零号上保号** ——
    // 这两类正是它唯一会分家的地方，让它们参与 tag。
    rec("SF::expandedTo", same_szf(qa.expandedTo(qb), pa.expandedTo(pb)),
        (std::isnan(aw) || std::isnan(bw) || std::isnan(ah) || std::isnan(bh))
            ? std::string("nan-dim")
        : (signedZero(aw) || signedZero(bw) || signedZero(ah) || signedZero(bh))
            ? std::string("signed-zero") : sh,
        in, qstr(qa.expandedTo(qb)), qstr(pa.expandedTo(pb)));
}

static void cmp_sizef_scale(double w, double h, double c)
{
    const QSizeF  q(w, h);
    const PkSizeF p(w, h);
    const std::string in = dstr(w) + "x" + dstr(h) + "*" + dstr(c);
    const std::string sh = shapeOfSizeD({w, h, c});

    rec("SF::operator*", same_szf(q * c, p * c), sh, in, qstr(q * c), qstr(p * c));
    rec("SF::operator*(rev)", same_szf(c * q, c * p), sh, in, qstr(c * q), qstr(c * p));
    { QSizeF q2 = q; PkSizeF p2 = p; q2 *= c; p2 *= c;
      rec("SF::operator*=", same_szf(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    rec("SF::operator/", same_szf(q / c, p / c),
        (c == 0.0) ? std::string("divisor-zero") : sh, in, qstr(q / c), qstr(p / c));
    { QSizeF q2 = q; PkSizeF p2 = p; q2 /= c; p2 /= c;
      rec("SF::operator/=", same_szf(q2, p2),
          (c == 0.0) ? std::string("divisor-zero") : sh, in, qstr(q2), qstr(p2)); }
}

static void cmp_sizef_scaled(double sw, double sh, double tw, double th, int mi)
{
    const QSizeF  qs(sw, sh), qt(tw, th);
    const PkSizeF ps(sw, sh), pt(tw, th);
    const std::string in = dstr(sw) + "x" + dstr(sh) + "->" + dstr(tw) + "x" + dstr(th);
    // 浮点版的分支条件是 qIsNull（== 0.0，含 -0.0），**次正规不走那条**；
    // 而 NaN 会让 `rw <= s.wd` 恒假从而自动落到 else 分支 —— 两者根因不同。
    const std::string branch =
          (mi == 0)                          ? "ignore-returns-target"
        : (sw == 0.0 || sh == 0.0)           ? "degenerate-source"
        : (std::isnan(sw) || std::isnan(sh) || std::isnan(tw) || std::isnan(th))
                                             ? "nan-dim"
        : (nonFinite(sw) || nonFinite(sh) || nonFinite(tw) || nonFinite(th))
                                             ? "infinite-dim"
        : (subnormal(sw) || subnormal(sh) || subnormal(tw) || subnormal(th))
                                             ? "subnormal"
        : (sw < 0.0 || sh < 0.0 || tw < 0.0 || th < 0.0) ? "negative-dim"
                                             : "ordinary";
    const std::string tag = std::string(kModeName[mi]) + "/" + branch;

    rec("SF::scaled(size)", same_szf(qs.scaled(qt, kQtModes[mi]), ps.scaled(pt, kPkModes[mi])),
        tag, in, qstr(qs.scaled(qt, kQtModes[mi])), qstr(ps.scaled(pt, kPkModes[mi])));
    rec("SF::scaled(w,h)",
        same_szf(qs.scaled(tw, th, kQtModes[mi]), ps.scaled(tw, th, kPkModes[mi])),
        tag, in, qstr(qs.scaled(tw, th, kQtModes[mi])), qstr(ps.scaled(tw, th, kPkModes[mi])));
    { QSizeF q2 = qs; PkSizeF p2 = ps;
      q2.scale(qt, kQtModes[mi]); p2.scale(pt, kPkModes[mi]);
      rec("SF::scale(size)", same_szf(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    // ⚠ 规则三：**这一条曾经漏掉**（整数版四个重载齐全、浮点版只有三个）。
    // 复评把 PkSizeF::scale(qreal,qreal,mode) 整个改坏，对拍一条都没红。
    { QSizeF q2 = qs; PkSizeF p2 = ps;
      q2.scale(tw, th, kQtModes[mi]); p2.scale(tw, th, kPkModes[mi]);
      rec("SF::scale(w,h)", same_szf(q2, p2), tag, in, qstr(q2), qstr(p2)); }
}

// PkSize → PkSizeF 的隐式提升（Task 4/5 的 PkRectF 靠这条），以及往返。
static void cmp_size_promotion(int w, int h, double fw, double fh)
{
    const QSizeF  qf = QSize(w, h);      // 隐式
    const PkSizeF pf = PkSize(w, h);
    const std::string in = istr(w) + "x" + istr(h) + "+" + dstr(fw) + "x" + dstr(fh);
    const std::string sh = shapeOfSizeI({w, h});

    rec("SF::fromSize", same_szf(qf, pf), sh, in, qstr(qf), qstr(pf));
    // 往返：提升再 toSize 必须回到原值（int 全值域内都是精确的）
    rec("SF::roundTrip", same_sz(qf.toSize(), pf.toSize()), sh, in,
        qstr(qf.toSize()), qstr(pf.toSize()));
    // 混合二元：浮点尺寸与**隐式提升上来的**整数尺寸做 expandedTo。
    // 两族分量都参与 tag（约定见 shapeOfMixed 上方）。
    rec("SF::mixedExpandedTo",
        same_szf(QSizeF(fw, fh).expandedTo(QSize(w, h)),
                 PkSizeF(fw, fh).expandedTo(PkSize(w, h))),
        shapeOfSizeI({w, h}) + "+" + shapeOfSizeD({fw, fh}), in,
        qstr(QSizeF(fw, fh).expandedTo(QSize(w, h))),
        qstr(PkSizeF(fw, fh).expandedTo(PkSize(w, h))));
    rec("SF::mixedEquality",
        (QSizeF(fw, fh) == QSizeF(QSize(w, h))) == (PkSizeF(fw, fh) == PkSizeF(PkSize(w, h))),
        shapeOfSizeI({w, h}) + "+" + shapeOfSizeD({fw, fh}), in,
        bstr(QSizeF(fw, fh) == QSizeF(QSize(w, h))),
        bstr(PkSizeF(fw, fh) == PkSizeF(PkSize(w, h))));
}

// ═══ Rect 族（整数）：逐 API 对拍 ══════════════════════════════════════════
//
// 与 Point/Size 两族的四处结构性不同，直接决定了这一节的 tag 怎么设计：
//   ① **内部是四个边界坐标**，(l,t,w,h) 构造够不到一大片形态
//      （x2 == x1-1 的"宽恰为 0"只能由 setCoords 摆出来），所以输入集是
//      **坐标四元组**，不是 (x,y,w,h) 四元组。
//   ② **退化分三档而不是两档**：x2 == x1-1（宽 0，normalized **不**交换）、
//      x2 < x1-1（宽为负，交换）、x2 == x1（宽 1）。这三条线是 normalized /
//      contains / operator& 的真实分支边界，压成一档等于把它们一起白名单化。
//   ③ **双目 API 在 null 上不对称**（operator| 的两条 return 有先后），所以
//      双目 tag 用 `shapeA ^ shapeB` 这种**有序对**，不能照抄 Point/Size 那条
//      「把两侧分量合起来取最特殊的一个」的约定 —— 那条会把
//      「a 为 null」与「b 为 null」压成同一个 tag，恰好盖住这个 API 唯一的怪处。
//      有序对比"取最特殊"**更窄**，方向上是安全的（规则二）。
//   ④ 双目 API 还额外带一个**相对位置**分量（disjoint / a-covers-b / …）。
//      它由 qtInterval() 用纯 long long 算术算出来，**不调用任何一侧的实现**，
//      所以仍然是"由输入形态构造"的 tag（规则一）。
//
// 规则三（一个重载一条 rec）在这一族是硬要求，且有机器闸门：
// 见 g_apis / APISEEN 与 oracle/rect_api.map。

// 跨距是否越出 int。⚠ **两个量都要查**：`x2 - x1` 这个中间量自己就可能溢出，
// 而最终的 `x2 - x1 + 1` 反而回到值域里 —— 例如 x1=1、x2=INT_MIN 时
// x2-x1 = -2147483649（越界），+1 之后是 INT_MIN（在界内）。
// 只查后者会漏掉一整片跨距溢出的输入，让它们混进 `extremum` / `normal` 两档 ——
// 那正是规则二要防的形状：标签比事实宽一格，两类根因不同的输入压成同一个 tag。
// （它现在**只用来给 tag 分档**，不再有任何豁免作用：曾经基于它的 spanUB()
// 白名单随 Task 4 修复轮删除，见下面 pairTag 的长注释。）
static bool spanOverflows(int lo, int hi)
{
    const long long d = (long long)hi - lo;
    if (d < INT_MIN || d > INT_MAX) return true;
    const long long s = d + 1;
    return s < INT_MIN || s > INT_MAX;
}

// 单个轴的形态。**六档**，顺序就是"这次差异该归咎于谁"的判断：
//   span-overflow → 跨距本身溢出（只有极值坐标才到得了）
//   zero          → x2 == x1-1，宽恰为 0：normalized **不**交换的边界、
//                   isNull 的构成条件、contains(point) 上区间为空
//   negative      → x2 < x1-1，宽为负：要交换的那一侧
//   unit          → x2 == x1，宽 1
//   extremum      → 坐标里有 INT_MIN/INT_MAX 但跨距没溢出
//   normal        → 其余
static std::string axisShape(int lo, int hi)
{
    if (spanOverflows(lo, hi)) return "span-overflow";
    if (hi == lo - 1) return "zero";
    if (hi < lo - 1) return "negative";
    if (hi == lo) return "unit";
    if (intExtremum(lo) || intExtremum(hi)) return "extremum";
    return "normal";
}

static std::string shapeOfRect(int x1, int y1, int x2, int y2)
{ return axisShape(x1, x2) + "-" + axisShape(y1, y2); }

// 按 **Qt 的翻正规则**（`x2 - x1 + 1 < 0` 时交换）算出一个轴上的闭区间。
// 纯 long long 算术，**不调用两侧任何一个实现** —— 所以拿它做 tag 不构成
// "用被测物给自己贴标签"。
static void qtInterval(int lo, int hi, long long &l, long long &r)
{
    if ((long long)hi - lo + 1 < 0) { l = hi; r = lo; } else { l = lo; r = hi; }
}

static std::string axisRel(int alo, int ahi, int blo, int bhi)
{
    long long l1, r1, l2, r2;
    qtInterval(alo, ahi, l1, r1);
    qtInterval(blo, bhi, l2, r2);
    if (l1 > r2 || l2 > r1) return "disjoint";
    if (l1 == l2 && r1 == r2) return "same";
    if (l2 >= l1 && r2 <= r1) return "a-covers-b";
    if (l1 >= l2 && r1 <= r2) return "b-covers-a";
    return "partial";
}

// 双目 tag。**全部输入一律走细粒度 `shape/<形态对>/<相对位置>`，没有豁免档。**
//
// ⚠ 前缀从 `defined/` 改名成 `shape/`：跨距溢出的输入两侧**仍然都是 UB**
// （只是现在两侧同旗标，UB 的取值恰好一致），叫 "defined" 等于让标签比事实
// 宽一格 —— 正是方法论规则二要防的形状。`shape/` 只声称"这是按输入形态贴的"。
//
// 曾经这里有一个 `spanUB()` 分支，把"任一侧跨距越出 int"的输入整片塌成单个
// tag `span-overflow-ub` 并在 geometry.deviation 里按 9 个 API 各声明一行。
// 那套机制随 Task 4 修复轮**整个删掉**，原因是它解决的问题已经从根上没了：
//
//   那 704 589 条差异全部来自**旗标不对等** —— operator| / operator& /
//   contains(QRect) / intersects 的实现编在 libQt5Core.so 里，对拍 TU 的
//   `-fwrapv` 到不了那侧，于是同一条 `x2 - x1 + 1 < 0` 判据两侧取值不同。
//   run_oracle.sh 去掉 `-fwrapv` 之后两侧同旗标，mismatch 直接回到 3（只剩
//   canary）—— **不需要豁免任何输入**。
//
// 删掉它的收益是可量的：那一档原本覆盖 **3 286 575 次比对（占 total 的 2.62%，
// 实测计数）**，这些比对现在全都是真比对，任何差异都会是**新 tag** 而 FAIL。
// （历史记录：修复前的报告写的 704 589 是**差异**次数，不是被覆盖的**比对**
// 次数，把盲区规模低估了 4.7 倍 —— 这正是"豁免档必须量化"的理由。）
static std::string pairTag(int ax1, int ay1, int ax2, int ay2,
                           int bx1, int by1, int bx2, int by2)
{
    return "shape/" + shapeOfRect(ax1, ay1, ax2, ay2)
         + "^" + shapeOfRect(bx1, by1, bx2, by2)
         + "/" + axisRel(ax1, ax2, bx1, bx2) + "," + axisRel(ay1, ay2, by1, by2);
}

// normalized 专用：直接把"这个轴交不交换"做成 tag。四档里 boundary-zero 与
// swap 只差一格，正是 `x2 < x1-1` 写成 `x2 < x1` 时会挪动的那条线。
static std::string swapAxis(int lo, int hi)
{
    if (hi == lo - 1) return "boundary-zero";
    if (hi < lo - 1) return "swap";
    if (hi == lo) return "unit";
    return "keep";
}

// contains(point) 专用：点相对于**点版翻正规则**（`x2 < x1 - 1`）给出的区间
// 落在哪。at-lo / at-hi 两档就是差一边界，below/above 是外侧。
static std::string edgeTag(int lo, int hi, int p)
{
    long long l, r;
    if ((long long)hi < (long long)lo - 1) { l = hi; r = lo; } else { l = lo; r = hi; }
    if (l > r) return "empty-interval";
    if (p < l) return "below";
    if (p == l) return "at-lo";
    if (p == r) return "at-hi";
    if (p > r) return "above";
    return "inside";
}

static std::string argShape(std::initializer_list<int> vs)
{
    for (int v : vs) if (intExtremum(v)) return "int-extremum";
    for (int v : vs) if (v == 0) return "zero";
    for (int v : vs) if (v < 0) return "negative";
    return "positive";
}

static std::string rin(int a, int b, int c, int d)
{ return "[" + istr(a) + "," + istr(b) + "," + istr(c) + "," + istr(d) + "]"; }

// 无输入的 API（默认构造）：只有一种输入形态，tag 退化成常量是规则一的
// 边界情形而不是违反（与 cmp_size_constants 同一个道理）。只跑一次。
static void cmp_rect_constants()
{
    rec("R::defaultCtor", same_rect(QRect(), PkRect()), "no-input", "QRect()",
        qstr(QRect()), qstr(PkRect()));
}

// 三个带参构造。**三条各自一个 rec**（规则三）：它们做的事不一样 ——
// (l,t,w,h) 与 (topLeft,size) 做 +w-1，(topLeft,bottomRight) 直接摆坐标。
static void cmp_rect_ctor(int a, int b, int c, int d)
{
    const std::string in = rin(a, b, c, d);
    // 分歧只可能出在"+w-1 有没有溢出"与"宽高是 0 / 负"这两类形态上
    const bool ov = (long long)a + c - 1 > INT_MAX || (long long)a + c - 1 < INT_MIN
                 || (long long)b + d - 1 > INT_MAX || (long long)b + d - 1 < INT_MIN;
    const std::string sh =
          ov                                              ? "ctor-span-overflow"
        : (intExtremum(a) || intExtremum(b)
           || intExtremum(c) || intExtremum(d))            ? "int-extremum"
        : (c == 0 || d == 0)                               ? "zero-dim"
        : (c < 0 || d < 0)                                 ? "negative-dim"
                                                           : "ordinary";

    rec("R::ctorLTWH", same_rect(QRect(a, b, c, d), PkRect(a, b, c, d)), sh, in,
        qstr(QRect(a, b, c, d)), qstr(PkRect(a, b, c, d)));
    rec("R::ctorPoints",
        same_rect(QRect(QPoint(a, b), QPoint(c, d)), PkRect(PkPoint(a, b), PkPoint(c, d))),
        shapeOfRect(a, b, c, d), in,
        qstr(QRect(QPoint(a, b), QPoint(c, d))), qstr(PkRect(PkPoint(a, b), PkPoint(c, d))));
    rec("R::ctorPointSize",
        same_rect(QRect(QPoint(a, b), QSize(c, d)), PkRect(PkPoint(a, b), PkSize(c, d))),
        sh, in,
        qstr(QRect(QPoint(a, b), QSize(c, d))), qstr(PkRect(PkPoint(a, b), PkSize(c, d))));
}

// 一元取值器 + normalized。输入是**坐标四元组**。
static void cmp_rect_unary(int x1, int y1, int x2, int y2)
{
    const QRect  q = mkQ(x1, y1, x2, y2);
    const PkRect p = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2);
    const std::string sh = shapeOfRect(x1, y1, x2, y2);

    rec("R::left", q.left() == p.left(), sh, in, istr(q.left()), istr(p.left()));
    rec("R::top", q.top() == p.top(), sh, in, istr(q.top()), istr(p.top()));
    rec("R::right", q.right() == p.right(), sh, in, istr(q.right()), istr(p.right()));
    rec("R::bottom", q.bottom() == p.bottom(), sh, in, istr(q.bottom()), istr(p.bottom()));
    rec("R::x", q.x() == p.x(), sh, in, istr(q.x()), istr(p.x()));
    rec("R::y", q.y() == p.y(), sh, in, istr(q.y()), istr(p.y()));

    // 差一：width/height 是 x2-x1+1，在极值坐标上溢出（两侧都是 UB，对拍靠
    // "两侧同旗标"比同取值，不靠 -fwrapv 钉死 —— 见 run_oracle.sh 的旗标注释）
    rec("R::width", q.width() == p.width(), sh, in, istr(q.width()), istr(p.width()));
    rec("R::height", q.height() == p.height(), sh, in, istr(q.height()), istr(p.height()));
    rec("R::size", same_sz(q.size(), p.size()), sh, in, qstr(q.size()), qstr(p.size()));

    // 三条谓词的分界线各不相同，**各用各的 tag**（把它们压成一个 sh 就等于
    // 允许"isValid 抄了 isEmpty 的门槛"这类偏离藏在同一片白名单下）。
    rec("R::isNull", q.isNull() == p.isNull(),
        (x2 == x1 - 1 || y2 == y1 - 1) ? std::string("null-boundary") : sh,
        in, bstr(q.isNull()), bstr(p.isNull()));
    rec("R::isEmpty", q.isEmpty() == p.isEmpty(),
        (x1 > x2 || y1 > y2) ? std::string("empty-side") : std::string("nonempty-side"),
        in, bstr(q.isEmpty()), bstr(p.isEmpty()));
    rec("R::isValid", q.isValid() == p.isValid(),
        (x1 <= x2 && y1 <= y2) ? std::string("valid-side") : std::string("invalid-side"),
        in, bstr(q.isValid()), bstr(p.isValid()));

    rec("R::topLeft", same_pt(q.topLeft(), p.topLeft()), sh, in,
        qstr(q.topLeft()), qstr(p.topLeft()));
    rec("R::topRight", same_pt(q.topRight(), p.topRight()), sh, in,
        qstr(q.topRight()), qstr(p.topRight()));
    rec("R::bottomLeft", same_pt(q.bottomLeft(), p.bottomLeft()), sh, in,
        qstr(q.bottomLeft()), qstr(p.bottomLeft()));
    rec("R::bottomRight", same_pt(q.bottomRight(), p.bottomRight()), sh, in,
        qstr(q.bottomRight()), qstr(p.bottomRight()));

    // center 唯一会分家的形态是"x1+x2 越出 int"——那正是 qint64 中间量存在的理由
    const bool sumOv = (long long)x1 + x2 > INT_MAX || (long long)x1 + x2 < INT_MIN
                    || (long long)y1 + y2 > INT_MAX || (long long)y1 + y2 < INT_MIN;
    rec("R::center", same_pt(q.center(), p.center()),
        sumOv ? std::string("sum-out-of-int-range") : sh,
        in, qstr(q.center()), qstr(p.center()));

    // normalized 用专门的交换 tag（形态就是"这个轴交不交换"）
    rec("R::normalized", same_rect(q.normalized(), p.normalized()),
        "x-" + swapAxis(x1, x2) + "/y-" + swapAxis(y1, y2),
        in, qstr(q.normalized()), qstr(p.normalized()));

    // getRect / getCoords：四个出参逐个比（**两条各自一个 rec**，它们差一个 ±1）
    { int a1, b1, c1, d1, a2, b2, c2, d2;
      q.getRect(&a1, &b1, &c1, &d1); p.getRect(&a2, &b2, &c2, &d2);
      rec("R::getRect", a1 == a2 && b1 == b2 && c1 == c2 && d1 == d2, sh, in,
          rin(a1, b1, c1, d1), rin(a2, b2, c2, d2)); }
    { int a1, b1, c1, d1, a2, b2, c2, d2;
      q.getCoords(&a1, &b1, &c1, &d1); p.getCoords(&a2, &b2, &c2, &d2);
      rec("R::getCoords", a1 == a2 && b1 == b2 && c1 == c2 && d1 == d2, sh, in,
          rin(a1, b1, c1, d1), rin(a2, b2, c2, d2)); }
}

// 全部带标量/点/尺寸参数的修改器。**一个重载一条 rec**（规则三）：
// setLeft 与 setX 写的是同一个字段但是两个 API，moveTo(int,int) 与
// moveTo(PkPoint) 是两个重载，translate/translated 同理 —— 合并的话
// "坏掉的是哪一跳"在 DIFFTAG 里就看不见了。
static void cmp_rect_mutate(int x1, int y1, int x2, int y2, int a, int b)
{
    const QRect  q0 = mkQ(x1, y1, x2, y2);
    const PkRect p0 = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "+(" + istr(a) + "," + istr(b) + ")";
    const std::string sh = shapeOfRect(x1, y1, x2, y2) + "^" + argShape({a, b});

// 变参：`r.setRect(a, b, a, b)` 里的逗号会被预处理器当成实参分隔符，
// 写成 (label, expr) 那一版编不过。
#define PK_MUT2(label, ...)                                                   \
    do { QRect q2 = q0; PkRect p2 = p0;                                       \
         { QRect &r = q2; __VA_ARGS__; }                                      \
         { PkRect &r = p2; __VA_ARGS__; }                                     \
         rec(label, same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); } while (0)

    // ⚠ 这个宏把**同一段源码**分别在 QRect 与 PkRect 上跑一遍。它不是"少打字"：
    // 两侧写成两段代码时，改了一侧忘改另一侧就变成"对拍在比两件不同的事"，
    // 而那种错是静默的（Task 3 复评抓到过同型问题的另一种形态）。
    PK_MUT2("R::setLeft", r.setLeft(a));
    PK_MUT2("R::setTop", r.setTop(a));
    PK_MUT2("R::setRight", r.setRight(a));
    PK_MUT2("R::setBottom", r.setBottom(a));
    PK_MUT2("R::setX", r.setX(a));
    PK_MUT2("R::setY", r.setY(a));
    PK_MUT2("R::setWidth", r.setWidth(a));
    PK_MUT2("R::setHeight", r.setHeight(a));
    PK_MUT2("R::moveLeft", r.moveLeft(a));
    PK_MUT2("R::moveTop", r.moveTop(a));
    PK_MUT2("R::moveToXY", r.moveTo(a, b));
    PK_MUT2("R::setRect", r.setRect(a, b, a, b));
    PK_MUT2("R::setCoords", r.setCoords(a, b, a, b));
    PK_MUT2("R::translateXY", r.translate(a, b));
    PK_MUT2("R::translatedXY", r = r.translated(a, b));

    // 吃 QPoint / QSize 的那几个：两侧的实参类型不同，宏套不进来，逐条写。
    { QRect q2 = q0; PkRect p2 = p0; q2.setTopLeft(QPoint(a, b)); p2.setTopLeft(PkPoint(a, b));
      rec("R::setTopLeft", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.setBottomRight(QPoint(a, b)); p2.setBottomRight(PkPoint(a, b));
      rec("R::setBottomRight", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.moveTopLeft(QPoint(a, b)); p2.moveTopLeft(PkPoint(a, b));
      rec("R::moveTopLeft", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0;
      q2.moveCenter(QPoint(a, b)); p2.moveCenter(PkPoint(a, b));
      rec("R::moveCenter", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.moveTo(QPoint(a, b)); p2.moveTo(PkPoint(a, b));
      rec("R::moveToPoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.translate(QPoint(a, b)); p2.translate(PkPoint(a, b));
      rec("R::translatePoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRect q2 = q0.translated(QPoint(a, b));
      const PkRect p2 = p0.translated(PkPoint(a, b));
      rec("R::translatedPoint", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { QRect q2 = q0; PkRect p2 = p0; q2.setSize(QSize(a, b)); p2.setSize(PkSize(a, b));
      rec("R::setSize", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }

#undef PK_MUT2
}

// adjust / adjusted：四个增量，单独一层输入。
static void cmp_rect_adjust(int x1, int y1, int x2, int y2, int d1, int d2, int d3, int d4)
{
    const QRect  q0 = mkQ(x1, y1, x2, y2);
    const PkRect p0 = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "+" + rin(d1, d2, d3, d4);
    // 四个增量全参与形态判断（约定见 shapeOfD 上方那段）。溢出单独一档：
    // adjust 是裸的 int 加法，极值坐标上会回绕。
    const bool ov = (long long)x1 + d1 > INT_MAX || (long long)x1 + d1 < INT_MIN
                 || (long long)y1 + d2 > INT_MAX || (long long)y1 + d2 < INT_MIN
                 || (long long)x2 + d3 > INT_MAX || (long long)x2 + d3 < INT_MIN
                 || (long long)y2 + d4 > INT_MAX || (long long)y2 + d4 < INT_MIN;
    const std::string sh = (ov ? std::string("int-overflow")
                               : shapeOfRect(x1, y1, x2, y2))
                         + "^" + argShape({d1, d2, d3, d4});

    { QRect q2 = q0; PkRect p2 = p0; q2.adjust(d1, d2, d3, d4); p2.adjust(d1, d2, d3, d4);
      rec("R::adjust", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
    { const QRect q2 = q0.adjusted(d1, d2, d3, d4);
      const PkRect p2 = p0.adjusted(d1, d2, d3, d4);
      rec("R::adjusted", same_rect(q2, p2), sh, in, qstr(q2), qstr(p2)); }
}

// 双目：| & |= &= united intersected intersects contains(rect) == !=
static void cmp_rect_binary(int ax1, int ay1, int ax2, int ay2,
                            int bx1, int by1, int bx2, int by2)
{
    const QRect  qa = mkQ(ax1, ay1, ax2, ay2), qb = mkQ(bx1, by1, bx2, by2);
    const PkRect pa = mkP(ax1, ay1, ax2, ay2), pb = mkP(bx1, by1, bx2, by2);
    const std::string in = rin(ax1, ay1, ax2, ay2) + "|" + rin(bx1, by1, bx2, by2);
    const std::string tag = pairTag(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);

    rec("R::operator|", same_rect(qa | qb, pa | pb), tag, in,
        qstr(qa | qb), qstr(pa | pb));
    rec("R::operator&", same_rect(qa & qb, pa & pb), tag, in,
        qstr(qa & qb), qstr(pa & pb));
    // ⚠ united / intersected 在 Qt 里是 `*this | r` / `*this & r` 的转发，
    // **仍然各写一条 rec**：规则三说的正是"转发链上坏掉的可能是转发那一跳"。
    rec("R::united", same_rect(qa.united(qb), pa.united(pb)), tag, in,
        qstr(qa.united(qb)), qstr(pa.united(pb)));
    rec("R::intersected", same_rect(qa.intersected(qb), pa.intersected(pb)), tag, in,
        qstr(qa.intersected(qb)), qstr(pa.intersected(pb)));
    { QRect q2 = qa; PkRect p2 = pa; q2 |= qb; p2 |= pb;
      rec("R::operator|=", same_rect(q2, p2), tag, in, qstr(q2), qstr(p2)); }
    { QRect q2 = qa; PkRect p2 = pa; q2 &= qb; p2 &= pb;
      rec("R::operator&=", same_rect(q2, p2), tag, in, qstr(q2), qstr(p2)); }

    rec("R::intersects", qa.intersects(qb) == pa.intersects(pb), tag, in,
        bstr(qa.intersects(qb)), bstr(pa.intersects(pb)));
    // contains(rect) 的两个 proper 取值走的是两条不同分支（`<` vs `<=`），
    // 各一条 rec，且 proper 参与 tag。
    rec("R::containsRect", qa.contains(qb) == pa.contains(pb), tag + "/proper-false", in,
        bstr(qa.contains(qb)), bstr(pa.contains(pb)));
    rec("R::containsRectProper", qa.contains(qb, true) == pa.contains(pb, true),
        tag + "/proper-true", in,
        bstr(qa.contains(qb, true)), bstr(pa.contains(pb, true)));

    // == / != 与相对位置无关，用形态对 + "坐标是否逐个相等"
    const std::string eqtag = shapeOfRect(ax1, ay1, ax2, ay2) + "^"
                            + shapeOfRect(bx1, by1, bx2, by2) + "/"
                            + ((ax1 == bx1 && ay1 == by1 && ax2 == bx2 && ay2 == by2)
                               ? "same-coords" : "differ");
    rec("R::operator==", (qa == qb) == (pa == pb), eqtag, in,
        bstr(qa == qb), bstr(pa == pb));
    rec("R::operator!=", (qa != qb) == (pa != pb), eqtag, in,
        bstr(qa != qb), bstr(pa != pb));
}

// contains(point) 的三个重载 + proper。四条各一个 rec（规则三）：
// contains(int,int) 与 contains(int,int,bool) 是两个独立的重载，
// 都转发到 contains(QPoint,bool)，但**转发那一跳本身可能坏**。
static void cmp_rect_contains_point(int x1, int y1, int x2, int y2, int px, int py)
{
    const QRect  q = mkQ(x1, y1, x2, y2);
    const PkRect p = mkP(x1, y1, x2, y2);
    const std::string in = rin(x1, y1, x2, y2) + "@(" + istr(px) + "," + istr(py) + ")";
    const std::string tag = "x-" + edgeTag(x1, x2, px) + "/y-" + edgeTag(y1, y2, py);

    rec("R::containsPoint", q.contains(QPoint(px, py)) == p.contains(PkPoint(px, py)),
        tag + "/proper-false", in,
        bstr(q.contains(QPoint(px, py))), bstr(p.contains(PkPoint(px, py))));
    rec("R::containsPointProper",
        q.contains(QPoint(px, py), true) == p.contains(PkPoint(px, py), true),
        tag + "/proper-true", in,
        bstr(q.contains(QPoint(px, py), true)), bstr(p.contains(PkPoint(px, py), true)));
    rec("R::containsXY", q.contains(px, py) == p.contains(px, py), tag + "/proper-false", in,
        bstr(q.contains(px, py)), bstr(p.contains(px, py)));
    rec("R::containsXYProper",
        q.contains(px, py, true) == p.contains(px, py, true), tag + "/proper-true", in,
        bstr(q.contains(px, py, true)), bstr(p.contains(px, py, true)));
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

// ── Group 3：Rect 族的坐标 token ──────────────────────────────────────────
//
// 矩形是**四元**的（一元 API 4 个分量、双目 API 8 个），44⁴ / 44⁸ 那种做满显然
// 做不了，所以按 README 那条约定做「密集小域 + 极值域 + 手挑集」三层交叉，
// 保证**每个分量位上都取得到极值与手挑值**（不是索引轮转）。
//
// 密集小域：6 个 token → 6⁴ = 1 296 个矩形。双目做满 1 296² ≈ 168 万组。
// 这批值覆盖了 x2 相对 x1 的全部关键位置（-1 = 宽 0、-2 = 宽 -1 要交换、
// 0 = 宽 1、+1/+2/+3 = 正常宽）。
static const int kRectTok6[] = { -2, -1, 0, 1, 2, 3 };
// 极值域：7 个 token → 7⁴ = 2 401 个矩形。只跑一元/修改器，双目太大。
static const int kRectTokX[] = { INT_MIN, INT_MIN + 1, -1, 0, 1, INT_MAX - 1, INT_MAX };
// 极值域的双目版：5 个 token → 5⁴ = 625 个矩形，双目做满 625² ≈ 39 万组。
static const int kRectTokE[] = { INT_MIN, -1, 0, 1, INT_MAX };
// 修改器/点的实参 token（含极值，让 setLeft(INT_MIN) 这类走到）
static const int kRectArg[] = { -2, -1, 0, 1, INT_MIN, INT_MAX };
// adjust 的增量：计划点名的 {-2,-1,0,1,2}⁴ = 625 组
static const int kAdjD[] = { -2, -1, 0, 1, 2 };
// adjust 的矩形底座（4⁴ = 256，再乘 625 已经是 16 万组）
static const int kRectTok4[] = { -1, 0, 1, 2 };

// 手挑对抗矩形，**以内部坐标给出**（(l,t,w,h) 构造够不到其中一大半形态）。
// 每一条都对应一个已实测的语义分界，注释写的就是它守着哪一条。
static const int kHandRect[][4] = {
    {  0,  0, -1, -1 },                       // QRect()：null，| 的偏心分支
    {  5,  5,  4,  4 },                       // (5,5,0,0)：null 但不在原点
    {  0,  0,  0,  0 },                       // (0,0,1,1)：最小的非空矩形
    {  0,  0,  9,  9 },                       // (0,0,10,10)：差一的标准样本
    {  0,  0, -2, -2 },                       // (0,0,-1,-1)：empty 非 null，要交换
    {  5,  5,  1,  1 },                       // (5,5,-3,-3)：负宽高且不在原点
    { 20, 20, 24, 24 },                       // 与上面几个完全不相交
    {  0,  0, -1,  0 },                       // 宽恰为 0：normalized **不**交换的边界
    {  0,  0, -2,  0 },                       // 宽 -1：交换边界的另一侧
    {  0,  0, -3,  0 },                       // 宽 -2
    {  0,  0,  9, -1 },                       // (0,0,10,0)：一边退化
    { INT_MIN, INT_MIN, INT_MIN, INT_MIN },   // (INT_MIN,INT_MIN,1,1)
    {  0,  0, INT_MAX - 1, INT_MAX - 1 },     // (0,0,INT_MAX,INT_MAX)
    {  0,  0, INT_MAX, INT_MAX },             // 跨距溢出
    { INT_MIN, INT_MIN, INT_MAX, INT_MAX },   // 跨距溢出到 0，center 靠 qint64
    { INT_MAX, INT_MAX, INT_MIN, INT_MIN },   // 反向极值：要交换且溢出
};

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

    // ── Group 1：手挑对抗用例的**全组合**（不是索引轮转）──────────────────
    //
    // ⚠ **Task 3–6 照抄这个形状。** 第一版写的是 `x = kHand[i], y = kHand[(i+1)%n]`
    // 这种索引轮转，44 个手挑值只产生 44 个点而不是 44² 个 —— 后果是
    // **kHand 里那批 kTok 没有的值永远无法同时出现在 x 和 y 上**
    //（0.5000000000000001、1e-323、2.2250738585072014e-308、2147483647/8.0、
    //  -2147483649.0、4294967296.0、1.0+1e-13、2e-12、±0.25、±3.5 …）。
    // 复评实测：注入一个只在 `x == y == 2147483648.0`（手挑独有值）时触发的
    // toPoint 缺陷，对拍**全绿 exit=0**，连跑两遍一致。旁证：manhattanLength 用
    // fabs 的真缺陷全场只抓到 1 次，落在 (-0.0,-0.0) —— 纯粹因为 -0.0 恰好也在
    // kTok 里；若它只在手挑集里，那个真缺陷会全绿溜过。
    //
    // Point 是二元的（一元 API 2 个分量、二元 API 4 个），所以这里直接做满：
    // 一元 44²（double）/ 25²（int），二元 44⁴ / 25⁴ —— **数字以 kHandD/kHandI
    // 的实际元素个数为准**（countOf 算的，44 与 25）。**分量更多时不要退回轮转**
    // —— Rect 的二元 API 有 8 个分量、44⁸ 显然做不了，那就至少做
    //「手挑 × token」的交叉（手挑值必须能出现在每一个分量位上），
    // 绝不能让某个分量位永远取不到手挑值。Size 族的 scaled 就是这么做的
    //（4 个分量 + mode，见下面 Size 那几段）。
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            cmp_pointf_unary(kHandD[i], kHandD[j]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int k = 0; k < nHandD; ++k)
                for (int l = 0; l < nHandD; ++l)
                    cmp_pointf_binary(kHandD[i], kHandD[j], kHandD[k], kHandD[l]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_pointf_scale(kHandD[i], kHandD[j], kFacD[f]);

    cmp_point_constants();
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            cmp_point_unary(kHandI[i], kHandI[j]);
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int k = 0; k < nHandI; ++k)
                for (int l = 0; l < nHandI; ++l)
                    cmp_point_binary(kHandI[i], kHandI[j], kHandI[k], kHandI[l]);
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j) {
            for (int f = 0; f < nFacD; ++f) cmp_point_scale_double(kHandI[i], kHandI[j], kFacD[f]);
            for (int f = 0; f < nFacF; ++f) cmp_point_scale_float(kHandI[i], kHandI[j], kFacF[f]);
            for (int f = 0; f < nFacI; ++f) cmp_point_scale_int(kHandI[i], kHandI[j], kFacI[f]);
        }
    // 跨类型：整数点全组合 × 浮点分量全组合。
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int a = 0; a < nHandD; ++a)
                for (int b = 0; b < nHandD; ++b)
                    cmp_promotion(kHandI[i], kHandI[j], kHandD[a], kHandD[b]);

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

    // ═══ Size 族 ═══════════════════════════════════════════════════════════
    // 输入集与 Point 族**共用**（kHandI/kTokI/kHandD/kTokD/kFacD）：尺寸的
    // 关键边界（0、±1、INT_MIN/MAX、±0.0、nan、inf、次正规）它们都覆盖到了，
    // 另起一套只会多一份要维护的东西。
    cmp_size_constants();

    // 一元：手挑² + token²（与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            cmp_size_unary(kHandI[i], kHandI[j]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            cmp_size_unary(kTokI[a], kTokI[b]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            cmp_sizef_unary(kHandD[i], kHandD[j]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            cmp_sizef_unary(kTokD[a], kTokD[b]);

    // 二元：手挑⁴ + token⁴（做满，与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int k = 0; k < nHandI; ++k)
                for (int l = 0; l < nHandI; ++l)
                    cmp_size_binary(kHandI[i], kHandI[j], kHandI[k], kHandI[l]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int c = 0; c < nTokI; ++c)
                for (int d = 0; d < nTokI; ++d)
                    cmp_size_binary(kTokI[a], kTokI[b], kTokI[c], kTokI[d]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int k = 0; k < nHandD; ++k)
                for (int l = 0; l < nHandD; ++l)
                    cmp_sizef_binary(kHandD[i], kHandD[j], kHandD[k], kHandD[l]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int c = 0; c < nTokD; ++c)
                for (int d = 0; d < nTokD; ++d)
                    cmp_sizef_binary(kTokD[a], kTokD[b], kTokD[c], kTokD[d]);

    // 带标量参数的缩放：尺寸两层 + 参数一层（与 Point 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_size_scale(kHandI[i], kHandI[j], kFacD[f]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_size_scale(kTokI[a], kTokI[b], kFacD[f]);
    for (int i = 0; i < nHandD; ++i)
        for (int j = 0; j < nHandD; ++j)
            for (int f = 0; f < nFacD; ++f)
                cmp_sizef_scale(kHandD[i], kHandD[j], kFacD[f]);
    for (int a = 0; a < nTokD; ++a)
        for (int b = 0; b < nTokD; ++b)
            for (int f = 0; f < nFacD; ++f)
                cmp_sizef_scale(kTokD[a], kTokD[b], kFacD[f]);

    // ── scaled/scale：4 个分量 + mode，做不了 25⁴×25⁴ ─────────────────────
    // 按 README 那条约定做**「手挑 × token」双向交叉**：源取手挑对、目标取
    // token 对，再反过来一遍 —— 于是**每个分量位上都取得到手挑值**，
    // 而不是像索引轮转那样让某些手挑值永远进不了某个位置。
    // 规模：int 25²×20²×2×3 模式 = 1 500 000 组；double 44²×21²×2×3 = 5 122 656 组。
    for (int mi = 0; mi < 3; ++mi) {
        for (int i = 0; i < nHandI; ++i)
            for (int j = 0; j < nHandI; ++j)
                for (int a = 0; a < nTokI; ++a)
                    for (int b = 0; b < nTokI; ++b) {
                        cmp_size_scaled(kHandI[i], kHandI[j], kTokI[a], kTokI[b], mi);
                        cmp_size_scaled(kTokI[a], kTokI[b], kHandI[i], kHandI[j], mi);
                    }
        for (int i = 0; i < nHandD; ++i)
            for (int j = 0; j < nHandD; ++j)
                for (int a = 0; a < nTokD; ++a)
                    for (int b = 0; b < nTokD; ++b) {
                        cmp_sizef_scaled(kHandD[i], kHandD[j], kTokD[a], kTokD[b], mi);
                        cmp_sizef_scaled(kTokD[a], kTokD[b], kHandD[i], kHandD[j], mi);
                    }
    }

    // 跨类型：整数尺寸全组合 × 浮点分量全组合（与 Point 的 cmp_promotion 同形）
    for (int i = 0; i < nHandI; ++i)
        for (int j = 0; j < nHandI; ++j)
            for (int a = 0; a < nHandD; ++a)
                for (int b = 0; b < nHandD; ++b)
                    cmp_size_promotion(kHandI[i], kHandI[j], kHandD[a], kHandD[b]);
    for (int a = 0; a < nTokI; ++a)
        for (int b = 0; b < nTokI; ++b)
            for (int d = 0; d < nTokD; ++d)
                cmp_size_promotion(kTokI[a], kTokI[b], kTokD[d], kTokD[(d + 3) % nTokD]);

    // ═══ Rect 族（整数）═══════════════════════════════════════════════════
    const int nR6 = countOf(kRectTok6), nRX = countOf(kRectTokX);
    const int nRE = countOf(kRectTokE), nR4 = countOf(kRectTok4);
    const int nArg = countOf(kRectArg), nAdj = countOf(kAdjD);
    const int nHR = countOf(kHandRect);

    cmp_rect_constants();

    // 构造函数：8 个 token 四层做满（计划点名的那 8 个）
    {
        static const int kCtorTok[] = { -2, -1, 0, 1, 2, 3, INT_MIN, INT_MAX };
        const int n = countOf(kCtorTok);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k)
                    for (int l = 0; l < n; ++l)
                        cmp_rect_ctor(kCtorTok[i], kCtorTok[j], kCtorTok[k], kCtorTok[l]);
    }

    // ── 一元：密集域 1 296 + 极值域 2 401 + 手挑 16 ──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    cmp_rect_unary(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
    for (int i = 0; i < nRX; ++i)
        for (int j = 0; j < nRX; ++j)
            for (int k = 0; k < nRX; ++k)
                for (int l = 0; l < nRX; ++l)
                    cmp_rect_unary(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
    for (int h = 0; h < nHR; ++h)
        cmp_rect_unary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3]);

    // ── 修改器：矩形三层 × 实参 6² ──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    for (int a = 0; a < nArg; ++a)
                        for (int b = 0; b < nArg; ++b)
                            cmp_rect_mutate(kRectTok6[i], kRectTok6[j], kRectTok6[k],
                                            kRectTok6[l], kRectArg[a], kRectArg[b]);
    for (int i = 0; i < nRE; ++i)
        for (int j = 0; j < nRE; ++j)
            for (int k = 0; k < nRE; ++k)
                for (int l = 0; l < nRE; ++l)
                    for (int a = 0; a < nArg; ++a)
                        for (int b = 0; b < nArg; ++b)
                            cmp_rect_mutate(kRectTokE[i], kRectTokE[j], kRectTokE[k],
                                            kRectTokE[l], kRectArg[a], kRectArg[b]);
    for (int h = 0; h < nHR; ++h)
        for (int a = 0; a < nArg; ++a)
            for (int b = 0; b < nArg; ++b)
                cmp_rect_mutate(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                kHandRect[h][3], kRectArg[a], kRectArg[b]);

    // ── adjust：矩形 4⁴ / 手挑 × 增量 5⁴ ──
    for (int i = 0; i < nR4; ++i)
        for (int j = 0; j < nR4; ++j)
            for (int k = 0; k < nR4; ++k)
                for (int l = 0; l < nR4; ++l)
                    for (int a = 0; a < nAdj; ++a)
                        for (int b = 0; b < nAdj; ++b)
                            for (int c = 0; c < nAdj; ++c)
                                for (int d = 0; d < nAdj; ++d)
                                    cmp_rect_adjust(kRectTok4[i], kRectTok4[j], kRectTok4[k],
                                                    kRectTok4[l], kAdjD[a], kAdjD[b],
                                                    kAdjD[c], kAdjD[d]);
    for (int h = 0; h < nHR; ++h)
        for (int a = 0; a < nAdj; ++a)
            for (int b = 0; b < nAdj; ++b)
                for (int c = 0; c < nAdj; ++c)
                    for (int d = 0; d < nAdj; ++d)
                        cmp_rect_adjust(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3], kAdjD[a], kAdjD[b], kAdjD[c], kAdjD[d]);

    // ── contains(point)：点坐标由**矩形自己的边界**导出（l-1/l/l+1/r-1/r/r+1），
    // 这样差一边界在每个矩形上都真的被踩到；固定 token 集做不到这一点。
    {
        auto pointsAround = [](int lo, int hi, long long out[6]) {
            out[0] = (long long)lo - 1; out[1] = lo; out[2] = (long long)lo + 1;
            out[3] = (long long)hi - 1; out[4] = hi; out[5] = (long long)hi + 1;
        };
        auto runContains = [&](int x1, int y1, int x2, int y2) {
            long long xs[6], ys[6];
            pointsAround(x1, x2, xs);
            pointsAround(y1, y2, ys);
            for (int a = 0; a < 6; ++a)
                for (int b = 0; b < 6; ++b) {
                    // 导出值可能越出 int（lo-1 在 INT_MIN 上）——夹回值域，
                    // 越界的那一格由极值 token 自己覆盖。
                    if (xs[a] < INT_MIN || xs[a] > INT_MAX) continue;
                    if (ys[b] < INT_MIN || ys[b] > INT_MAX) continue;
                    cmp_rect_contains_point(x1, y1, x2, y2, (int)xs[a], (int)ys[b]);
                }
        };
        for (int i = 0; i < nR6; ++i)
            for (int j = 0; j < nR6; ++j)
                for (int k = 0; k < nR6; ++k)
                    for (int l = 0; l < nR6; ++l)
                        runContains(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
        for (int i = 0; i < nRX; ++i)
            for (int j = 0; j < nRX; ++j)
                for (int k = 0; k < nRX; ++k)
                    for (int l = 0; l < nRX; ++l)
                        runContains(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
        for (int h = 0; h < nHR; ++h)
            runContains(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3]);
    }

    // ── 双目：密集域做满 1 296²，极值域做满 625²，再加手挑 × 两个域的
    // **双向**交叉（手挑值必须能出现在 a 侧和 b 侧的每一个分量位上）──
    for (int i = 0; i < nR6; ++i)
        for (int j = 0; j < nR6; ++j)
            for (int k = 0; k < nR6; ++k)
                for (int l = 0; l < nR6; ++l)
                    for (int m = 0; m < nR6; ++m)
                        for (int n = 0; n < nR6; ++n)
                            for (int o = 0; o < nR6; ++o)
                                for (int p = 0; p < nR6; ++p)
                                    cmp_rect_binary(kRectTok6[i], kRectTok6[j], kRectTok6[k],
                                                    kRectTok6[l], kRectTok6[m], kRectTok6[n],
                                                    kRectTok6[o], kRectTok6[p]);
    for (int i = 0; i < nRE; ++i)
        for (int j = 0; j < nRE; ++j)
            for (int k = 0; k < nRE; ++k)
                for (int l = 0; l < nRE; ++l)
                    for (int m = 0; m < nRE; ++m)
                        for (int n = 0; n < nRE; ++n)
                            for (int o = 0; o < nRE; ++o)
                                for (int p = 0; p < nRE; ++p)
                                    cmp_rect_binary(kRectTokE[i], kRectTokE[j], kRectTokE[k],
                                                    kRectTokE[l], kRectTokE[m], kRectTokE[n],
                                                    kRectTokE[o], kRectTokE[p]);
    for (int h = 0; h < nHR; ++h) {
        for (int i = 0; i < nR6; ++i)
            for (int j = 0; j < nR6; ++j)
                for (int k = 0; k < nR6; ++k)
                    for (int l = 0; l < nR6; ++l) {
                        cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3],
                                        kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l]);
                        cmp_rect_binary(kRectTok6[i], kRectTok6[j], kRectTok6[k], kRectTok6[l],
                                        kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3]);
                    }
        for (int i = 0; i < nRX; ++i)
            for (int j = 0; j < nRX; ++j)
                for (int k = 0; k < nRX; ++k)
                    for (int l = 0; l < nRX; ++l) {
                        cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3],
                                        kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l]);
                        cmp_rect_binary(kRectTokX[i], kRectTokX[j], kRectTokX[k], kRectTokX[l],
                                        kHandRect[h][0], kHandRect[h][1], kHandRect[h][2],
                                        kHandRect[h][3]);
                    }
        for (int g = 0; g < nHR; ++g)
            cmp_rect_binary(kHandRect[h][0], kHandRect[h][1], kHandRect[h][2], kHandRect[h][3],
                            kHandRect[g][0], kHandRect[g][1], kHandRect[g][2], kHandRect[g][3]);
    }

    for (const auto &kv : g_tags)
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    // 规则三的机器闸门：见 g_apis 上方那段。run_oracle.sh 拿这批行与
    // oracle/api_seen.expected、oracle/rect_api.map 三向核对。
    for (const auto &a : g_apis)
        std::printf("APISEEN %s\n", a.c_str());
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 已声明的偏离不算失败；判定在 run_oracle.sh
}
