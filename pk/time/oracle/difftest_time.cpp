// pk/time/oracle/difftest_time.cpp —— PkDateTime/PkElapsedTimer ↔
// QDateTime/QElapsedTimer 逐输入对拍
//
// R-16 Task 4。形态契约（docs/superpowers/specs/R线-spec.md「对拍怎么做（甲类）」）：
//   - 两侧真的分别 #include <QDateTime>/<QElapsedTimer> 与 "../PkDateTime.h"/
//     "../PkElapsedTimer.h"，static_assert 防止两侧被 compat 垫片解析成同一个
//     类型
//   - -I 绝不能给 pk/time/compat/（run_oracle.sh 保证）
//   - stdout 只有 DIFF total=<N> mismatch=<M>（恰一行）与 DIFFTAG（0..N 行）
//   - 退出码恒为 0
//
// tag 命名：<api> <输入形态>，形态由触发差异的输入构造（规则一）；谓词与
// R-16.deviation 的理由同宽（规则二）——不做"两侧都失败就算过"这种比声明宽的
// 判定。
//
// TZ=UTC 由 run_oracle.sh 显式 export（仿 R-13 的 LC_ALL=C.UTF-8 I4 先例）：
// QDateTime::fromMSecsSinceEpoch 等工厂函数默认 timeSpec()==Qt::LocalTime，
// 渲染日历字段（toString()/fromString() 的年月日时分秒）时走的是系统本地
// 时区；PkDateTime 恒定按 UTC 渲染日历字段（PkDateTime.cpp 顶部注释）。把
// 运行环境的本地时区钉成 UTC，两侧的"本地时区"与"UTC"重合，toSecsSinceEpoch()
// 之外的日历字段比较才有意义、且跨机器可复现。

#include <QDateTime>
#include <QElapsedTimer>
#include <QString>

#include "../PkDateTime.h"
#include "../PkElapsedTimer.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <type_traits>

