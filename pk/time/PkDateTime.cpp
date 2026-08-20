#include "PkDateTime.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

// ============================================================================
// R-29 Task 3：PkDateTime 重写为 calendar 版（m_date + m_time），保 epoch API。
//
// 内部表示从「单 TimePoint」换成「m_date + m_time」（对齐 Qt QDateTime 的
// QDateTimePrivate 模型），但 epoch 换算走内部 helper 把 (date,time) 转成
// TimePoint 再取 epoch。**毫秒是 PkTime 的组成部分（m_time.msec()），不额外存
// 独立字段**。
//
// 日历字段（年/月/日/时/分/秒）↔ TimePoint 的换算统一按 **LocalTime** 处理，
// 对齐真 Qt 默认 `timeSpec()==Qt::LocalTime`（`fromSecsSinceEpoch`/
// `fromMSecsSinceEpoch`/`fromString` 单参默认 LocalTime、`toString` 按本地墙钟
// 渲染——2026-08-18 裁决，见 `R线-spec.md`「PkDateTime 时区」一节）。全部走
// C 库 `localtime_r`/`mktime`（读系统 `TZ`）：C++17 `std::chrono` 无 tzdb，拿
// 不到系统时区偏移，退用 C 库是唯一可用的本地时区来源。这是对 Qt 自带 tzdb 的
// 近似——若将来某真实调用点依赖精确 tzdb 语义（历史 DST 表、未来法规变更、
// DST 切换时刻的歧义解析等），按偏离登记，别当作"已经对齐了"。
//
// RFC2822Date 的时区尾缀随本地时区输出（`localtime_r` 填的 `tm_gmtoff`，秒、
// 东正西负），不再是硬编码 "+0000"——toString(RFC2822Date) 用 toTimePoint()
// 重建 TimePoint → localtime_r 拆 tm_gmtoff，主体字段直接从 m_date/m_time 渲染。
//
// epoch 语义对齐 Qt 探针钉死的行为：
//   - toSecsSinceEpoch == toMSecsSinceEpoch() / 1000（C++ 整数除法向零截断）——
//     fromMSecsSinceEpoch(-1999).toSecsSinceEpoch()==-1、(-999)→0、(-1)→0（探针实测）。
//     2026-08-19 终审修复（final-fix-findings F2）：fromTimePoint 对负亚秒做 floor 借位后
//     toMSecsSinceEpoch() 精确往返，toSecsSinceEpoch 直接用它除 1000 即得 Qt 语义
//     （旧实现 duration_cast<seconds> 对整秒重建截断，在 F2 借位下会给 -1999ms → -2，
//     与 Qt 的 -1 不符）。
//   - fromSecsSinceEpoch 直接 TimePoint(seconds(secs))，不 secs*1000（避免
//     oracle kExtremeTok 的 INT64_MAX 有符号溢出 UB）。
// ============================================================================

namespace {

const char *const kDayNames[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *const kMonthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool isAllDigits(const std::string &s, std::size_t pos, std::size_t len)
{
    if (pos + len > s.size()) return false;
    for (std::size_t i = 0; i < len; ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[pos + i])) == 0) return false;
    }
    return true;
}

int parseIntField(const std::string &s, std::size_t pos, std::size_t len)
{
    return std::stoi(s.substr(pos, len));
}

int monthNumberFromAbbrev(const std::string &abbrev)
{
    for (int i = 0; i < 12; ++i) {
        if (abbrev == kMonthNames[i]) return i + 1;
    }
    return -1;
}

// 粗粒度范围校验（不做"某月最多几天"这种精确闰年校验）——够用来挡住
// 明显不合法的字段组合（如月份 13），真实调用点（EXIF/元数据日期串）不会
// 产出这类输入，过度校验不划算。R-16 Task 4 对拍已实测确认这条设计选择本身
// （day 不做闰年/每月天数精确校验、year 不禁 0）：真实 Qt 会拒绝
// "2024-02-30"/"2023-02-29"（非闰年）/"0000-01-01" 这类输入，PkDateTime 按本
// 函数的粗粒度校验接受——属于已裁定、已在本注释写明理由的范围决策，不是本轮
// 新发现，登记见 pk/time/oracle/R-16.deviation。
bool fieldsInRange(int month, int day, int hour, int minute, int second)
{
    return month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 &&
           second >= 0 && second <= 59;
}

