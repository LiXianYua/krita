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

#include <cmath>
#include <cstdio>
#include <cstdint>
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
std::string esI(int v) { return std::to_string(v); }
std::string esD(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}
std::string esB(bool v) { return v ? "true" : "false"; }

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
    rec("trimmed", same, "input", sIn, esQ(qr), esP(pr));
}

void diffContains(const char* hay, const char* needle)
{
    QString qh = QString::fromUtf8(hay), qn = QString::fromUtf8(needle);
    PkString ph(hay), pn(needle);
    const bool qr = qh.contains(qn);
    const bool pr = ph.contains(pn);
    const bool same = (qr == pr);
    rec("contains", same, "pair", std::string(hay) + "|" + needle, esB(qr), esB(pr));
}

void diffStartsWith(const char* hay, const char* prefix)
{
    QString qh = QString::fromUtf8(hay), qp = QString::fromUtf8(prefix);
    PkString ph(hay), pp(prefix);
    const bool qr = qh.startsWith(qp);
    const bool pr = ph.startsWith(pp);
    const bool same = (qr == pr);
    rec("startsWith", same, "pair", std::string(hay) + "|" + prefix, esB(qr), esB(pr));
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
    rec("split", same, "sep", std::string(sIn) + "/" + std::string(1, sep), qdump, pdump);
}

void diffArg1(const char* fmt, const char* a)
{
    QString qf = QString::fromUtf8(fmt), qa = QString::fromUtf8(a);
    PkString pf(fmt), pa(a);
    QString qr = qf.arg(qa);
    PkString pr = pf.arg(pa);
    const bool same = (esQ(qr) == esP(pr));
    rec("arg1", same, "fmt", std::string(fmt) + "<-" + a, esQ(qr), esP(pr));
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

int main()
{
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {});
    std::printf("ORACLE-QT file=difftest_string.cpp qVersion=%s\n", qVersion());

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
    }

    // contains/startsWith：字符串 token 两两全组合
    for (int a = 0; a < kNStrTok; ++a) {
        for (int b = 0; b < kNStrTok; ++b) {
            diffContains(kStrTok[a], kStrTok[b]);
            diffStartsWith(kStrTok[a], kStrTok[b]);
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

    for (const auto& kv : g_cover) {
        std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    }
    for (const auto& kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;
}
