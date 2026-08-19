// global_difftest.cpp —— pk/global 标量函数与真 Qt5 的逐输入对拍。
//
// 抄自 pk/geometry/oracle/geometry_difftest.cpp（R-03）的骨架：**单 TU 双侧**，
// Q 侧在全局作用域 include 真 Qt 头，P 侧塞进 `namespace pkoracle`。
//
// ── 输出契约（run_oracle.sh 读这些行，别的都是给人看的）────────────────
//     DIFF total=<N> mismatch=<M>      恰好一行，程序末尾打
//     DIFFTAG <api> <tag> <count>      一类差异一行（Q_ASSERT 登记行也是它）
//     DIFFDEN <api> <tag> <分母>        分母行，给人读「命中 N 次里分家 M 次」
//     APISEEN <api>                    规则三机器闸门，与 api_seen.expected 核对
// **退出码必须是 0，即使 M>0** —— 已声明的偏离不算失败，退出码只表示"跑完没崩"。
//
// ── 为什么替代品要塞进 namespace pkoracle ──────────────────────────────
// PkGlobal.h 与 Qt 的 qglobal.h 在**同一个全局作用域**里定义了签名完全相同的
// qAbs / qRound / qMin / qMax / qBound / qFuzzyCompare / qFuzzyIsNull —— 两个头
// 直接进同一个 TU 是硬性的重定义错误。解法：`namespace pkoracle { #include
// "PkGlobal.h" }`。PkGlobal.h 只 #include <cmath>/<limits>，这两个必须由本文件
// **在 namespace 之外先 include**（include guard 让 namespace 里的二次 include
// 空转），否则 std 会被卷进 pkoracle::std。
// 顺带：PkGlobal.h 的「让位」机制（PK_GLOBAL_SCALARS_FROM_*）在 oracle 里不触发
// —— 真 Qt 的 qFuzzyCompare/qFuzzyIsNull 是函数不是宏，`defined(qFuzzyCompare)`
// 为假；pk/geometry 的 include guard 更不会出现。于是 pkoracle:: 里是**全量**标量。
//
// ── 两侧真的各链各的吗 ────────────────────────────────────────────────
// Q 侧 :qFloor/:qIsNaN/:qInf/:qQNaN 是 Q_CORE_EXPORT 的非 inline 函数，定义在
// libQt5Core.so 里 —— run_oracle.sh 用 ldd 必须看到 libQt5Core（链上别的东西或
// 只链到替代品，ldd 查不出来就 FAIL）。P 侧是 pkoracle:: 里的 constexpr inline，
// 编译时全部内联，零 Qt。
//
// ── 跑出 mismatch=0 是警报，不是好消息 ─────────────────────────────────
// 与 R-03 相同：PkGlobal 逐字照抄 Qt 5.15.7，真实差异应该为零。判别力靠两件事：
//   ① **canary**：每 API 一条故意不相等的比对，走与真实 API 完全相同的
//      rec()/比较原语/tag 路径。它们必须出现在输出里 —— 少一条说明比较管道被
//      写死/被优化掉/tag 构造断了，run_oracle.sh 直接 FAIL。
//   ② **注入自证**：往 PkGlobal.h 里塞真 bug，必须产生**未声明**的 tag。
// **已登记的真实偏离：无** —— 修复轮随 PkGlobal.h 修正消除了 qFloor/qCeil 在 ±inf
// 上的分家（对非有限值原样返回 int(v)=INT_MIN，与 Qt 运行期行为一致）。非有限输入
// 的跨侧比较本身不可靠（int(±inf) 是 UB，见 cmp_floorceil 注释），改为钉 INT_MIN。
//
// ── tag 的两条硬规则 ──────────────────────────────────────────────────
//   规则一：tag 由「触发这次差异的输入形态」参与构造，不能是每 API 一个字面量。
//           unary API 的 tag 只看它自己的输入；binary/ternary 看全部实参。
//   规则二：谓词不能比 .deviation 里那句理由宽（预期零差异 → 一律严格
//           位精确 same_double / 整数 == / bool ==，不用任何容差）。
//           ⚠ 浮点结果用**位精确**而不是 `==`：`==` 会把 +0.0/-0.0 判等（qAbs
//           的 signbit、qMin 的 ±0 相等分支恰恰要盯这两类），把 NaN 判不等
//           （两侧同一输入派生的 NaN 载荷可能不同，必须按 NaN==NaN 判同）。
//           `==` 只用于整数与 bool 返回。
//   规则三：每一个已实现的重载都要有自己的 rec()，一个都不许合并。落地方式：
//           oracle/api_seen.expected 列全量（人维护），APISEEN 与它对账（机器抓）。