// R-16 Task 4 对拍逼出的真实根因修复（不是范围决策，是内部表示的正确性
// 缺口）：PkDateTime 内部用 std::chrono::system_clock 存绝对时刻，这台机器
// 上（以及绝大多数 libstdc++/libc++ 实现）它的原生 duration 精度是纳秒、
// 用 int64 计数——INT64_MAX ns ≈ 292.28 年，可安全表示的窗口只有约
// [1677.72, 2262.28]（对拍程序 pk/time/oracle/difftest_time.cpp 用独立最小
// 复现验证过这个边界的来源与数值）。超出这个窗口时，"日历字段 → time_t →
// TimePoint"这条换算链会发生有符号整数回绕：不是"拒绝"，是**悄悄给出一个
// 貌似合法、实际上错得离谱的时刻**。真实调用点（EXIF/XMP 元数据日期）不会落
// 在这个窗口之外，但"看起来合法的输入不能悄悄给错答案"这条原则不能因为
// "用不到"就放弃——用一次朴素的年份区间校验把它变成"明确拒绝"
// （isValid()==false），而不是放着悄悄腐化数据。
bool yearRepresentable(int year)
{
    return year >= 1678 && year <= 2261;
}

// RFC2822Date 的时区尾缀：从 `localtime_r` 填的 `tm_gmtoff`（秒、东正西负）
// 格式化成 "±hhmm"。`tm_gmtoff` 是 glibc/BSD 扩展，`localtime_r` 会按当前 DST
// 状态正确设置它。Qt 的 RFC2822 尾缀正是这个本地偏移（探针实测：LA →
// "-0800"、Shanghai → "+0800"、Kolkata 半时区 → "+0530"、UTC → "+0000"）。
std::string formatRfc2822Offset(const std::tm &tmVal)
{
    long off = tmVal.tm_gmtoff;
    const char sign = off < 0 ? '-' : '+';
    off = std::labs(off);
    const int hh = static_cast<int>(off / 3600);
    const int mm = static_cast<int>((off % 3600) / 60);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%02d%02d", sign, hh, mm);
    return std::string(buf);
}

} // namespace

// ── PkDate 实现（照 Qt qdatetime.cpp 的格里历算法）──────────────────────────

// 从 jd 拆出年月日（标准格里历算法，照 Qt QDatePrivate::getDateFromJulianDay）。
static void getDateFromJulianDay(std::int64_t jd, int &year, int &month, int &day)
{
    std::int64_t a = jd + 32044;
    std::int64_t b = (4 * a + 3) / 146097;
    std::int64_t c = a - (146097 * b) / 4;
    std::int64_t d = (4 * c + 3) / 1461;
    std::int64_t e = c - (1461 * d) / 4;
    std::int64_t m = (5 * e + 2) / 153;
    day = static_cast<int>(e - (153 * m + 2) / 5 + 1);
    month = static_cast<int>(m + 3 - 12 * (m / 10));
    year = static_cast<int>(100 * b + d - 4800 + m / 10);
}

// 从年月日算 jd（标准格里历算法，照 Qt QDatePrivate::gregorianToJulianDay）。
static std::int64_t julianDayFromGregorian(int y, int m, int d)
{
    if (m <= 2) { y -= 1; m += 12; }
    std::int64_t a = y / 100;
    std::int64_t b = 2 - a + a / 4;
    return static_cast<std::int64_t>(365 * (y + 4716)) + (y + 4716) / 4
           + static_cast<std::int64_t>(153 * (m + 1) / 5) + d + b - 1524;
}

static int daysInMonth(int y, int m)
{
    static const int kDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && PkDate::isLeapYear(y)) return 29;
    return kDaysInMonth[m - 1];
}

PkDate::PkDate(int y, int m, int d)
{
    if (!isValid(y, m, d)) { m_jd = nullJd(); return; }
    m_jd = julianDayFromGregorian(y, m, d);
}