static_assert(!std::is_same<QDateTime, PkDateTime>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");
static_assert(!std::is_same<QElapsedTimer, PkElapsedTimer>::value,
              "对拍两侧解析成了同一个类型 —— 检查 -I 有没有把 compat/ 带进来");

namespace {

long g_total = 0;
long g_mismatch = 0;
std::map<std::string, long> g_tags;
std::map<std::string, long> g_cover;
long g_printed = 0;

void rec(const std::string &api, bool same, const std::string &tag, const std::string &in,
         const std::string &qs, const std::string &ps)
{
    ++g_total;
    ++g_cover[api];
    if (same) return;
    ++g_mismatch;
    ++g_tags[api + " " + tag];
    if (g_printed < 80) {
        ++g_printed;
        std::printf("MISMATCH: %s [%s] in=%s qt=%s pk=%s\n", api.c_str(), tag.c_str(), in.c_str(),
                    qs.c_str(), ps.c_str());
    }
}

std::string toStd(const QString &s) { return s.toUtf8().toStdString(); }

// ── QDateTime/PkDateTime 的统一"可比较字符串"表示 ──────────────────────
// isValid()==false 时只比 isValid/isNull（两侧字段拆解在无效实例上没有意义，
// PkDateTime 的无效实例甚至不保证 m_time 落在任何合法范围内）；isValid()==true
// 时额外带上 toSecsSinceEpoch()（与日历字段渲染方式无关，纯粹是绝对时刻）。
std::string esQDT(const QDateTime &dt)
{
    if (!dt.isValid()) {
        return std::string("INVALID isNull=") + (dt.isNull() ? "1" : "0");
    }
    return "VALID secs=" + std::to_string(static_cast<long long>(dt.toSecsSinceEpoch()));
}
std::string esPDT(const PkDateTime &dt)
{
    if (!dt.isValid()) {
        return std::string("INVALID isNull=") + (dt.isNull() ? "1" : "0");
    }
    return "VALID secs=" + std::to_string(static_cast<long long>(dt.toSecsSinceEpoch()));
}

// ── 输入 token 表 ──────────────────────────────────────────

// 常规量级的 epoch 秒：覆盖纪元 0、纪元前后 1 天、1970 前（负数秒）、常见
// "正常"日期（含探针钉死的 2024-01-15T12:30:45=1705318245）、9999 年上界、
// 2100 年。
const std::int64_t kSecTok[] = {
    0, 1, -1, 86400, -86400, 1705318245, 1700000000, -2208988800LL /* 1900-01-01 */,
    4102444800LL /* 2100-01-01 */, 253402300799LL /* 9999-12-31T23:59:59 */,
    -62135596800LL /* 0001-01-01，std::tm/timegm 的实际下界附近 */,
};
constexpr int kNSecTok = sizeof(kSecTok) / sizeof(kSecTok[0]);

// 极大 qint64（溢出边界）：直接探 fromSecsSinceEpoch/fromMSecsSinceEpoch 的
// 参数本身撞 int64 表示上限——两侧内部都要做 duration 换算（ms/s → 内部
// TimePoint 的 ns 精度），换算本身可能整数溢出。已用独立最小复现（uboverflow_
// test.cpp / qt_extreme_test.cpp）确认这台机器上 g++ -O0/-O2 与真 Qt5 都不会
// 崩溃、只是产出经典的有符号整数回绕值——真正对拍关心的是"回绕之后两侧是否
// 落到同一个值"，探出来落到不同值本身就是合法的偏离，不是对拍程序的 bug。
const std::int64_t kExtremeTok[] = {
    std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min(),
    std::numeric_limits<std::int64_t>::max() / 2, std::numeric_limits<std::int64_t>::min() / 2,
};
constexpr int kNExtremeTok = sizeof(kExtremeTok) / sizeof(kExtremeTok[0]);

std::string classifySec(std::int64_t s)
{
    if (s == 0) return "zero";
    if (s == std::numeric_limits<std::int64_t>::max()) return "int64-max";
    if (s == std::numeric_limits<std::int64_t>::min()) return "int64-min";
    if (s > 1000000000000LL || s < -1000000000000LL) return "extreme";
    if (s < 0) return "pre-1970";
    return "post-1970";
}

} // namespace

// ── fromSecsSinceEpoch / fromMSecsSinceEpoch / toSecsSinceEpoch / isValid /
//    isNull ─────────────────────────────────────────────────────────────

void diffFromSecsSinceEpoch(std::int64_t secs)
{
    QDateTime qdt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs), Qt::LocalTime);
    PkDateTime pdt = PkDateTime::fromSecsSinceEpoch(secs);
    const std::string qs = esQDT(qdt);
    const std::string ps = esPDT(pdt);
    rec("fromSecsSinceEpoch", qs == ps, classifySec(secs), std::to_string(secs), qs, ps);
}

void diffFromMSecsSinceEpoch(std::int64_t msecs)
{
    QDateTime qdt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(msecs), Qt::LocalTime);
    PkDateTime pdt = PkDateTime::fromMSecsSinceEpoch(msecs);
    const std::string qs = esQDT(qdt);
    const std::string ps = esPDT(pdt);
    rec("fromMSecsSinceEpoch", qs == ps, classifySec(msecs), std::to_string(msecs), qs, ps);
}

// ── operator== / secsTo（两两组合，覆盖同值/异值/默认构造）─────────────

void diffOperatorEq(std::int64_t secsA, std::int64_t secsB)
{
    QDateTime qa = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secsA), Qt::LocalTime);
    QDateTime qb = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secsB), Qt::LocalTime);
    PkDateTime pa = PkDateTime::fromSecsSinceEpoch(secsA);
    PkDateTime pb = PkDateTime::fromSecsSinceEpoch(secsB);
    const bool qr = (qa == qb);
    const bool pr = (pa == pb);
    const std::string tag = std::string("equal-input=") + (secsA == secsB ? "1" : "0");
    rec("operator==", qr == pr, tag, std::to_string(secsA) + "/" + std::to_string(secsB),
        qr ? "true" : "false", pr ? "true" : "false");
}

