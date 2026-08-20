// pk/string/oracle/difftest_string.cpp —— PkString ↔ QString 逐输入对拍
//
// R-13 之前 pk/string 没有持久化的对拍程序（R-01 完成时用的是旧版 replacement.sh
// 判据，对拍是临时跑的，没有落盘）。本文件是这个基础设施缺口的补齐，同时验证
// R-13 Task 1-3 的对齐修复。
//
// 形态契约（spec「对拍怎么做」）：
//   - 两侧真的分别 #include <QString> 与 "PkString.h"，static_assert 防止两侧
//     被 compat 垫片解析成同一个类型
//   - -I 绝不能给 pk/string/compat/（run_oracle.sh 保证）
//   - stdout 只有 DIFF total=<N> mismatch=<M>（恰一行）与 DIFFTAG（0..N 行）
//   - 退出码恒为 0
//
// tag 命名：<api>/<输入形态>，形态由触发差异的输入构造（规则一）；
//   本文件只处理标量 API（无容器/无共享状态维度，与 pk/container 的对拍不同），
//   所以 tag 不需要 pk/container 那套 shared/elem-type 后缀。

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMessageLogContext>

#include <PkString.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

static_assert(!std::is_same<QString, PkString>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");

namespace {

long g_total = 0;
long g_mismatch = 0;
std::map<std::string, long> g_tags;
std::map<std::string, long> g_cover;
long g_printed = 0;

void rec(const std::string& api, bool same, const std::string& tag,
        const std::string& in, const std::string& qs, const std::string& ps)
{
    ++g_total;
    ++g_cover[api];
    if (same) {
        return;
    }
    ++g_mismatch;
    ++g_tags[api + " " + tag];
    if (g_printed < 60) {
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n",
                    api.c_str(), tag.c_str(), in.c_str(), qs.c_str(), ps.c_str());
    }
}

std::string esQ(const QString& s)
{
    const QByteArray b = s.toUtf8();
    return "\"" + std::string(b.constData(), static_cast<std::size_t>(b.size())) + "\"";
}
std::string esP(const PkString& s) { return "\"" + s.PkToUtf8() + "\""; }
std::string esD(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}
std::string esB(bool v) { return v ? "true" : "false"; }

// ── tag 构造辅助（规则一：tag 必须编码触发差异的输入形态，不能是字面量常量）──

// hay/needle 关系分类，contains/startsWith 共用：needle 是否为空、是否与 hay
// 相同、是否是 hay 的前缀、是否是 hay 的子串（非前缀）、还是完全不相关。
std::string pkClassifyPair(const std::string& hay, const std::string& needle)
{
    if (needle.empty()) return "needle-empty";
    if (hay == needle) return "equal";
    if (hay.size() >= needle.size() && hay.compare(0, needle.size(), needle) == 0) return "prefix";
    if (hay.find(needle) != std::string::npos) return "substring";
    return "unrelated";
}

// trimmed() 的输入分类：空串 / 全是 ASCII 空白 / 含非 ASCII 字节（NBSP 等
// Unicode 空白探针都落在这一类）/ 其余（含非空白 ASCII 内容）。
std::string pkClassifyTrimInput(const std::string& s)
{
    if (s.empty()) return "empty";
    bool hasNonAscii = false;
    bool allAsciiSpace = true;
    for (unsigned char c : s) {
        if (c >= 0x80) hasNonAscii = true;
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')) {
            if (c < 0x80) allAsciiSpace = false;
        }
    }
    if (hasNonAscii) return "has-non-ascii";
    if (allAsciiSpace) return "all-ascii-space";
    return "ascii-content";
}

// fmt 里第一个 %<digits> 占位符的编号（没有则 "none"），加上实参是否为空——
// arg1/argDouble/arg2 共用同一套分类逻辑。
std::string pkClassifyFmt(const std::string& fmt)
{
    for (std::size_t i = 0; i + 1 < fmt.size(); ++i) {
        if (fmt[i] == '%' && std::isdigit(static_cast<unsigned char>(fmt[i + 1]))) {
            std::string num;
            std::size_t j = i + 1;
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j])) && num.size() < 2) {
                num += fmt[j];
                ++j;
            }
            return "num=" + num;
        }
    }
    return "num=none";
}