int PkDate::year() const { if (!isValid()) return 0; int y, m, d; getDateFromJulianDay(m_jd, y, m, d); return y; }
int PkDate::month() const { if (!isValid()) return 0; int y, m, d; getDateFromJulianDay(m_jd, y, m, d); return m; }
int PkDate::day() const { if (!isValid()) return 0; int y, m, d; getDateFromJulianDay(m_jd, y, m, d); return d; }
int PkDate::dayOfWeek() const
{
    if (!isValid()) return 0;
    // Qt: (jd % 7) + 1，jd=0 → 星期一（1）
    const std::int64_t r = m_jd % 7;
    return static_cast<int>(r < 0 ? r + 8 : r + 1);
}
int PkDate::dayOfYear() const { if (!isValid()) return 0; return static_cast<int>(m_jd - julianDayFromGregorian(year(), 1, 1)) + 1; }
int PkDate::daysInMonth() const { if (!isValid()) return 0; return ::daysInMonth(year(), month()); }
int PkDate::daysInYear() const { if (!isValid()) return 0; return isLeapYear(year()) ? 366 : 365; }

bool PkDate::setDate(int year, int month, int day)
{
    if (!isValid(year, month, day)) { m_jd = nullJd(); return false; }
    m_jd = julianDayFromGregorian(year, month, day);
    return true;
}

PkDate PkDate::addDays(std::int64_t days) const
{
    if (!isValid()) return PkDate();
    const std::int64_t njd = m_jd + days;
    return (njd >= minJd() && njd <= maxJd()) ? PkDate(njd) : PkDate();
}
PkDate PkDate::addMonths(int months) const
{
    if (!isValid()) return PkDate();
    int y, m, d;
    getDateFromJulianDay(m_jd, y, m, d);
    // Qt: 月份算术（跨年进位/借位，日钳到当月天数）
    int ny = y + months / 12;
    int nm = m + months % 12;
    if (nm <= 0) { nm += 12; ny -= 1; }
    else if (nm > 12) { nm -= 12; ny += 1; }
    const int nd = std::min(d, ::daysInMonth(ny, nm));
    return PkDate(ny, nm, nd);
}
PkDate PkDate::addYears(int years) const
{
    if (!isValid()) return PkDate();
    int y, m, d;
    getDateFromJulianDay(m_jd, y, m, d);
    const int nd = std::min(d, ::daysInMonth(y + years, m));
    return PkDate(y + years, m, nd);
}
std::int64_t PkDate::daysTo(const PkDate &other) const
{
    if (!isValid() || !other.isValid()) return 0;
    return other.m_jd - m_jd;
}

PkDate PkDate::currentDate()
{
    // 从系统当前时刻拆出本地日历字段
    const std::time_t t = std::time(nullptr);
    std::tm tmVal{};
    localtime_r(&t, &tmVal);
    return PkDate(tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday);
}

bool PkDate::isValid(int y, int m, int d)
{
    // Qt: 年份范围对齐 minJd/maxJd 的格里历换算
    if (y < 1 || y > 9999) return false;
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > 31) return false;
    if (d > ::daysInMonth(y, m)) return false;
    return true;
}
bool PkDate::isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// ── PkTime 实现（照 Qt qdatetime.cpp）─────────────────────────────────────

PkTime::PkTime(int h, int m, int s, int ms)
{
    if (!isValid(h, m, s, ms)) { m_mds = NullTime; return; }
    m_mds = h * 3600000 + m * 60000 + s * 1000 + ms;
}

bool PkTime::isValid() const { return m_mds >= 0 && m_mds < 86400000; }

bool PkTime::setHMS(int h, int m, int s, int ms)
{
    if (!isValid(h, m, s, ms)) { m_mds = NullTime; return false; }
    m_mds = h * 3600000 + m * 60000 + s * 1000 + ms;
    return true;
}

PkTime PkTime::addSecs(int secs) const
{
    if (!isValid()) return PkTime();
    const int n = m_mds + secs * 1000;
    const int wrapped = ((n % 86400000) + 86400000) % 86400000;
    return PkTime(wrapped);
}
PkTime PkTime::addMSecs(int ms) const
{
    if (!isValid()) return PkTime();
    const int n = m_mds + ms;
    const int wrapped = ((n % 86400000) + 86400000) % 86400000;
    return PkTime(wrapped);
}