void diffOperatorEqDefault()
{
    QDateTime qa, qb;
    PkDateTime pa, pb;
    const bool qr = (qa == qb);
    const bool pr = (pa == pb);
    rec("operator==", qr == pr, "both-default", "default/default", qr ? "true" : "false",
        pr ? "true" : "false");
}

void diffSecsTo(std::int64_t secsA, std::int64_t secsB)
{
    QDateTime qa = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secsA), Qt::LocalTime);
    QDateTime qb = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secsB), Qt::LocalTime);
    PkDateTime pa = PkDateTime::fromSecsSinceEpoch(secsA);
    PkDateTime pb = PkDateTime::fromSecsSinceEpoch(secsB);
    const qint64 qr = qa.secsTo(qb);
    const std::int64_t pr = pa.secsTo(pb);
    const std::string tag = std::string("dir=") + (secsA < secsB ? "a<b" : (secsA > secsB ? "a>b" : "a==b"));
    rec("secsTo", static_cast<std::int64_t>(qr) == pr, tag,
        std::to_string(secsA) + "->" + std::to_string(secsB), std::to_string(static_cast<long long>(qr)),
        std::to_string(pr));
}

// ── isValid()/isNull() 的默认构造对照（规则一：不是单独一个字面量常量，
//    tag 编码"哪一种构造路径"）──────────────────────────────────────────

void diffDefaultConstructed()
{
    QDateTime qdt;
    PkDateTime pdt;
    const std::string tag = "ctor=default";
    rec("isValid", qdt.isValid() == pdt.isValid(), tag, "default", qdt.isValid() ? "true" : "false",
        pdt.isValid() ? "true" : "false");
    rec("isNull", qdt.isNull() == pdt.isNull(), tag, "default", qdt.isNull() ? "true" : "false",
        pdt.isNull() ? "true" : "false");
}

// ── currentDateTime()/currentDateTimeUtc()：两侧几乎同时取"现在"，不能字面量
//    比较——比较"取到的都是有效值、且两者相差在容忍窗口内"这条关系性质
//    （spec"对拍怎么做"允许对 elapsed 类不比绝对值只比关系，currentDateTime 同理
//    ——它同样是"墙钟当前时刻"，两次独立调用之间必然有微小的真实时间差）。────

void diffCurrentDateTime(const char *api, QDateTime (*qtCall)(), PkDateTime (*pkCall)())
{
    QDateTime qdt = qtCall();
    PkDateTime pdt = pkCall();
    const bool qOk = qdt.isValid();
    const bool pOk = pdt.isValid();
    const std::int64_t diffSecs =
        std::llabs(static_cast<long long>(qdt.toSecsSinceEpoch()) - static_cast<long long>(pdt.toSecsSinceEpoch()));
    // 容忍窗口：5 秒，远大于两次调用之间真实经过的时间（同进程内背靠背调用），
    // 只用来确认"两侧真的都在取同一个数量级的当前时刻"，不是字面量相等。
    const bool same = (qOk == pOk) && qOk && (diffSecs <= 5);
    rec(api, same, "near-now", "now",
        qOk ? ("valid secs=" + std::to_string(static_cast<long long>(qdt.toSecsSinceEpoch()))) : "invalid",
        pOk ? ("valid secs=" + std::to_string(static_cast<long long>(pdt.toSecsSinceEpoch()))) : "invalid");
}

// ── toString() / toString(DateFormat) ───────────────────────────────────

void diffToStringDefault(std::int64_t secs)
{
    QDateTime qdt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs), Qt::LocalTime);
    PkDateTime pdt = PkDateTime::fromSecsSinceEpoch(secs);
    const std::string qs = toStd(qdt.toString());
    const std::string ps = pdt.toString();
    rec("toString-default", qs == ps, classifySec(secs), std::to_string(secs), qs, ps);
}