// Unicode 标量的 UTF-8 编码。不能走 C 字符串字面量：穷举包含 cp=0（NUL）。
std::string pkUtf8Encode(unsigned cp)
{
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

// ── 输入 token 表 ──────────────────────────────────────────
// 字符串 token：ASCII 常见形态 + 三个非 ASCII 探针（é 单码元 / U+FFFD / 🎨 代理对）。
const char* const kStrTok[] = {
    "", "a", "ab", "hello world", " pad ", "\t\n\r ", "%1", "%0", "%L1",
    "\xC3\xA9",              // é U+00E9
    "\xEF\xBF\xBD",          // U+FFFD
    "\xF0\x9F\x8E\xA8",      // 🎨 U+1F3A8 代理对
    "\xC2\xA0hi\xC2\xA0",    // NBSP 包裹
};
constexpr int kNStrTok = sizeof(kStrTok) / sizeof(kStrTok[0]);

// 数值 token：覆盖边界 / 符号 / 十六进制 / inf-nan 家族 / 下溢上溢 / 分隔符
const char* const kNumTok[] = {
    "0", "1", "-1", "42", "-42", "1.5", "-0.25", "+3.5", "abc", "",
    "  7 ", "0x10", "0x1p3", "-0X2", "++3.5", "+ 3.5", "1e-310", "4.9e-324",
    "1e-400", "1e400", "inf", "Inf", "INF", "+inf", "-inf", "infinity",
    "Infinity", "infi", "nan", "NaN", "+nan", "-nan", "nano", "1.5f",
    "1_000", "99999999999999999999", "-2147483648", "2147483647",
};
constexpr int kNNumTok = sizeof(kNumTok) / sizeof(kNumTok[0]);

const int kIntTok[] = {0, 1, -1, 3, -3, 42, -42, 999, 1000, -999, -1000,
                        1234567, -1234567, 123456789, 6, -6, 2147483647, -2147483647};
constexpr int kNIntTok = sizeof(kIntTok) / sizeof(kIntTok[0]);

const int kFieldWidthTok[] = {0, 1, 6, -6, 20, -20};
constexpr int kNFieldWidthTok = sizeof(kFieldWidthTok) / sizeof(kFieldWidthTok[0]);

// n/pos 的下标编码：覆盖负数、0、size 内、恰好 size、size 外
const int kIdxTok[] = {-100, -2, -1, 0, 1, 2, 900, 901, 902};  // 900/901/902 见 pkResolveIdx
constexpr int kNIdxTok = sizeof(kIdxTok) / sizeof(kIdxTok[0]);

// arg(double)：格式串覆盖 %1（不分组）与 %L1（分组），数值覆盖普通值、
// 正负零、超过 1000 的值、以及切科学计数法的大值/小值（I2）。
const char* const kDblFmtTok[] = {"%1", "%L1"};
constexpr int kNDblFmtTok = sizeof(kDblFmtTok) / sizeof(kDblFmtTok[0]);
const double kDblTok[] = {
    0.0, -0.0, 1.5, -1.5, 999.5, 1000.0, 1234.5, -1234.5, 123456.0,
    1000000.0, -1000000.0, 1e21, 1e-7,
};
constexpr int kNDblTok = sizeof(kDblTok) / sizeof(kDblTok[0]);

// 双参字符串版 arg(a,b) 的格式串：带占位符（含 %0 起始编号、重复引用、无
// 占位符对照组）。
const char* const kArg2FmtTok[] = {"%1-%2", "%2-%1", "%1 %1 %2", "%0-%1-%2", "no placeholder"};
constexpr int kNArg2FmtTok = sizeof(kArg2FmtTok) / sizeof(kArg2FmtTok[0]);

// 三参字符串版 arg(a,b,c) 的格式串：按位置、乱序编号、重复编号、真实调用点
// 形态（kis_assert.cpp:120）各覆盖一条，另加无占位符对照组。
const char* const kArg3FmtTok[] = {
    "%1-%2-%3", "%3-%1-%2", "%1-%1-%2",
    "ASSERT failure in %1: \"%2\" (%3)", "no placeholder",
};
constexpr int kNArg3FmtTok = sizeof(kArg3FmtTok) / sizeof(kArg3FmtTok[0]);

// 三参实参用 kStrTok 的代表性子集（空串/ASCII/空白/占位符样式文本/非 ASCII/
// 代理对），而不是 13^3 全组合——覆盖面已含关键形态，运行时间可控（R-13 补充
// 任务的权衡取舍）。
const int kArg3TokIdx[] = {0, 1, 4, 6, 9, 11};
constexpr int kNArg3TokIdx = sizeof(kArg3TokIdx) / sizeof(kArg3TokIdx[0]);

int pkResolveIdx(int code, int n)
{
    if (code == 900) return n;
    if (code == 901) return n - 1;
    if (code == 902) return n + 1;
    return code;
}

} // namespace