PkTime PkTime::currentTime()
{
    const std::time_t t = std::time(nullptr);
    std::tm tmVal{};
    localtime_r(&t, &tmVal);
    return PkTime(tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);
}

bool PkTime::isValid(int h, int m, int s, int ms)
{
    return h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 59 && ms >= 0 && ms <= 999;
}

// ── PkDateTime 实现（calendar 版，保 epoch API）────────────────────────────

// date+time+本地时区 → TimePoint（照 Qt：QDateTime 按 LocalTime 解析本地墙钟字段，
// 用 mktime 读系统 TZ）。只取整秒部分（m_time.msec() 由 toMSecsSinceEpoch 单独加）。
std::chrono::system_clock::time_point PkDateTime::toTimePoint() const
{
    std::tm tmVal{};
    tmVal.tm_year = m_date.year() - 1900;
    tmVal.tm_mon = m_date.month() - 1;
    tmVal.tm_mday = m_date.day();
    tmVal.tm_hour = m_time.hour();
    tmVal.tm_min = m_time.minute();
    tmVal.tm_sec = m_time.second();
    tmVal.tm_isdst = -1;
    const std::time_t t = mktime(&tmVal);
    return std::chrono::system_clock::from_time_t(t);
}

// TimePoint → date+time（LocalTime，localtime_r 读系统 TZ）
PkDateTime PkDateTime::fromTimePoint(const std::chrono::system_clock::time_point &tp)
{
    const auto msSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    // 拆成 整秒 + 亚秒。负 epoch 的亚秒要落到「整秒的前一秒」，不是转正当天：
    // Qt 模型：fromMSecs(-1) → date 1969-12-31, time 23:59:59.999（msec 在前一天）。
    // 做法：先取整秒（向零截断），亚秒用 floor 语义（对负值向 -inf 借位）。
    std::int64_t wholeSecs = msSinceEpoch / 1000;          // 向零截断整秒
    std::int64_t subMs = msSinceEpoch % 1000;              // -999..999
    if (subMs < 0) {
        // 负亚秒：整秒借 1，亚秒转成前一秒的正 msec（Qt 的 msecs-of-day 落在前一天）
        wholeSecs -= 1;
        subMs += 1000;
    }
    const std::time_t t = static_cast<std::time_t>(wholeSecs);
    std::tm tmVal{};
    localtime_r(&t, &tmVal);
    PkDateTime dt;
    dt.m_date = PkDate(tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday);
    dt.m_time = PkTime(tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec, static_cast<int>(subMs));
    return dt;
}

// 日历字段 → PkDateTime（LocalTime，mktime 读系统 TZ）。带 fieldsInRange /
// yearRepresentable 校验（保 R-16 fromString 系列的拒绝语义），非法返回默认构造。
// 毫秒参数默认 0（本任务 fromString 系列不解析毫秒字段）；非 0 时覆盖拆解回的
// 毫秒分量（保留调用方给的日历字段不变）。
PkDateTime PkDateTime::fromLocalFields(int year, int month, int day, int hour, int minute, int second, int msec)
{
    if (!yearRepresentable(year)) return PkDateTime();
    if (!fieldsInRange(month, day, hour, minute, second)) return PkDateTime();

    std::tm tmVal{};
    tmVal.tm_year = year - 1900;
    tmVal.tm_mon = month - 1;
    tmVal.tm_mday = day;
    tmVal.tm_hour = hour;
    tmVal.tm_min = minute;
    tmVal.tm_sec = second;
    // -1 = 让 mktime 按系统 TZ 自行判定 DST：parse 本地墙钟字段时不知道目标时刻
    // 是否处于夏令时，交给系统决定。这与 Qt 用 tzdb 决定 DST 在正常日期上等价，
    // 在 DST 切换的歧义时刻（春季跳过的那一小时、秋季重复的那一小时）上可能有
    // 细微差异——属已注明的 tzdb 近似，见本文件顶部注释。
    tmVal.tm_isdst = -1;
    const std::time_t t = mktime(&tmVal);

    const auto tp = std::chrono::system_clock::from_time_t(t);
    PkDateTime dt = fromTimePoint(tp);
    if (msec != 0) dt.m_time = PkTime(hour, minute, second, msec);
    return dt;
}