void diffToStringDefaultInvalid()
{
    QDateTime qdt;
    PkDateTime pdt;
    const std::string qs = toStd(qdt.toString());
    const std::string ps = pdt.toString();
    rec("toString-default", qs == ps, "invalid", "default", qs, ps);
}

const char *fmtName(PkDateTime::DateFormat fmt)
{
    switch (fmt) {
    case PkDateTime::DateFormat::ISODate: return "ISODate";
    case PkDateTime::DateFormat::RFC2822Date: return "RFC2822Date";
    case PkDateTime::DateFormat::ISODateWithMs: return "ISODateWithMs";
    }
    return "?";
}

Qt::DateFormat toQtFmt(PkDateTime::DateFormat fmt)
{
    switch (fmt) {
    case PkDateTime::DateFormat::ISODate: return Qt::ISODate;
    case PkDateTime::DateFormat::RFC2822Date: return Qt::RFC2822Date;
    case PkDateTime::DateFormat::ISODateWithMs: return Qt::ISODateWithMs;
    }
    return Qt::ISODate;
}

void diffToStringFormat(std::int64_t secs, PkDateTime::DateFormat fmt)
{
    QDateTime qdt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs), Qt::LocalTime);
    PkDateTime pdt = PkDateTime::fromSecsSinceEpoch(secs);
    const std::string qs = toStd(qdt.toString(toQtFmt(fmt)));
    const std::string ps = pdt.toString(fmt);
    const std::string tag = std::string("fmt=") + fmtName(fmt) + "/" + classifySec(secs);
    rec("toString-fmt", qs == ps, tag, std::to_string(secs), qs, ps);
}

void diffToStringFormatInvalid(PkDateTime::DateFormat fmt)
{
    QDateTime qdt;
    PkDateTime pdt;
    const std::string qs = toStd(qdt.toString(toQtFmt(fmt)));
    const std::string ps = pdt.toString(fmt);
    const std::string tag = std::string("fmt=") + fmtName(fmt) + "/invalid";
    rec("toString-fmt", qs == ps, tag, "default", qs, ps);
}

// ── fromString(s)：无格式兜底，只认 "Www Mmm d hh:mm:ss yyyy" ──────────

void diffFromStringDefault(const std::string &s)
{
    QDateTime qdt = QDateTime::fromString(QString::fromStdString(s));
    PkDateTime pdt = PkDateTime::fromString(s);
    const std::string qs = esQDT(qdt);
    const std::string ps = esPDT(pdt);
    // 形态分类：空/纯垃圾/token 数目不对/合法形态。
    std::string shape;
    if (s.empty()) {
        shape = "empty";
    } else {
        int spaces = 0;
        for (char c : s) {
            if (c == ' ') ++spaces;
        }
        shape = "tokens=" + std::to_string(spaces + 1);
    }
    rec("fromString-default", qs == ps, shape, s, qs, ps);
}

// ── fromString(s, customFormat)：5 个具体格式串 ─────────────────────────

void diffFromStringCustom(const std::string &s, const std::string &fmt)
{
    QDateTime qdt = QDateTime::fromString(QString::fromStdString(s), QString::fromStdString(fmt));
    PkDateTime pdt = PkDateTime::fromString(s, fmt);
    const std::string qs = esQDT(qdt);
    const std::string ps = esPDT(pdt);
    const std::string tag = "fmt=" + fmt + "/len=" + std::to_string(s.size());
    rec("fromString-custom", qs == ps, tag, s, qs, ps);
}

// ── fromString(s, DateFormat) —— 只支持 ISODate 一支 ────────────────────

void diffFromStringISODate(const std::string &s)
{
    QDateTime qdt = QDateTime::fromString(QString::fromStdString(s), Qt::ISODate);
    PkDateTime pdt = PkDateTime::fromString(s, PkDateTime::DateFormat::ISODate);
    const std::string qs = esQDT(qdt);
    const std::string ps = esPDT(pdt);
    rec("fromString-ISODate", qs == ps, "len=" + std::to_string(s.size()), s, qs, ps);
}