// ── 真 Qt 侧 + 系统头（都必须在 namespace 之外）────────────────────────
#include <QtGlobal>
#include <QtCore/qmath.h>
#include <QtCore/qalgorithms.h>
#include <QtCore/qnumeric.h>

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

// pk/test 的 compat/QtGlobal 会把 qFuzzyCompare/qFuzzyIsNull 定义成**宏**
//（→ pkFuzzyCompare/pkFuzzyIsNull）。若 compat/ 混进 -I，`<QtGlobal>` 会解析到
// 那份垫片，本文件里所有 `::qFuzzyCompare(...)` 会被预处理器当场改写，P 侧的
// `pkoracle::qFuzzyCompare` 也因 PkGlobal.h 的让位机制消失 → 编译失败。下面这
// 道 #error 只是把失败点提前、说得更明白；run_oracle.sh 的 -I 检查才是第一道。
#if defined(qFuzzyCompare) || defined(qFuzzyIsNull)
#  error "对拍源被 pk/test 的 compat/QtGlobal 的 #define 改写 —— -I 里混进了 compat/"
#endif

// ── 替代品侧 ───────────────────────────────────────────────────────────
namespace pkoracle {
#include "PkGlobal.h"
}

// ── 计数与记录（照 R-03 骨架，六件事不要改）────────────────────────────
static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags;   // "<api> <tag>" -> **分家**次数（分子）
static std::map<std::string, long> g_tag_seen; // 分母：该 tag 的谓词命中过多少次比对
static long g_printed = 0;
static std::set<std::string> g_apis;          // 规则三闸门：见 api_seen.expected