// ── 每个 API 一个对拍函数：同一输入喂两侧，比返回值 ──────────

void diffLeft(const char* sIn, int nCode)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const int n = pkResolveIdx(nCode, qs.size());
    QString qr = qs.left(n);
    PkString pr = ps.left(n);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = "n=" + std::to_string(nCode);
    rec("left", same, tag, std::string(sIn) + "/" + std::to_string(n), esQ(qr), esP(pr));
}

void diffRight(const char* sIn, int nCode)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const int n = pkResolveIdx(nCode, qs.size());
    QString qr = qs.right(n);
    PkString pr = ps.right(n);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = "n=" + std::to_string(nCode);
    rec("right", same, tag, std::string(sIn) + "/" + std::to_string(n), esQ(qr), esP(pr));
}

void diffMid(const char* sIn, int posCode, int nCode)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const int pos = pkResolveIdx(posCode, qs.size());
    const int n = (nCode == 999) ? -1 : pkResolveIdx(nCode, qs.size());  // 999 = 用默认值
    QString qr = (nCode == 999) ? qs.mid(pos) : qs.mid(pos, n);
    PkString pr = (nCode == 999) ? ps.mid(pos) : ps.mid(pos, n);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = "pos=" + std::to_string(posCode) + "/n=" + std::to_string(nCode);
    rec("mid", same, tag, std::string(sIn) + "/" + std::to_string(pos) + "/" + std::to_string(n),
        esQ(qr), esP(pr));
}

void diffTrimmed(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    QString qr = qs.trimmed();
    PkString pr = ps.trimmed();
    const bool same = (esQ(qr) == esP(pr));
    rec("trimmed", same, pkClassifyTrimInput(sIn), sIn, esQ(qr), esP(pr));
}

// I3：trimmed() 的 Unicode 空白判据穷举码点 0x0000..0x3001（含），每个码点前后
// 各包一层、中间夹一个 'x'。tag 直接是码点本身——25 个真空白码点应当在两侧都
// 被剥掉、其余码点在两侧都不该被剥掉，任何一条不一致都是 pkIsSpace 的判据漏了
// 或多了这个码点。
void diffTrimmedCodepoint(unsigned cp)
{
    const std::string enc = pkUtf8Encode(cp);
    const std::string s = enc + "x" + enc;
    QString qs = QString::fromUtf8(s.data(), static_cast<int>(s.size()));
    PkString ps = PkString::PkFromUtf8(s.data(), static_cast<int>(s.size()));
    QString qr = qs.trimmed();
    PkString pr = ps.trimmed();
    const bool same = (esQ(qr) == esP(pr));
    char tagbuf[16];
    std::snprintf(tagbuf, sizeof(tagbuf), "cp=U+%04X", cp);
    rec("trimmed-cp", same, tagbuf, "U+" + std::to_string(cp), esQ(qr), esP(pr));
}

void diffContains(const char* hay, const char* needle)
{
    QString qh = QString::fromUtf8(hay), qn = QString::fromUtf8(needle);
    PkString ph(hay), pn(needle);
    const bool qr = qh.contains(qn);
    const bool pr = ph.contains(pn);
    const bool same = (qr == pr);
    rec("contains", same, pkClassifyPair(hay, needle), std::string(hay) + "|" + needle, esB(qr), esB(pr));
}

void diffStartsWith(const char* hay, const char* prefix)
{
    QString qh = QString::fromUtf8(hay), qp = QString::fromUtf8(prefix);
    PkString ph(hay), pp(prefix);
    const bool qr = qh.startsWith(qp);
    const bool pr = ph.startsWith(pp);
    const bool same = (qr == pr);
    rec("startsWith", same, pkClassifyPair(hay, prefix), std::string(hay) + "|" + prefix, esB(qr), esB(pr));
}