// ── PkElapsedTimer ↔ QElapsedTimer：只对拍"归零/单调/数量级关系"性质，不比
//    绝对毫秒数字面量（依赖真实睡眠，两次调用取的不是同一个时钟读数）────

void diffElapsedRelational()
{
    // isValid() 生命周期：默认无效 → start() 后有效 → invalidate() 后无效。
    QElapsedTimer qt;
    PkElapsedTimer pt;
    rec("elapsed-isValid", qt.isValid() == pt.isValid(), "before-start", "n/a",
        qt.isValid() ? "true" : "false", pt.isValid() ? "true" : "false");
    qt.start();
    pt.start();
    rec("elapsed-isValid", qt.isValid() == pt.isValid(), "after-start", "n/a", qt.isValid() ? "true" : "false",
        pt.isValid() ? "true" : "false");
    qt.invalidate();
    pt.invalidate();
    rec("elapsed-isValid", qt.isValid() == pt.isValid(), "after-invalidate", "n/a",
        qt.isValid() ? "true" : "false", pt.isValid() ? "true" : "false");
}

void diffElapsedSequence(int sleepMs)
{
    QElapsedTimer qt;
    PkElapsedTimer pt;
    qt.start();
    pt.start();
    // 归零性质：start() 立即之后 elapsed() 应当很小（用宽松上界而不是 ==0，
    // 避免进程调度抖动把"立即"变成几毫秒偶发失败）。
    const bool qImmediateOk = qt.elapsed() < 50;
    const bool pImmediateOk = pt.elapsed() < 50;
    rec("elapsed-seq", qImmediateOk == pImmediateOk, "immediate-small/sleep=" + std::to_string(sleepMs),
        "start", qImmediateOk ? "small" : "NOT-small", pImmediateOk ? "small" : "NOT-small");

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

    const qint64 qElapsed = qt.elapsed();
    const std::int64_t pElapsed = pt.elapsed();
    // 数量级关系：真实睡眠 sleepMs 之后，两侧 elapsed() 都应当 >= 0 且与
    // sleepMs 同数量级——容忍窗口给 30ms（覆盖线程调度抖动、两次 start() 之间
    // 的极小时间差），不比字面量相等。sleepMs=0 时只要求两侧都 >=0（不要求
    // 严格 <某个值，调度延迟可能让 0ms 睡眠实际耗时到两位数毫秒）。
    const bool qNonNeg = qElapsed >= 0;
    const bool pNonNeg = pElapsed >= 0;
    const std::int64_t diffMs = std::llabs(static_cast<long long>(qElapsed) - static_cast<long long>(pElapsed));
    const bool magnitudeOk = diffMs <= 30;
    const std::string tag = "post-sleep/sleep=" + std::to_string(sleepMs);
    rec("elapsed-seq", qNonNeg && pNonNeg && magnitudeOk, tag,
        "sleep=" + std::to_string(sleepMs), std::to_string(static_cast<long long>(qElapsed)),
        std::to_string(pElapsed));

    // nsecsElapsed() 与 elapsed() 的数量级关系（约差 1e6）：两侧各自内部关系
    // 一致即可，不跨侧比较绝对纳秒数。
    const qint64 qNs = qt.nsecsElapsed();
    const std::int64_t pNs = pt.nsecsElapsed();
    const bool qRatioOk = (qElapsed == 0) || (qNs / qElapsed > 100000 && qNs / qElapsed < 10000000);
    const bool pRatioOk = (pElapsed == 0) || (pNs / pElapsed > 100000 && pNs / pElapsed < 10000000);
    rec("elapsed-ns-ratio", qRatioOk == pRatioOk, tag, "sleep=" + std::to_string(sleepMs),
        qRatioOk ? "ratio-ok" : "ratio-BAD", pRatioOk ? "ratio-ok" : "ratio-BAD");

    // restart()：真的把基点归零，不是只返回旧值不重置——sleepMs 之后 restart()
    // 返回值应当与 elapsed() 同数量级（都约等于 sleepMs），随后立即 elapsed()
    // 应当很小（归零生效）。
    const qint64 qRestart = qt.restart();
    const std::int64_t pRestart = pt.restart();
    const std::int64_t restartDiffMs =
        std::llabs(static_cast<long long>(qRestart) - static_cast<long long>(pRestart));
    rec("elapsed-restart", restartDiffMs <= 30, tag, "sleep=" + std::to_string(sleepMs),
        std::to_string(static_cast<long long>(qRestart)), std::to_string(pRestart));

    const bool qAfterRestartSmall = qt.elapsed() < 50;
    const bool pAfterRestartSmall = pt.elapsed() < 50;
    rec("elapsed-restart-resets", qAfterRestartSmall == pAfterRestartSmall,
        "immediate-after-restart/sleep=" + std::to_string(sleepMs), "restart",
        qAfterRestartSmall ? "small" : "NOT-small", pAfterRestartSmall ? "small" : "NOT-small");
}