std::int64_t PkDateTime::toMSecsSinceEpoch() const
{
    if (!isValid()) return 0;
    // 整秒部分从 toTimePoint()（mktime 重建的整秒 TimePoint）取，再加 m_time.msec()。
    const auto secPart = std::chrono::duration_cast<std::chrono::milliseconds>(toTimePoint().time_since_epoch()).count();
    return secPart + m_time.msec();
}
std::int64_t PkDateTime::toSecsSinceEpoch() const
{
    if (!isValid()) return 0;
    // Qt 探针：toSecsSinceEpoch == toMSecsSinceEpoch()/1000（C++ 向零截断）。
    //   -1999ms → -1999/1000 = -1；-999ms → -999/1000 = 0；-1ms → -1/1000 = 0。
    // F2 修复后 toMSecsSinceEpoch() 对负亚秒精确往返，除 1000 即得 Qt 语义
    // （旧实现 duration_cast<seconds> 对整秒重建截断，在 floor 借位下 -1999ms
    // 会得到 -2，与探针的 -1 不符）。测试 millisecondsSubSecondTruncatesTowardZero
    // 钉死 -1999 → -1、1999 → 1。
    return toMSecsSinceEpoch() / 1000;
}
PkDateTime PkDateTime::fromMSecsSinceEpoch(std::int64_t msecs)
{
    return fromTimePoint(std::chrono::system_clock::time_point(std::chrono::milliseconds(msecs)));
}
PkDateTime PkDateTime::fromSecsSinceEpoch(std::int64_t secs)
{
    // ⚠ 直接 TimePoint(seconds(secs))，不能 secs*1000 → fromMSecsSinceEpoch：
    // oracle kExtremeTok 会喂 INT64_MAX，secs*1000 是有符号溢出 UB。
    return fromTimePoint(std::chrono::system_clock::time_point(std::chrono::seconds(secs)));
}
PkDateTime PkDateTime::currentDateTime()
{
    return fromTimePoint(std::chrono::system_clock::now());
}
PkDateTime PkDateTime::currentDateTimeUtc()
{
    return fromTimePoint(std::chrono::system_clock::now());
}

bool PkDateTime::isValid() const { return m_date.isValid() && m_time.isValid(); }
bool PkDateTime::isNull() const { return !isValid(); }   // 保 R-16 探针语义
bool PkDateTime::operator==(const PkDateTime &other) const
{
    if (!isValid() || !other.isValid()) return isNull() && other.isNull();
    return m_date == other.m_date && m_time == other.m_time;
}
bool PkDateTime::operator<(const PkDateTime &other) const
{
    if (!isValid() || !other.isValid()) return isNull() && !other.isNull();
    if (m_date != other.m_date) return m_date < other.m_date;
    return m_time < other.m_time;
}

std::int64_t PkDateTime::secsTo(const PkDateTime &other) const
{
    // 无效 guard：任一侧无效返回 0（探针：Qt invalid.secsTo(valid)=0 等，终审 F1）。
    if (!isValid() || !other.isValid()) return 0;
    // diff-based（不是 operand-wise）：(other - this) 的毫秒差除 1000、C++ 向零截断
    // ——探针：a(-999).secsTo(b(1))=1（若按两个 toSecsSinceEpoch 之差算得 0，错）。
    // 测试 secsToSignConvention 钉死 a.secsTo(b)==500（1000→1500）、
    // secsToSubSecondDiffBased 钉死 a(-999).secsTo(b(1))==1。
    return (other.toMSecsSinceEpoch() - toMSecsSinceEpoch()) / 1000;
}
std::int64_t PkDateTime::msecsTo(const PkDateTime &other) const
{
    // 无效 guard：任一侧无效返回 0（探针：Qt invalid.msecsTo(valid)=0 等，终审 F1）。
    if (!isValid() || !other.isValid()) return 0;
    return other.toMSecsSinceEpoch() - toMSecsSinceEpoch();
}
std::int64_t PkDateTime::daysTo(const PkDateTime &other) const
{
    if (!isValid() || !other.isValid()) return 0;
    return m_date.daysTo(other.m_date);
}
PkDateTime PkDateTime::addDays(std::int64_t days) const
{
    if (!isValid()) return PkDateTime();
    PkDateTime r = *this;
    r.m_date = m_date.addDays(days);
    return r;
}
PkDateTime PkDateTime::addMonths(int months) const
{
    if (!isValid()) return PkDateTime();
    PkDateTime r = *this;
    r.m_date = m_date.addMonths(months);
    return r;
}
PkDateTime PkDateTime::addYears(int years) const
{
    if (!isValid()) return PkDateTime();
    PkDateTime r = *this;
    r.m_date = m_date.addYears(years);
    return r;
}
PkDateTime PkDateTime::addSecs(std::int64_t secs) const
{
    // 无效态 guard（评审 F1，探针：Qt invalid.addSecs(10).isValid()==0）。
    // 公式保持 msec 精度不变：照 Qt QDateTime::addSecs = addMSecs(secs*1000)。
    if (!isValid()) return PkDateTime();
    return fromMSecsSinceEpoch(toMSecsSinceEpoch() + secs * 1000);
}
PkDateTime PkDateTime::addMSecs(std::int64_t msecs) const
{
    if (!isValid()) return PkDateTime();
    return fromMSecsSinceEpoch(toMSecsSinceEpoch() + msecs);
}