void diffSplit(const char* sIn, char sep)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    QStringList qr = qs.split(QChar(static_cast<ushort>(sep)));
    std::vector<PkString> pr = ps.split(static_cast<char16_t>(sep));
    std::string qdump = std::to_string(qr.size());
    for (const QString& x : qr) qdump += "|" + esQ(x);
    std::string pdump = std::to_string(pr.size());
    for (const PkString& x : pr) pdump += "|" + esP(x);
    const bool same = (qdump == pdump);
    const std::string tag = std::string("sep=") + sep + "/contains-sep="
                           + (std::string(sIn).find(sep) != std::string::npos ? "1" : "0");
    rec("split", same, tag, std::string(sIn) + "/" + std::string(1, sep), qdump, pdump);
}

void diffArg1(const char* fmt, const char* a)
{
    QString qf = QString::fromUtf8(fmt), qa = QString::fromUtf8(a);
    PkString pf(fmt), pa(a);
    QString qr = qf.arg(qa);
    PkString pr = pf.arg(pa);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = pkClassifyFmt(fmt) + "/arg-empty=" + (std::string(a).empty() ? "1" : "0");
    rec("arg1", same, tag, std::string(fmt) + "<-" + a, esQ(qr), esP(pr));
}

void diffArgDouble(const char* fmt, double v)
{
    QString qf = QString::fromUtf8(fmt);
    PkString pf(fmt);
    QString qr = qf.arg(v);
    PkString pr = pf.arg(v);
    const bool same = (esQ(qr) == esP(pr));
    const std::string magClass = (v == 0.0) ? (std::signbit(v) ? "neg-zero" : "zero")
                                : (std::fabs(v) < 1000.0 ? "small"
                                   : (std::fabs(v) >= 1e6 ? "sci-magnitude" : "large"));
    const std::string tag = pkClassifyFmt(fmt) + "/mag=" + magClass;
    rec("argDouble", same, tag, std::string(fmt) + "<-" + esD(v), esQ(qr), esP(pr));
}

void diffArg2(const char* fmt, const char* a, const char* b)
{
    QString qf = QString::fromUtf8(fmt), qa = QString::fromUtf8(a), qb = QString::fromUtf8(b);
    PkString pf(fmt), pa(a), pb(b);
    QString qr = qf.arg(qa, qb);
    PkString pr = pf.arg(pa, pb);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = pkClassifyFmt(fmt) + "/a-empty=" + (std::string(a).empty() ? "1" : "0")
                           + "/b-empty=" + (std::string(b).empty() ? "1" : "0");
    rec("arg2", same, tag, std::string(fmt) + "<-" + a + "," + b, esQ(qr), esP(pr));
}

void diffArg3(const char* fmt, const char* a, const char* b, const char* c)
{
    QString qf = QString::fromUtf8(fmt), qa = QString::fromUtf8(a), qb = QString::fromUtf8(b),
            qc = QString::fromUtf8(c);
    PkString pf(fmt), pa(a), pb(b), pc(c);
    QString qr = qf.arg(qa, qb, qc);
    PkString pr = pf.arg(pa, pb, pc);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = pkClassifyFmt(fmt) + "/a-empty=" + (std::string(a).empty() ? "1" : "0")
                           + "/b-empty=" + (std::string(b).empty() ? "1" : "0")
                           + "/c-empty=" + (std::string(c).empty() ? "1" : "0");
    rec("arg3", same, tag, std::string(fmt) + "<-" + a + "," + b + "," + c, esQ(qr), esP(pr));
}

void diffAppend(const char* baseIn, const char* otherIn)
{
    QString qb = QString::fromUtf8(baseIn);
    PkString pb(baseIn);
    qb.append(QString::fromUtf8(otherIn));
    pb.append(PkString(otherIn));
    const bool same = (esQ(qb) == esP(pb));
    const std::string tag = std::string("base-empty=") + (std::string(baseIn).empty() ? "1" : "0")
                           + "/other-empty=" + (std::string(otherIn).empty() ? "1" : "0");
    rec("append", same, tag, std::string(baseIn) + "+" + otherIn, esQ(qb), esP(pb));
}

void diffSize(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const int qr = qs.size();
    const int pr = ps.size();
    const bool same = (qr == pr);
    const std::string tag = std::string("empty=") + (std::string(sIn).empty() ? "1" : "0");
    rec("size", same, tag, sIn, std::to_string(qr), std::to_string(pr));
}