int main()
{
    const char *tz = std::getenv("TZ");
    std::printf("ORACLE-QT file=difftest_time.cpp qVersion=%s TZ=%s\n", qVersion(), tz != nullptr ? tz : "(unset)");

    // ── fromSecsSinceEpoch / fromMSecsSinceEpoch：常规量级 + 极大 qint64 ──
    for (int i = 0; i < kNSecTok; ++i) {
        diffFromSecsSinceEpoch(kSecTok[i]);
        diffFromMSecsSinceEpoch(kSecTok[i]);
        diffFromMSecsSinceEpoch(kSecTok[i] * 1000);
    }
    for (int i = 0; i < kNExtremeTok; ++i) {
        diffFromSecsSinceEpoch(kExtremeTok[i]);
        diffFromMSecsSinceEpoch(kExtremeTok[i]);
    }

    // ── isValid()/isNull() 默认构造 ──
    diffDefaultConstructed();

    // ── operator== / secsTo：两两组合（含默认构造）──
    for (int i = 0; i < kNSecTok; ++i) {
        for (int j = 0; j < kNSecTok; ++j) {
            diffOperatorEq(kSecTok[i], kSecTok[j]);
            diffSecsTo(kSecTok[i], kSecTok[j]);
        }
    }
    diffOperatorEqDefault();

    // ── currentDateTime()/currentDateTimeUtc() ──
    for (int i = 0; i < 5; ++i) {
        diffCurrentDateTime("currentDateTime", []() { return QDateTime::currentDateTime(); },
                             []() { return PkDateTime::currentDateTime(); });
        diffCurrentDateTime("currentDateTimeUtc", []() { return QDateTime::currentDateTimeUtc(); },
                             []() { return PkDateTime::currentDateTimeUtc(); });
    }

    // ── toString() / toString(DateFormat) ──
    for (int i = 0; i < kNSecTok; ++i) {
        diffToStringDefault(kSecTok[i]);
        diffToStringFormat(kSecTok[i], PkDateTime::DateFormat::ISODate);
        diffToStringFormat(kSecTok[i], PkDateTime::DateFormat::RFC2822Date);
        diffToStringFormat(kSecTok[i], PkDateTime::DateFormat::ISODateWithMs);
    }
    diffToStringDefaultInvalid();
    diffToStringFormatInvalid(PkDateTime::DateFormat::ISODate);
    diffToStringFormatInvalid(PkDateTime::DateFormat::RFC2822Date);
    diffToStringFormatInvalid(PkDateTime::DateFormat::ISODateWithMs);

    // ── fromString(s)：手挑对抗 + token 数量/形态组合爆破 ──
    const char *const kDefaultAdversarial[] = {
        "",
        " ",
        "garbage",
        "Wed May 20 03:40:13 2015",       // 探针钉死的合法样例
        "wed may 20 03:40:13 2015",       // 大小写不匹配月/星期缩写
        "Wed Foo 20 03:40:13 2015",       // 非法月份缩写
        "Wed May 20 03:40:13 2015 extra", // 第 6 个 token
        "Wed May 20 03:40:13",            // 缺年份 token
        "Wed May 5 03:40:13 2015",        // 单数日
        "Wed May 32 03:40:13 2015",       // 越界日
        "Wed May 20 3:40:13 2015",        // 单数小时（非 hh:mm:ss 两位格式）
        "Wed May 20 03-40-13 2015",       // 分隔符不对
        "Wed May 20 25:00:00 2015",       // 越界小时
        "Wed May 20 03:40:13 99999",      // 5 位年份
    };
    for (const char *s : kDefaultAdversarial) {
        diffFromStringDefault(s);
    }
    // 组合爆破：月缩写 × 日 × 时间串 × 年份，星期缩写固定 "Wed"（星期本身不参与
    // 校验，makeFromUtcFieldsChecked 不检查星期与日期是否吻合）。
    const char *const kMonthAbbrevs[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char *const kDayFields[] = {"1", "09", "15", "31", "32", "00"};
    const char *const kTimeFields[] = {"00:00:00", "12:30:45", "23:59:59", "24:00:00", "12:60:00", "12:00:60"};
    const char *const kYearFields[] = {"0000", "1970", "2024", "9999"};
    for (const char *mon : kMonthAbbrevs) {
        for (const char *day : kDayFields) {
            for (const char *tm : kTimeFields) {
                for (const char *yr : kYearFields) {
                    const std::string s = std::string("Wed ") + mon + " " + day + " " + tm + " " + yr;
                    diffFromStringDefault(s);
                }
            }
        }
    }

    // ── fromString(s, customFormat)：边界长度手挑 + 跨格式组合爆破 + 字段扫描 ──
    struct FmtSpec { const char *fmt; std::size_t len; };
    const FmtSpec kFmts[] = {
        {"yyyy", 4}, {"yyyy-MM", 7}, {"yyyy-MM-dd", 10}, {"yyyy-MM-ddThh:mm", 16},
        {"yyyy-MM-ddThh:mm:ss", 19},
    };
    const char *const kFullSample = "2024-06-15T12:30:45"; // 19 字符，逐格式截取当样本
    for (const auto &spec : kFmts) {
        // 边界长度：阈值-1 / 阈值 / 阈值+1（阈值那条用 kFullSample 的前缀，保证
        // 内容本身合法，只有长度在边界上变化；阈值±1 通过截断/补一个数字字符
        // 构造，触发的是"长度判断分支"本身，不是内容合不合法）。
        const std::string exact = std::string(kFullSample).substr(0, spec.len);
        diffFromStringCustom(exact, spec.fmt);                                   // ==len
        diffFromStringCustom(exact.substr(0, spec.len - 1), spec.fmt);           // len-1
        diffFromStringCustom(exact + "9", spec.fmt);                             // len+1
    }
    // 跨格式组合爆破：每个格式串 × 一组不同长度/内容的输入（覆盖空、全数字、
    // 各格式的"正确形态"字符串、纯垃圾串）。
    const char *const kCrossInputs[] = {
        "", "0", "2024", "2024-06", "2024-06-15", "2024-06-15T12:30", "2024-06-15T12:30:45",
        "garbage", "9999-99-99T99:99:99", "0000-01-01T00:00:00", "2024/06/15", "abcd-ef-ghTij:kl:mn",
    };
    for (const auto &spec : kFmts) {
        for (const char *in : kCrossInputs) {
            diffFromStringCustom(in, spec.fmt);
        }
    }
    // 字段扫描（单因子逐个扫，避免全交叉的组合爆炸）：只对最长格式
    // "yyyy-MM-ddThh:mm:ss" 做，基线合法字段 2024-06-15T12:30:45，逐个字段替换
    // 成边界值，验证 fieldsInRange 的粗粒度校验与真实 QDateTime 的精确校验
    // （比如"某月最多几天"）在哪里分岔。
    struct FieldSweep { const char *label; const char *value; };
    const FieldSweep kYearSweep[] = {{"y", "0000"}, {"y", "0001"}, {"y", "1969"}, {"y", "1970"}, {"y", "2024"}, {"y", "9999"}};
    const FieldSweep kMonthSweep[] = {{"m", "00"}, {"m", "01"}, {"m", "06"}, {"m", "12"}, {"m", "13"}};
    const FieldSweep kDaySweep[] = {{"d", "00"}, {"d", "01"}, {"d", "15"}, {"d", "30"}, {"d", "31"}, {"d", "32"}};
    const FieldSweep kHourSweep[] = {{"h", "00"}, {"h", "12"}, {"h", "23"}, {"h", "24"}};
    const FieldSweep kMinSweep[] = {{"mi", "00"}, {"mi", "30"}, {"mi", "59"}, {"mi", "60"}};
    const FieldSweep kSecSweep[] = {{"s", "00"}, {"s", "30"}, {"s", "59"}, {"s", "60"}};
    auto build = [](const char *y, const char *mo, const char *d, const char *h, const char *mi,
                     const char *s) {
        return std::string(y) + "-" + mo + "-" + d + "T" + h + ":" + mi + ":" + s;
    };
    for (const auto &f : kYearSweep) diffFromStringCustom(build(f.value, "06", "15", "12", "30", "45"), "yyyy-MM-ddThh:mm:ss");
    for (const auto &f : kMonthSweep) diffFromStringCustom(build("2024", f.value, "15", "12", "30", "45"), "yyyy-MM-ddThh:mm:ss");
    for (const auto &f : kDaySweep) diffFromStringCustom(build("2024", "06", f.value, "12", "30", "45"), "yyyy-MM-ddThh:mm:ss");
    for (const auto &f : kHourSweep) diffFromStringCustom(build("2024", "06", "15", f.value, "30", "45"), "yyyy-MM-ddThh:mm:ss");
    for (const auto &f : kMinSweep) diffFromStringCustom(build("2024", "06", "15", "12", f.value, "45"), "yyyy-MM-ddThh:mm:ss");
    for (const auto &f : kSecSweep) diffFromStringCustom(build("2024", "06", "15", "12", "30", f.value), "yyyy-MM-ddThh:mm:ss");
    // 一个真实"某月最多几天"边界：2024 是闰年，Feb 29 合法、Feb 30 不合法
    // （真实 Qt 会拒绝，PkDateTime 的 fieldsInRange 只查 1<=day<=31 不查月份，
    // 预期在这里分岔——这正是对拍要抓的"粗粒度校验 vs 精确校验"差异）。
    diffFromStringCustom("2024-02-29T00:00:00", "yyyy-MM-ddThh:mm:ss");
    diffFromStringCustom("2024-02-30T00:00:00", "yyyy-MM-ddThh:mm:ss");
    diffFromStringCustom("2023-02-29T00:00:00", "yyyy-MM-ddThh:mm:ss"); // 2023 非闰年

    // ── fromString(s, DateFormat)：只 ISODate 一支，复用一部分 19 字符样本 ──
    const char *const kIsoInputs[] = {
        "2024-06-15T12:30:45", "", "garbage", "2024-06-15T12:30:4", "2024-06-15T12:30:456",
        "0000-01-01T00:00:00", "9999-12-31T23:59:59",
    };
    for (const char *s : kIsoInputs) {
        diffFromStringISODate(s);
    }

    // ── PkElapsedTimer ↔ QElapsedTimer ──
    diffElapsedRelational();
    for (int sleepMs : {0, 1, 5, 20, 50}) {
        diffElapsedSequence(sleepMs);
    }

    for (const auto &kv : g_cover) {
        std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    }
    for (const auto &kv : g_tags) {
        std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    }
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;
}