std::string PkDateTime::toString() const
{
    if (!isValid()) return std::string();
    // 星期名：QDate::dayOfWeek() 1=周一，QDateTime::toString 默认格式
    // "Www Mmm d hh:mm:ss yyyy" 的星期缩写是日曜日起始（Sun..Sat）
    const int wday = m_date.dayOfWeek();   // 1=Mon..7=Sun
    const int dayIdx = (wday % 7);         // 1→1(Mon), 7→0(Sun)
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d:%02d %d",
                  kDayNames[dayIdx], kMonthNames[m_date.month() - 1], m_date.day(),
                  m_time.hour(), m_time.minute(), m_time.second(), m_date.year());
    return std::string(buf);
}

std::string PkDateTime::toString(DateFormat fmt) const
{
    if (!isValid()) return std::string();
    char buf[64];
    switch (fmt) {
    case DateFormat::ISODate:
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                      m_date.year(), m_date.month(), m_date.day(),
                      m_time.hour(), m_time.minute(), m_time.second());
        return std::string(buf);
    case DateFormat::ISODateWithMs:
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                      m_date.year(), m_date.month(), m_date.day(),
                      m_time.hour(), m_time.minute(), m_time.second(), m_time.msec());
        return std::string(buf);
    case DateFormat::RFC2822Date: {
        // 时区尾缀随本地偏移（测试 toStringRfc2822DateReflectsLocalOffset 用
        // localtime_r 独立比对 tm_gmtoff）：toTimePoint() 重建 TimePoint →
        // localtime_r 拆 tm_gmtoff（复用 R-16 的 formatRfc2822Offset）；日期时间
        // 主体直接从 m_date/m_time 字段渲染（"dd Mmm yyyy hh:mm:ss"）。
        std::tm tmVal{};
        const std::time_t t = std::chrono::system_clock::to_time_t(toTimePoint());
        if (localtime_r(&t, &tmVal) == nullptr) return std::string();
        const std::string off = formatRfc2822Offset(tmVal);
        std::snprintf(buf, sizeof(buf), "%02d %s %04d %02d:%02d:%02d %s",
                      m_date.day(), kMonthNames[m_date.month() - 1], m_date.year(),
                      m_time.hour(), m_time.minute(), m_time.second(), off.c_str());
        return std::string(buf);
    }
    } // switch (fmt)
    return std::string();
}