void diffIsEmpty(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const bool qr = qs.isEmpty();
    const bool pr = ps.isEmpty();
    const bool same = (qr == pr);
    const std::string tag = std::string("empty=") + (std::string(sIn).empty() ? "1" : "0");
    rec("isEmpty", same, tag, sIn, esB(qr), esB(pr));
}

// at() 越界是 QString 的 UB（PkString.h 顶部的用量表注释同样这么写：
// "i 越界 → u'\0'（QString 在此是 UB）"），所以不对拍越界形态——那样对 QString
// 一侧没有"正确答案"可比，只对拍 [0, size) 内的合法下标。
void diffAt(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    const int n = qs.size();
    for (int i = 0; i < n; ++i) {
        const QChar qc = qs.at(i);
        const char16_t pc = ps.at(i);
        const bool same = (qc.unicode() == pc);
        const std::string tag = std::string("pos=") + (i == 0 ? "first" : (i == n - 1 ? "last" : "mid"));
        rec("at", same, tag, std::string(sIn) + "/" + std::to_string(i),
            std::to_string(static_cast<unsigned>(qc.unicode())),
            std::to_string(static_cast<unsigned>(pc)));
    }
}

void diffArgInt(const char* fmt, int v)
{
    QString qf = QString::fromUtf8(fmt);
    PkString pf(fmt);
    QString qr = qf.arg(v);
    PkString pr = pf.arg(v);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = "v=" + std::to_string(v);
    rec("argInt", same, tag, std::string(fmt) + "<-" + std::to_string(v), esQ(qr), esP(pr));
}

void diffArgIntFieldWidth(const char* fmt, int v, int fw)
{
    QString qf = QString::fromUtf8(fmt);
    PkString pf(fmt);
    QString qr = qf.arg(v, fw);
    PkString pr = pf.arg(v, fw);
    const bool same = (esQ(qr) == esP(pr));
    const std::string tag = "v=" + std::to_string(v) + "/fw=" + std::to_string(fw);
    rec("argIntFieldWidth", same, tag,
        std::string(fmt) + "<-" + std::to_string(v) + "," + std::to_string(fw), esQ(qr), esP(pr));
}

void diffToInt(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    bool qok = false, pok = false;
    const int qr = qs.toInt(&qok);
    const int pr = ps.toInt(&pok);
    const bool same = (qok == pok) && (!qok || qr == pr);
    const std::string tag = std::string("ok=") + (qok ? "1" : "0");
    rec("toInt", same, tag, sIn, (qok ? std::to_string(qr) : std::string("FAIL")),
        (pok ? std::to_string(pr) : std::string("FAIL")));
}

void diffToDouble(const char* sIn)
{
    QString qs = QString::fromUtf8(sIn);
    PkString ps(sIn);
    bool qok = false, pok = false;
    const double qr = qs.toDouble(&qok);
    const double pr = ps.toDouble(&pok);
    // 规则二：谓词不能比"两侧都失败"宽。背景 ⑦a 证明了"两侧都失败"时返回值仍然
    // 是可观察行为（上溢失败要带回 ±inf，不是清零）——所以**不管 ok 是否一致，
    // 都要比较返回值本身**，只有 NaN 需要特判（NaN != NaN）。
    bool valueSame;
    if (qok != pok) {
        valueSame = false;
    } else if (std::isnan(qr) && std::isnan(pr)) {
        valueSame = true;
    } else {
        valueSame = (qr == pr);   // 覆盖 0.0==0.0 与 inf==inf 两种情形
    }
    const std::string tag = std::string("ok=") + (qok ? "1" : "0");
    // 值恒打印（不再是失败就打 "FAIL"）：背景 ⑦a 证明失败路径的返回值本身
    // 也是要对拍的行为，明细行必须能看见它，否则调试上溢那条差异时看不到数字。
    rec("toDouble", valueSame, tag, sIn,
        (qok ? "ok:" : "FAIL:") + esD(qr), (pok ? "ok:" : "FAIL:") + esD(pr));
}

std::string pkCaseInput(const std::vector<unsigned>& codePoints)
{
    std::string input;
    for (unsigned cp : codePoints) {
        input += pkUtf8Encode(cp);
    }
    return input;
}