static void rec(const char *api, bool same, const std::string &tag,
                const std::string &in, const std::string &qs, const std::string &ps)
{
    ++g_total;
    g_apis.insert(api);
    const std::string key = std::string(api) + " " + tag;
    ++g_tag_seen[key];              // 分母：谓词命中就记，不管同不同
    if (same) return;
    ++g_mismatch;
    ++g_tags[key];                  // 分子：只记分家
    if (g_printed < 40) {           // 明细只打前 40 条，run_oracle.sh 不读它
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api, tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

template <typename T, size_t N> static size_t countOf(const T (&)[N]) { return N; }

// ═══ 比较原语 ══════════════════════════════════════════════════════════
// **位精确**比较 double：`==` 会把 +0/-0 判等、把 NaN 判不等，两者都不是我们要的。
// 标量函数的 signbit 是会真传播的（qAbs(-0.0)、qMin 的 ±0 相等分支、qRound 的
// 0.5 进位），用 `==` 比等于把这一整类差异永久豁免。
static bool same_double(double a, double b)
{
    std::uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);    // NaN 的位模式两侧可不同，都是 NaN 就算同
}
static bool same_float(float a, float b)
{
    std::uint32_t ba, bb;
    std::memcpy(&ba, &a, sizeof ba);
    std::memcpy(&bb, &b, sizeof bb);
    if (ba == bb) return true;
    return (a != a) && (b != b);
}

static std::string dstr(double d)
{
    std::uint64_t b; std::memcpy(&b, &d, sizeof b);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g(0x%016llx)", d, (unsigned long long)b);
    return buf;
}
static std::string fstr(float f)
{
    std::uint32_t b; std::memcpy(&b, &f, sizeof b);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.9g(0x%08x)", f, (unsigned)b);
    return buf;
}
// ⚠ **别用 std::to_string 打印 int 结果**。本文件的输入集特意包含会触发
// float→int 越界（int(1e300)、int(±inf)）的 qRound/qFloor 输入 —— 那是 UB，
// -fwrapv 管不着。std::to_string 是内联实现，编进本 TU 后编译器拿「转换越界即
// UB」当前提，对结果的打印可能做任意推理。snprintf 是外部调用（libc），把 UB
// 结果的**实际寄存器值**照实印出来，与 %d 逐位一致。两侧的 qRound/qFloor 公式
// 逐字相同，越界结果一致，比较 `q == p` 不受影响。
static std::string istr(long long v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", v);
    return buf;
}
static std::string ustr(unsigned long long v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%llu", v);
    return buf;
}
static std::string bstr(bool b) { return b ? "true" : "false"; }

static bool isNegZero(double d) { return d == 0.0 && std::signbit(d); }
static bool isNegZero(float f)  { return f == 0.0f && std::signbit(f); }

// ═══ 输入宇宙 ══════════════════════════════════════════════════════════
// brief 的 10-token 组合爆破集 {NaN, -0.0, 0.0, 1.0, -1.0, 0.5, -0.5,
// 0.49999999999999994, 1e300, 2.0} + 上表手挑对抗值，合并成一个 double 宇宙。
// **必须来自运行期全局数组**：标量函数里 float→int 越界（int(inf)）与 INT_MIN
// 取负是 UB，编译期常量折叠给的是另一个答案（R-03 实测），运行期数组天然不被折叠。
static const double kD[] = {
    NAN, INFINITY, -INFINITY,
    -0.0, 0.0,
    0.5, -0.5, 1.5, -1.5, 2.5, -2.5,
    1.0, -1.0, 2.0, -2.0, 2.7, -2.7, 3.0, 5.0,
    0.49999999999999994,            // double 半值下界（int(d+0.5) 会进位到 1）
    1e-13, -1e-13, 1e-12, -1e-12, 1e-11, -1e-11,
    1e300, -1e300,                   // qRound 越 int 界
    5e-324,                          // 最小 double subnormal
    2147483647.0, -2147483648.0,     // INT_MAX / INT_MIN 的 double 形态
    1.000000000001,                  // qFuzzyCompare 相对阈值边界对（Qt 超阈为假）
    1.0000005,                       // 相对差 5e-7：Qt 阈(1e-12)下假，注入②(1e-6)下真
    2147483647.5, -2147483648.5,     // qRound 恰在 int 界外 0.5
};
static const float kF[] = {
    NAN, INFINITY, -INFINITY,
    -0.0f, 0.0f,
    0.5f, -0.5f, 1.5f, -1.5f, 2.5f, -2.5f,
    1.0f, -1.0f, 2.0f, -2.0f, 2.7f, -2.7f, 3.0f,
    0.49999997f,                     // float 半值下界
    1e-13f, -1e-13f, 1e-12f, -1e-12f, 1e-11f, -1e-11f,
    1e30f, -1e30f,                   // float 大值
    1.4e-45f,                        // 最小 float subnormal
    1.00001f,                        // float 相对阈值边界对
};
static const int kI[] = {
    0, 1, -1, 2, -2, 127, 128, 129,
    100000, -100000, INT_MIN, INT_MIN + 1, INT_MAX, INT_MAX - 1,
};
static const qint64 kI64[] = {
    0, 1, -1, 2, -2, 127, 128, 129,
    (qint64)INT_MIN, (qint64)INT_MAX,
    (qint64)0x7fffffffffffffffLL, (qint64)(-0x7fffffffffffffffLL - 1), // LLONG_MAX / LLONG_MIN
};
static const quint32 kU32[] = {
    0, 1, 2, 3, 127, 128, 129, 0x7fffffffu, 0x80000000u, 0xffffffffu,
};

// ═══ tag 构造（规则一：由输入形态参与构造）════════════════════════════
static std::string tagAbsD(double v)
{
    if (std::isnan(v)) return "abs_nan";
    if (std::isinf(v)) return "abs_inf";
    if (v == 0.0) return "abs_negzero";        // ±0.0 都算零号（signbit 必须一致）
    if (v == 0.49999999999999994) return "abs_half_ulp";
    return "abs_ordinary";
}
static std::string tagAbsF(float v)
{
    if (std::isnan(v)) return "abs_nan";
    if (std::isinf(v)) return "abs_inf";
    if (v == 0.0f) return "abs_negzero";
    if (v == 0.49999997f) return "abs_half_ulp";
    return "abs_ordinary";
}
static std::string tagMinMaxD(double a, double b)
{
    if (std::isnan(a)) return "min_nan_left";
    if (std::isnan(b)) return "min_nan_right";
    if (a == b) return "min_equal";            // 含 (-0.0, +0.0) 这类 == 但符号不同的对
    if (isNegZero(a)) return "min_negzero_left";
    if (isNegZero(b)) return "min_negzero_right";
    return "min_ordinary";
}
static std::string tagBoundD(double mn, double val, double mx)
{
    if (std::isnan(mn) || std::isnan(val) || std::isnan(mx)) return "bound_nan";
    if (isNegZero(mn) || isNegZero(val) || isNegZero(mx)) return "bound_negzero";
    if (mn > mx) return "bound_reversed";
    if (val < mn) return "bound_clamp_low";
    if (val > mx) return "bound_clamp_high";
    if (val == mn || val == mx) return "bound_clamp_equal";
    return "bound_ordinary";
}
static std::string tagRoundD(double v)
{
    if (std::isnan(v)) return "round_nan";
    if (std::isinf(v)) return "round_inf";
    if (v == 0.49999999999999994) return "round_ulp_half";
    double ip; double fp = std::modf(v, &ip);
    if (std::fabs(fp) == 0.5) return (v > 0) ? "round_half_pos" : "round_half_neg";
    if (v >= 1e300 || v <= -1e300) return "round_overflow";
    return "round_ordinary";
}
static std::string tagFuzzyD(double p1, double p2)
{
    if (std::isnan(p1) || std::isnan(p2)) return "fuzzy_nan";
    if (std::isinf(p1) || std::isinf(p2)) return "fuzzy_inf";
    if (p1 == 0.0 && p2 == 0.0) return "fuzzy_zero_zero";
    if ((p1 == 0.0 && p2 == 1e-13) || (p1 == 1e-13 && p2 == 0.0)) return "fuzzy_zero_near";
    if (p1 == p2) return "fuzzy_equal";
    // 「阈值邻域」形态：两值相对差在 1e-12 与 1e-6 之间的对抗对（注入②的判别输入）。
    if ((p1 == 1.0 && (p2 == 1.000000000001 || p2 == 1.0000005)) ||
        (p2 == 1.0 && (p1 == 1.000000000001 || p1 == 1.0000005)))
        return "fuzzy_threshold";
    return "fuzzy_ordinary";
}
static std::string tagFnullD(double d)
{
    if (std::isnan(d)) return "fnull_nan";
    if (std::isinf(d)) return "fnull_inf";
    if (d == 0.0) return "fnull_zero";
    double a = std::fabs(d);
    if (a == 1e-12) return "fnull_at";
    if (a < 1e-12) return "fnull_below";
    return "fnull_above";
}
static std::string tagFloorD(double v)
{
    if (std::isnan(v)) return "floor_nan";
    if (std::isinf(v)) return "floor_inf";
    if (v == 0.49999999999999994) return "floor_ulp";
    if (v == 0.5 || v == -0.5) return "floor_half";
    if (v == std::floor(v)) return "floor_int";
    return "floor_frac";
}
static std::string tagP2(quint32 v)
{
    if (v == 0) return "p2_zero";
    if (v == 0xffffffffu) return "p2_overflow";
    if (v >= 0x80000000u) return "p2_high";
    if ((v & (v - 1)) == 0) return "p2_exact";
    if (v > 1 && (((v - 1) & (v - 2)) == 0)) return "p2_plus1";
    return "p2_ordinary";
}
static std::string tagIsNaN(double v)
{
    if (std::isnan(v)) return "isnan_nan";
    if (std::isinf(v)) return "isnan_inf";
    return "isnan_finite";
}

// ═══ 逐 API 对拍（每个重载一条 rec() 函数，规则三）══════════════════════
static void cmp_qabs_d()
{
    for (size_t i = 0; i < countOf(kD); ++i) {
        double v = kD[i];
        double q = ::qAbs(v), p = pkoracle::qAbs(v);
        rec("qAbs(double)", same_double(q, p), tagAbsD(v),
            "in=" + dstr(v), "qt=" + dstr(q), "pk=" + dstr(p));
    }
}
static void cmp_qabs_f()
{
    for (size_t i = 0; i < countOf(kF); ++i) {
        float v = kF[i];
        float q = ::qAbs(v), p = pkoracle::qAbs(v);
        rec("qAbs(float)", same_float(q, p), tagAbsF(v),
            "in=" + fstr(v), "qt=" + fstr(q), "pk=" + fstr(p));
    }
}
static void cmp_qabs_i()
{
    for (size_t i = 0; i < countOf(kI); ++i) {
        int v = kI[i];
        int q = ::qAbs(v), p = pkoracle::qAbs(v);
        rec("qAbs(int)", q == p, (v == INT_MIN) ? "abs_intmin" : "abs_ordinary",
            "in=" + istr(v), "qt=" + istr(q), "pk=" + istr(p));
    }
}
static void cmp_qabs_i64()
{
    for (size_t i = 0; i < countOf(kI64); ++i) {
        qint64 v = kI64[i];
        qint64 q = ::qAbs(v), p = pkoracle::qAbs(v);
        rec("qAbs(qint64)", q == p,
            (v == (qint64)0x8000000000000000LL) ? "abs_intmin" : "abs_ordinary",
            "in=" + istr(v), "qt=" + istr(q), "pk=" + istr(p));
    }
}

static void cmp_minmax_d()
{
    for (size_t i = 0; i < countOf(kD); ++i)
        for (size_t j = 0; j < countOf(kD); ++j) {
            double a = kD[i], b = kD[j];
            double qmn = ::qMin(a, b), pmn = pkoracle::qMin(a, b);
            double qmx = ::qMax(a, b), pmx = pkoracle::qMax(a, b);
            std::string in = "in=(" + dstr(a) + "," + dstr(b) + ")";
            std::string tag = tagMinMaxD(a, b);
            rec("qMin(double)", same_double(qmn, pmn), tag, in, "qt=" + dstr(qmn), "pk=" + dstr(pmn));
            rec("qMax(double)", same_double(qmx, pmx), tag, in, "qt=" + dstr(qmx), "pk=" + dstr(pmx));
        }
}
static void cmp_minmax_i()
{
    for (size_t i = 0; i < countOf(kI); ++i)
        for (size_t j = 0; j < countOf(kI); ++j) {
            int a = kI[i], b = kI[j];
            int qmn = ::qMin(a, b), pmn = pkoracle::qMin(a, b);
            int qmx = ::qMax(a, b), pmx = pkoracle::qMax(a, b);
            std::string in = "in=(" + istr(a) + "," + istr(b) + ")";
            std::string tag = "min_ordinary";   // int 没有 NaN/±0 形态
            rec("qMin(int)", qmn == pmn, tag, in, "qt=" + istr(qmn), "pk=" + istr(pmn));
            rec("qMax(int)", qmx == pmx, tag, in, "qt=" + istr(qmx), "pk=" + istr(pmx));
        }
}

// qBound 带 3 参：只做二层 (min,val) 全组合、max 在边界集上展开（brief）。
static void cmp_bound_d()
{
    static const double kMaxB[] = { 0.0, -0.0, NAN, INFINITY, -INFINITY, 1.0, 5.0, 10.0 };
    for (size_t i = 0; i < countOf(kD); ++i)
        for (size_t j = 0; j < countOf(kD); ++j)
            for (size_t m = 0; m < countOf(kMaxB); ++m) {
                double mn = kD[i], val = kD[j], mx = kMaxB[m];
                double q = ::qBound(mn, val, mx), p = pkoracle::qBound(mn, val, mx);
                rec("qBound(double)", same_double(q, p), tagBoundD(mn, val, mx),
                    "in=(" + dstr(mn) + "," + dstr(val) + "," + dstr(mx) + ")",
                    "qt=" + dstr(q), "pk=" + dstr(p));
            }
    // 组合爆破补充：brief 的 10-token 核心做满三层（10³ = 1000），max 也走 token。
    static const double kCore10[] = { NAN, -0.0, 0.0, 1.0, -1.0, 0.5, -0.5,
                                      0.49999999999999994, 1e300, 2.0 };
    for (size_t i = 0; i < countOf(kCore10); ++i)
        for (size_t j = 0; j < countOf(kCore10); ++j)
            for (size_t m = 0; m < countOf(kCore10); ++m) {
                double mn = kCore10[i], val = kCore10[j], mx = kCore10[m];
                double q = ::qBound(mn, val, mx), p = pkoracle::qBound(mn, val, mx);
                rec("qBound(double)", same_double(q, p), tagBoundD(mn, val, mx),
                    "in=(" + dstr(mn) + "," + dstr(val) + "," + dstr(mx) + ")",
                    "qt=" + dstr(q), "pk=" + dstr(p));
            }
}
static void cmp_bound_i()
{
    static const int kMaxB[] = { 0, 1, -1, INT_MIN, INT_MAX, 127, 128, 100000 };
    for (size_t i = 0; i < countOf(kI); ++i)
        for (size_t j = 0; j < countOf(kI); ++j)
            for (size_t m = 0; m < countOf(kMaxB); ++m) {
                int mn = kI[i], val = kI[j], mx = kMaxB[m];
                int q = ::qBound(mn, val, mx), p = pkoracle::qBound(mn, val, mx);
                rec("qBound(int)", q == p, "bound_ordinary",
                    "in=(" + istr(mn) + "," + istr(val) + "," + istr(mx) + ")",
                    "qt=" + istr(q), "pk=" + istr(p));
            }
}

static void cmp_qround_d()
{
    for (size_t i = 0; i < countOf(kD); ++i) {
        double v = kD[i];
        int q = ::qRound(v), p = pkoracle::qRound(v);
        rec("qRound(double)", q == p, tagRoundD(v),
            "in=" + dstr(v), "qt=" + istr(q), "pk=" + istr(p));
    }
}
static void cmp_qround_f()
{
    for (size_t i = 0; i < countOf(kF); ++i) {
        float v = kF[i];
        int q = ::qRound(v), p = pkoracle::qRound(v);
        rec("qRound(float)", q == p, tagRoundD((double)v),
            "in=" + fstr(v), "qt=" + istr(q), "pk=" + istr(p));
    }
}

static void cmp_fuzzy_d()
{
    for (size_t i = 0; i < countOf(kD); ++i)
        for (size_t j = 0; j < countOf(kD); ++j) {
            double a = kD[i], b = kD[j];
            bool q = ::qFuzzyCompare(a, b), p = pkoracle::qFuzzyCompare(a, b);
            rec("qFuzzyCompare(double)", q == p, tagFuzzyD(a, b),
                "in=(" + dstr(a) + "," + dstr(b) + ")", "qt=" + bstr(q), "pk=" + bstr(p));
        }
}
static void cmp_fuzzy_f()
{
    for (size_t i = 0; i < countOf(kF); ++i)
        for (size_t j = 0; j < countOf(kF); ++j) {
            float a = kF[i], b = kF[j];
            bool q = ::qFuzzyCompare(a, b), p = pkoracle::qFuzzyCompare(a, b);
            rec("qFuzzyCompare(float)", q == p, tagFuzzyD((double)a, (double)b),
                "in=(" + fstr(a) + "," + fstr(b) + ")", "qt=" + bstr(q), "pk=" + bstr(p));
        }
}

static void cmp_fnull_d()
{
    for (size_t i = 0; i < countOf(kD); ++i) {
        double v = kD[i];
        bool q = ::qFuzzyIsNull(v), p = pkoracle::qFuzzyIsNull(v);
        rec("qFuzzyIsNull(double)", q == p, tagFnullD(v),
            "in=" + dstr(v), "qt=" + bstr(q), "pk=" + bstr(p));
    }
}
static void cmp_fnull_f()
{
    for (size_t i = 0; i < countOf(kF); ++i) {
        float v = kF[i];
        bool q = ::qFuzzyIsNull(v), p = pkoracle::qFuzzyIsNull(v);
        rec("qFuzzyIsNull(float)", q == p, tagFnullD((double)v),
            "in=" + fstr(v), "qt=" + bstr(q), "pk=" + bstr(p));
    }
}

// ⚠ ±inf/NaN 的 int(v) 是 float→int 越界 UB，**不能跨侧比较**。Q 侧（真 Qt 的
// int(::floor(v))/int(::ceil(v))）在 -O2 下被优化器按自己的 UB 推理折叠出任意值：
// 实测同一次构建里 qFloor(+inf) 折成 INT_MIN、qCeil(+inf) 折成 INT_MAX，且这个
// 取值随 TU 里无关代码的排布翻转（PkGlobal.h 的 qCeil 一改，Q 侧 qCeil(+inf) 就
// 从 INT_MIN 变 INT_MAX）。跨侧比这两个数，比的是编译器噪声，不是行为差异。
// 所以非有限输入**不跨侧比较**，而是把 P 侧钉在**运行期 cvttsd2si 的实测值
// INT_MIN**（真 Qt 5.15.7 探针 + 修复轮简报 Important 1：qFloor/qCeil 对非有限值
// 原样截断，x86 → INT_MIN）。P 侧一旦漂回旧的回绕值（qFloor(-inf)=INT_MAX、
// qCeil(+inf)=INT_MIN+1）就出未声明 tag，FAIL。有限输入无 UB，照常跨侧比较。
// ⚠ 喂入必须走运行期 noinline getter：编译期常量（static const 里的 INFINITY）会
// 让 P 侧自己把 int(±inf) 折叠成 INT_MAX，钉 INT_MIN 就假红（run_oracle.sh 注释
// 里「输入必须来自运行期数组」的同一告诫）。noinline 返回值对调用点编译期不可见。
static double oracle_inf() __attribute__((noinline));
static double oracle_inf() { return INFINITY; }
static double oracle_nan() __attribute__((noinline));
static double oracle_nan() { return NAN; }

static void cmp_floorceil()
{
    // brief 上表：{±2.7, ±2.0, ±0.5, ±0.49999999999999994, NaN, ±Inf}，各喂一次。
    // 有限值无 UB，可作 static const，跨侧比较。
    static const double kVFinite[] = { 2.7, -2.7, 2.0, -2.0, 0.5, -0.5,
                                       0.49999999999999994 };
    for (size_t i = 0; i < countOf(kVFinite); ++i) {
        double v = kVFinite[i];
        int qf = ::qFloor(v), pf = pkoracle::qFloor(v);
        int qc = ::qCeil(v), pc = pkoracle::qCeil(v);
        rec("qFloor(qreal)", qf == pf, tagFloorD(v),
            "in=" + dstr(v), "qt=" + istr(qf), "pk=" + istr(pf));
        rec("qCeil(qreal)", qc == pc, tagFloorD(v),
            "in=" + dstr(v), "qt=" + istr(qc), "pk=" + istr(pc));
    }

    // 非有限输入：钉 INT_MIN（见上方注释）。±inf/NaN 各喂一次，tag 照常构造。
    const int kTrunc = INT_MIN;
    double kVNonFinite[3] = { oracle_nan(), oracle_inf(), -oracle_inf() };
    for (size_t i = 0; i < 3; ++i) {
        double v = kVNonFinite[i];
        int pf = pkoracle::qFloor(v);
        int pc = pkoracle::qCeil(v);
        rec("qFloor(qreal)", pf == kTrunc, tagFloorD(v),
            "in=" + dstr(v), "qt=" + istr(kTrunc), "pk=" + istr(pf));
        rec("qCeil(qreal)", pc == kTrunc, tagFloorD(v),
            "in=" + dstr(v), "qt=" + istr(kTrunc), "pk=" + istr(pc));
    }
}

static void cmp_p2()
{
    for (size_t i = 0; i < countOf(kU32); ++i) {
        quint32 v = kU32[i];
        quint32 q = ::qNextPowerOfTwo(v), p = pkoracle::qNextPowerOfTwo(v);
        rec("qNextPowerOfTwo(quint32)", q == p, tagP2(v),
            "in=" + ustr(v), "qt=" + ustr(q), "pk=" + ustr(p));
    }
}

static void cmp_isnan()
{
    for (size_t i = 0; i < countOf(kD); ++i) {
        double v = kD[i];
        bool q = ::qIsNaN(v), p = pkoracle::qIsNaN(v);
        rec("qIsNaN(double)", q == p, tagIsNaN(v),
            "in=" + dstr(v), "qt=" + bstr(q), "pk=" + bstr(p));
    }
    for (size_t i = 0; i < countOf(kF); ++i) {
        float v = kF[i];
        bool q = ::qIsNaN(v), p = pkoracle::qIsNaN(v);
        rec("qIsNaN(float)", q == p, tagIsNaN((double)v),
            "in=" + fstr(v), "qt=" + bstr(q), "pk=" + bstr(p));
    }
}

// 零参函数：没有输入，tag 由「被测试的谓词」构造（brief：== +Inf / >0 / <0）。
static void cmp_inf_qnan()
{
    bool qp = (::qInf() == INFINITY), pp = (pkoracle::qInf() == INFINITY);
    rec("qInf()", qp == pp, "inf_pos", "pred:==+Inf", bstr(qp), bstr(pp));
    bool qg = (::qInf() > 0.0), pg = (pkoracle::qInf() > 0.0);
    rec("qInf()", qg == pg, "inf_sign", "pred:>0", bstr(qg), bstr(pg));
    bool ql = (::qInf() < 0.0), pl = (pkoracle::qInf() < 0.0);
    rec("qInf()", ql == pl, "inf_sign", "pred:<0", bstr(ql), bstr(pl));

    bool qsn = (::qQNaN() != ::qQNaN()), psn = (pkoracle::qQNaN() != pkoracle::qQNaN());
    rec("qQNaN()", qsn == psn, "qnan_self_neq", "pred:!=self", bstr(qsn), bstr(psn));
    bool qz = (::qQNaN() != 0.0), pz = (pkoracle::qQNaN() != 0.0);
    rec("qQNaN()", qz == pz, "qnan_nonzero", "pred:!=0", bstr(qz), bstr(pz));
}

// ═══ canary：每 API 至少一条故意不相等的比对 ═════════════════════════════
// 走与真实 API 完全相同的 rec()/same_*/tag 路径。全部必须出现在输出里，
// 少一条 → run_oracle.sh FAIL（比较管道被写死/被优化掉/tag 构造断了）。
static void cmp_canaries()
{
    rec("canary", same_double(::qAbs(-3.0) + 1.0, pkoracle::qAbs(-3.0)),
        "qabs_pipeline", "canary:qAbs", dstr(::qAbs(-3.0) + 1.0), dstr(pkoracle::qAbs(-3.0)));
    rec("canary", same_double(::qMin(1.0, 2.0) + 1.0, pkoracle::qMin(1.0, 2.0)),
        "minmax_pipeline", "canary:qMin", dstr(::qMin(1.0, 2.0) + 1.0), dstr(pkoracle::qMin(1.0, 2.0)));
    rec("canary", same_double(::qBound(0.0, 5.0, 10.0) + 1.0, pkoracle::qBound(0.0, 5.0, 10.0)),
        "bound_pipeline", "canary:qBound", dstr(::qBound(0.0, 5.0, 10.0) + 1.0), dstr(pkoracle::qBound(0.0, 5.0, 10.0)));
    rec("canary", ::qRound(2.5) + 1 == pkoracle::qRound(2.5),
        "round_pipeline", "canary:qRound", istr(::qRound(2.5) + 1), istr(pkoracle::qRound(2.5)));
    rec("canary", ::qFuzzyCompare(1.0, 1.0) != pkoracle::qFuzzyCompare(1.0, 1.0),
        "fuzzycompare_pipeline", "canary:qFuzzyCompare",
        bstr(::qFuzzyCompare(1.0, 1.0)), bstr(pkoracle::qFuzzyCompare(1.0, 1.0)));
    rec("canary", ::qFuzzyIsNull(0.0) != pkoracle::qFuzzyIsNull(0.0),
        "fuzzyisnull_pipeline", "canary:qFuzzyIsNull",
        bstr(::qFuzzyIsNull(0.0)), bstr(pkoracle::qFuzzyIsNull(0.0)));
    rec("canary", ::qFloor(2.7) + 1 == pkoracle::qFloor(2.7),
        "floorceil_pipeline", "canary:qFloor", istr(::qFloor(2.7) + 1), istr(pkoracle::qFloor(2.7)));
    rec("canary", ::qNextPowerOfTwo(3) + 5 == pkoracle::qNextPowerOfTwo(3),
        "p2_pipeline", "canary:qNextPowerOfTwo",
        ustr(::qNextPowerOfTwo(3) + 5), ustr(pkoracle::qNextPowerOfTwo(3)));
    rec("canary", ::qIsNaN(1.0) != pkoracle::qIsNaN(1.0),
        "isnan_pipeline", "canary:qIsNaN", bstr(::qIsNaN(1.0)), bstr(pkoracle::qIsNaN(1.0)));
    rec("canary", same_double(::qInf(), pkoracle::qInf()) && same_double(::qInf(), 0.0),
        "inf_pipeline", "canary:qInf", dstr(::qInf()), dstr(pkoracle::qInf()));
    rec("canary", same_double(::qQNaN(), pkoracle::qQNaN()) && same_double(::qQNaN(), 0.0),
        "qnan_pipeline", "canary:qQNaN", dstr(::qQNaN()), dstr(pkoracle::qQNaN()));
}

int main()
{
    cmp_qabs_d(); cmp_qabs_f(); cmp_qabs_i(); cmp_qabs_i64();
    cmp_minmax_d(); cmp_minmax_i();
    cmp_bound_d(); cmp_bound_i();
    cmp_qround_d(); cmp_qround_f();
    cmp_fuzzy_d(); cmp_fuzzy_f();
    cmp_fnull_d(); cmp_fnull_f();
    cmp_floorceil();
    cmp_p2();
    cmp_isnan();
    cmp_inf_qnan();
    cmp_canaries();

    for (const auto &kv : g_tags)
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    // 分母行（契约外，给人读「命中 N 次里分家 M 次」）：只为出现过差异的 tag 打。
    for (const auto &kv : g_tags)
        std::printf("DIFFDEN %s %ld\n", kv.first.c_str(), g_tag_seen[kv.first]);
    // 规则三机器闸门：与 oracle/api_seen.expected 对账。
    for (const auto &a : g_apis)
        std::printf("APISEEN %s\n", a.c_str());
    // Q_ASSERT 登记行（编译期语义，非运行期可比）：PkGlobal 的条件是
    // QT_NO_DEBUG||NDEBUG，真 Qt 是 QT_NO_DEBUG&&!QT_FORCE_ASSERTS。对拍以
    // -DQT_NO_DEBUG 编译，两侧宏展开一致，此差异在 -DNDEBUG（无 QT_NO_DEBUG）
    // 的构建下才显现 —— 登记在 global.deviation 首行，不经过 rec()。
    std::printf("DIFFTAG Q_ASSERT assert_gate 1\n");
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 已声明的偏离不算失败；判定在 run_oracle.sh
}