// 变异注入点：token 数量判断（5 个、不多不少）、月份缩写查表失败、
// hh:mm:ss 的冒号位置/数字校验，任一处被削弱都会让纯垃圾串误判为有效——被
// tests/test_date_time.cpp 的 fromStringDefaultRejectsGarbage /
// fromStringDefaultParsesTextDateShape 两个用例（一个覆盖拒绝方向，一个覆盖
// 接受方向）捕获。
PkDateTime PkDateTime::fromString(const std::string &s)
{
    std::istringstream iss(s);
    std::string dayName;
    std::string monthName;
    std::string dayStr;
    std::string timeStr;
    std::string yearStr;
    if (!(iss >> dayName >> monthName >> dayStr >> timeStr >> yearStr)) return PkDateTime();
    std::string extra;
    if (iss >> extra) return PkDateTime(); // 第 6 个 token：形态不对，拒绝

    const int month = monthNumberFromAbbrev(monthName);
    if (month < 0) return PkDateTime();

    if (dayStr.empty() || dayStr.size() > 2 || !isAllDigits(dayStr, 0, dayStr.size())) {
        return PkDateTime();
    }
    if (yearStr.empty() || yearStr.size() > 4 || !isAllDigits(yearStr, 0, yearStr.size())) {
        return PkDateTime();
    }
    if (timeStr.size() != 8 || timeStr[2] != ':' || timeStr[5] != ':' ||
        !isAllDigits(timeStr, 0, 2) || !isAllDigits(timeStr, 3, 2) || !isAllDigits(timeStr, 6, 2)) {
        return PkDateTime();
    }

    const int day = std::stoi(dayStr);
    const int hour = parseIntField(timeStr, 0, 2);
    const int minute = parseIntField(timeStr, 3, 2);
    const int second = parseIntField(timeStr, 6, 2);
    const int year = std::stoi(yearStr);

    return fromLocalFields(year, month, day, hour, minute, second, 0);
}

// 只实现 5 个具体格式串，逐条按固定长度 + 分隔符位置 + 全数字校验——不是通用
// format-token 解析器。变异注入点：任一格式分支的长度阈值/分隔符下标写错，
// 会被 tests/test_date_time.cpp 对应的边界长度用例（用探针钉死的期望输出）
// 捕获；把 fieldsInRange 校验去掉会被非法输入用例捕获。
PkDateTime PkDateTime::fromString(const std::string &s, const std::string &customFormat)
{
    if (customFormat == "yyyy") {
        if (s.size() != 4 || !isAllDigits(s, 0, 4)) return PkDateTime();
        return fromLocalFields(parseIntField(s, 0, 4), 1, 1, 0, 0, 0, 0);
    }
    if (customFormat == "yyyy-MM") {
        if (s.size() != 7 || s[4] != '-' || !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2)) {
            return PkDateTime();
        }
        return fromLocalFields(parseIntField(s, 0, 4), parseIntField(s, 5, 2), 1, 0, 0, 0, 0);
    }
    if (customFormat == "yyyy-MM-dd") {
        if (s.size() != 10 || s[4] != '-' || s[7] != '-' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2)) {
            return PkDateTime();
        }
        return fromLocalFields(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                               parseIntField(s, 8, 2), 0, 0, 0, 0);
    }
    if (customFormat == "yyyy-MM-ddThh:mm") {
        if (s.size() != 16 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2) ||
            !isAllDigits(s, 11, 2) || !isAllDigits(s, 14, 2)) {
            return PkDateTime();
        }
        return fromLocalFields(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                               parseIntField(s, 8, 2), parseIntField(s, 11, 2),
                               parseIntField(s, 14, 2), 0, 0);
    }
    if (customFormat == "yyyy-MM-ddThh:mm:ss") {
        if (s.size() != 19 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
            s[16] != ':' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2) ||
            !isAllDigits(s, 11, 2) || !isAllDigits(s, 14, 2) || !isAllDigits(s, 17, 2)) {
            return PkDateTime();
        }
        return fromLocalFields(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                               parseIntField(s, 8, 2), parseIntField(s, 11, 2),
                               parseIntField(s, 14, 2), parseIntField(s, 17, 2), 0);
    }
    // 不支持的格式串：只实现这 5 个，其余一律当作不匹配处理，返回无效实例。
    return PkDateTime();
}

PkDateTime PkDateTime::fromString(const std::string &s, DateFormat fmt)
{
    if (fmt == DateFormat::ISODate) {
        return fromString(s, std::string("yyyy-MM-ddThh:mm:ss"));
    }
    // RFC2822Date/ISODateWithMs 目前没有真实调用点（真实调用点只有
    // kis_exif_io.cpp / kis_exiv2_common.h，两处都传 Qt::ISODate）——按"不需要
    // 的不做"，先返回无效实例，等真的出现调用点再补。
    return PkDateTime();
}