std::string pkCaseShape(const std::vector<unsigned>& codePoints)
{
    bool hasAscii = false;
    bool hasBmp = false;
    bool hasAstral = false;
    for (unsigned cp : codePoints) {
        if (cp <= 0x7F) hasAscii = true;
        else if (cp <= 0xFFFF) hasBmp = true;
        else hasAstral = true;
    }
    std::string form;
    if (hasAscii) form += "ascii+";
    if (hasBmp) form += "bmp+";
    if (hasAstral) form += "astral+";
    if (!form.empty()) form.pop_back();
    if (form.empty()) form = "empty";
    return "units=" + std::to_string(codePoints.size()) + "/form=" + form;
}

std::string pkCaseCodePoints(const std::vector<unsigned>& codePoints)
{
    std::string dump;
    char buffer[16];
    for (unsigned cp : codePoints) {
        std::snprintf(buffer, sizeof(buffer), "U+%04X", cp);
        if (!dump.empty()) dump += "+";
        dump += buffer;
    }
    return dump.empty() ? "empty" : dump;
}

void diffCase(const std::vector<unsigned>& codePoints, const char* coverage)
{
    const std::string input = pkCaseInput(codePoints);
    // Anchor then slice so a leading U+FEFF is not interpreted as a UTF-8 BOM
    // by QString::fromUtf8. Both sides still consume the same bytes, and the
    // value entering the case algorithm is the intended code-point sequence.
    const std::string anchored = "x" + input;
    const QString q = QString::fromUtf8(anchored.data(), static_cast<int>(anchored.size())).mid(1);
    const PkString p = PkString::PkFromUtf8(anchored.data(), static_cast<int>(anchored.size())).mid(1);
    const std::string shape = std::string("coverage=") + coverage + "/" + pkCaseShape(codePoints);
    const std::string inputDump = pkCaseCodePoints(codePoints);

    const QString qLower = q.toLower();
    const PkString pLower = p.toLower();
    rec("toLower", esQ(qLower) == esP(pLower), "direction=lower/" + shape,
        inputDump, esQ(qLower), esP(pLower));

    const QString qUpper = q.toUpper();
    const PkString pUpper = p.toUpper();
    rec("toUpper", esQ(qUpper) == esP(pUpper), "direction=upper/" + shape,
        inputDump, esQ(qUpper), esP(pUpper));
}

PkString pkStringFromRawUtf16(const std::vector<char16_t>& units)
{
    PkString result;
    for (char16_t unit : units) {
        unsigned cp = unit;
        int slicePosition = 1;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            cp = 0x10000 + (static_cast<unsigned>(unit) - 0xD800) * 0x400;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            cp = 0x10000 + (static_cast<unsigned>(unit) - 0xDC00);
            slicePosition = 2;
        }

        const std::string anchored = "x" + pkUtf8Encode(cp);
        const PkString decoded =
            PkString::PkFromUtf8(anchored.data(), static_cast<int>(anchored.size())).mid(1);
        result += decoded.mid(slicePosition - 1, 1);
    }
    return result;
}

std::string pkUtf16Units(const std::u16string& units)
{
    std::string dump;
    char buffer[16];
    for (char16_t unit : units) {
        std::snprintf(buffer, sizeof(buffer), "%04X", static_cast<unsigned>(unit));
        if (!dump.empty()) dump += "+";
        dump += buffer;
    }
    return dump.empty() ? "empty" : dump;
}

std::u16string qUtf16Units(const QString& value)
{
    std::u16string units;
    units.reserve(static_cast<std::size_t>(value.size()));
    for (int i = 0; i < value.size(); ++i) {
        units.push_back(static_cast<char16_t>(value.at(i).unicode()));
    }
    return units;
}

void diffCaseRawUtf16(const std::vector<char16_t>& units, const char* shape)
{
    const QString q = QString::fromUtf16(
        reinterpret_cast<const ushort*>(units.data()), static_cast<int>(units.size()));
    const PkString p = pkStringFromRawUtf16(units);
    const std::string inputDump = pkUtf16Units(std::u16string(units.begin(), units.end()));

    const std::u16string qLower = qUtf16Units(q.toLower());
    const std::u16string pLower = p.toLower().PkToU16();
    rec("toLower", qLower == pLower,
        std::string("direction=lower/coverage=raw-utf16/shape=") + shape,
        inputDump, pkUtf16Units(qLower), pkUtf16Units(pLower));

    const std::u16string qUpper = qUtf16Units(q.toUpper());
    const std::u16string pUpper = p.toUpper().PkToU16();
    rec("toUpper", qUpper == pUpper,
        std::string("direction=upper/coverage=raw-utf16/shape=") + shape,
        inputDump, pkUtf16Units(qUpper), pkUtf16Units(pUpper));
}

int main()
{
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {});
    // I4：把当前生效的 LC_ALL 打进这一行——run_oracle.sh 会显式钉死它，但重跑的人
    // 一眼就能看出这次跑的是哪个 locale 下的结果，不用去猜（%L 分组固定不跟随
    // locale 这条本来就是"跟不跟随运行时 locale"的岔路，locale 没钉住会让
    // mismatch=0 这个断言变得环境相关）。
    const char* lcAll = std::getenv("LC_ALL");
    std::printf("ORACLE-QT file=difftest_string.cpp qVersion=%s LC_ALL=%s\n",
                qVersion(), lcAll != nullptr ? lcAll : "(unset)");

    // left/right/mid：字符串 token × 下标 token（含默认 n=999 的 mid 一支）
    for (int si = 0; si < kNStrTok; ++si) {
        for (int ni = 0; ni < kNIdxTok; ++ni) {
            diffLeft(kStrTok[si], kIdxTok[ni]);
            diffRight(kStrTok[si], kIdxTok[ni]);
        }
        for (int pi = 0; pi < kNIdxTok; ++pi) {
            for (int ni = 0; ni < kNIdxTok; ++ni) {
                diffMid(kStrTok[si], kIdxTok[pi], kIdxTok[ni]);
            }
            diffMid(kStrTok[si], kIdxTok[pi], 999);   // 默认 n
        }
        diffTrimmed(kStrTok[si]);
        diffSize(kStrTok[si]);
        diffIsEmpty(kStrTok[si]);
        diffAt(kStrTok[si]);
    }

    // I3：trimmed() 的 Unicode 空白判据穷举码点 0x0000..0x3001（含）
    for (unsigned cp = 0x0000; cp <= 0x3001; ++cp) {
        diffTrimmedCodepoint(cp);
    }

    // contains/startsWith/append：字符串 token 两两全组合
    for (int a = 0; a < kNStrTok; ++a) {
        for (int b = 0; b < kNStrTok; ++b) {
            diffContains(kStrTok[a], kStrTok[b]);
            diffStartsWith(kStrTok[a], kStrTok[b]);
            diffAppend(kStrTok[a], kStrTok[b]);
        }
    }

    // split：字符串 token × 常见分隔符
    for (int si = 0; si < kNStrTok; ++si) {
        for (char sep : {',', ' ', '\0'}) {
            if (sep == '\0') continue;   // char16_t(0) 的 split 形态另外单独覆盖，跳过噪音
            diffSplit(kStrTok[si], sep);
        }
    }

    // arg(PkString)：格式串 × 实参，两两全组合（覆盖 %0/%L/%1/%p 等 token 都在
    // kStrTok 里）
    for (int f = 0; f < kNStrTok; ++f) {
        for (int a = 0; a < kNStrTok; ++a) {
            diffArg1(kStrTok[f], kStrTok[a]);
        }
    }

    // arg(double)：格式串（%1/%L1） × 数值 token（I2）
    for (int fi = 0; fi < kNDblFmtTok; ++fi) {
        for (int vi = 0; vi < kNDblTok; ++vi) {
            diffArgDouble(kDblFmtTok[fi], kDblTok[vi]);
        }
    }

    // arg(a,b) 双参字符串版：格式串 × 字符串 token 两两全组合（I2）
    for (int fi = 0; fi < kNArg2FmtTok; ++fi) {
        for (int a = 0; a < kNStrTok; ++a) {
            for (int b = 0; b < kNStrTok; ++b) {
                diffArg2(kArg2FmtTok[fi], kStrTok[a], kStrTok[b]);
            }
        }
    }

    // arg(a,b,c) 三参字符串版：格式串 × 代表性实参子集三三组合（R-13 补充）
    for (int fi = 0; fi < kNArg3FmtTok; ++fi) {
        for (int ai = 0; ai < kNArg3TokIdx; ++ai) {
            for (int bi = 0; bi < kNArg3TokIdx; ++bi) {
                for (int ci = 0; ci < kNArg3TokIdx; ++ci) {
                    diffArg3(kArg3FmtTok[fi], kStrTok[kArg3TokIdx[ai]], kStrTok[kArg3TokIdx[bi]],
                             kStrTok[kArg3TokIdx[ci]]);
                }
            }
        }
    }

    // arg(int) / arg(int,fieldWidth)：格式串（只取带占位符的几个） × 整数 token
    const char* argFmts[] = {"%1", "%L1", "%0-%1", "n=%1"};
    for (const char* fmt : argFmts) {
        for (int vi = 0; vi < kNIntTok; ++vi) {
            diffArgInt(fmt, kIntTok[vi]);
            for (int fw = 0; fw < kNFieldWidthTok; ++fw) {
                diffArgIntFieldWidth(fmt, kIntTok[vi], kFieldWidthTok[fw]);
            }
        }
    }

    // toInt/toDouble：数值 token 全覆盖
    for (int i = 0; i < kNNumTok; ++i) {
        diffToInt(kNumTok[i]);
        diffToDouble(kNumTok[i]);
    }

    // Unicode default case conversion: first pin the named adversarial shapes
    // from the R-31 plan, including full mappings that change string length.
    const std::vector<std::vector<unsigned>> caseProbes = {
        {},
        {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'O', 'R', 'L', 'D'},
        {0x00C4, 0x03A9, 0x0416},             // BMP Latin / Greek / Cyrillic
        {0x10400, 0x10428},                   // supplementary-plane surrogate pairs
        {'S', 't', 'r', 'a', 0x00DF, 'e'},    // sharp s expands when uppercased
        {0xFB03},                              // ligature expands to FFI
        {0x0130, 'I', 0x0131, 'i'},            // unconditional SpecialCasing
        {0x039F, 0x03A3, 0x039F, 0x03A3},     // Qt default: no final-sigma tailoring
        {0x039C, 0x03AC, 0x03CA, 0x03BF, 0x03C2},
    };
    for (const std::vector<unsigned>& probe : caseProbes) {
        diffCase(probe, "handpicked");
    }

    struct RawCaseProbe {
        const char* shape;
        std::vector<char16_t> units;
    };
    const RawCaseProbe rawCaseProbes[] = {
        {"lone-high", {0xD83D}},
        {"lone-low", {0xDE00}},
        {"high-before-upper-bmp", {0xD83D, u'A'}},
        {"high-before-lower-bmp", {0xD83D, u'a'}},
        {"low-before-upper-bmp", {0xDE00, u'A'}},
        {"low-before-lower-bmp", {0xDE00, u'a'}},
        {"reversed-pair-upper-bmp", {0xDE00, 0xD83D, u'A'}},
        {"reversed-pair-lower-bmp", {0xDE00, 0xD83D, u'a'}},
        {"caseable-before-high", {u'A', 0xD83D}},
        {"valid-pair-before-caseable", {0xD83D, 0xDE00, u'A'}},
        {"mixed-malformed", {u'A', 0xD83D, u'b', 0xDE00, u'C'}},
    };
    for (const RawCaseProbe& probe : rawCaseProbes) {
        diffCaseRawUtf16(probe.units, probe.shape);
    }

    // Single-scalar exhaustive pass catches every Unicode 13 table entry and
    // every identity gap. Surrogate code points are not Unicode scalars.
    for (unsigned cp = 0; cp <= 0x10FFFF; ++cp) {
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        diffCase({cp}, "scalar-exhaustive");
    }

    // Category-token triple product checks ordering and expansion when ASCII,
    // BMP, combining marks, uncased symbols, and astral mappings are adjacent.
    const unsigned caseTokens[] = {
        'A', 'a', 0x00DF, 0x0130, 0x03A3, 0x03C2, 0x0307,
        0x0416, 0x4E2D, 0xFB03, 0x10400, 0x10428,
    };
    for (unsigned a : caseTokens) {
        for (unsigned b : caseTokens) {
            for (unsigned c : caseTokens) {
                diffCase({a, b, c}, "category-product-3");
            }
        }
    }

    for (const auto& kv : g_cover) {
        std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    }
    for (const auto& kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;
}
