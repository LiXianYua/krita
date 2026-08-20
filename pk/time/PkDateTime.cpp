#include "PkDateTime.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <type_traits>

// 哨兵设计（TimePoint::min() 作"无效/未设置"标记）不应该给类型带来额外状态：
// 整个类型必须仍是对单个 TimePoint 的薄包装，没有隐藏的 bool 有效位——与
// PkElapsedTimer.cpp 的同名断言同一个理由。
static_assert(sizeof(PkDateTime) == sizeof(PkDateTime::TimePoint),
              "PkDateTime must stay a thin wrapper over one TimePoint, no extra state");

// PkDateTime 表达墙钟时间（跟随系统时间调整），与 PkElapsedTimer 故意选
// steady_clock（单调、不受系统时间调整影响）分工明确：这里必须是 system_clock，
// 不能反过来。
static_assert(std::is_same<PkDateTime::Clock, std::chrono::system_clock>::value,
              "PkDateTime requires std::chrono::system_clock (wall-clock semantics), not a steady clock");

// ============================================================================
// R-16 Task 3：字符串转换（toString / fromString 系列）
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
// 东正西负），不再是硬编码 "+0000"。
//
// 本文件所有测试用例都是"构造 → 拆解回同一批日历字段"的自洽往返（不依赖与
// `fromMSecsSinceEpoch`/`fromSecsSinceEpoch` 等 epoch 工厂函数比较绝对值），
// 所以 LocalTime 选择不会与 Task 2 已有的 epoch 语义冲突——parse 用 `mktime`
// 把本地墙钟字段换算成绝对 epoch，render 用 `localtime_r` 把绝对 epoch 拆回
// 本地墙钟字段，往返自洽（与时区无关）。
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
// 复现验证过这个边界的来源与数值）。超出这个窗口时，`makeFromLocalFieldsChecked`
// 内部"日历字段 → time_t → TimePoint"这条换算链会发生有符号整数回绕：不是
// "拒绝"，是**悄悄给出一个貌似合法、实际上错得离谱的时刻**——对拍实测例子：
// `fromString("9999-06-15T12:30:45", "yyyy-MM-ddThh:mm:ss")` 在补这条校验
// 之前 isValid()==true，但 toSecsSinceEpoch() 解出来的日期落在 1683 年附近，
// 与输入的 9999 年南辕北辙。真实调用点（EXIF/XMP 元数据日期）不会落在这个
// 窗口之外，但"看起来合法的输入不能悄悄给错答案"这条原则不能因为"用不到"就
// 放弃——用一次朴素的年份区间校验把它变成"明确拒绝"（isValid()==false），
// 而不是放着悄悄腐化数据。两端各留几年安全余量（不精确到 1677/2262 那两个
// 边界年份本身，避免该年份内具体月日进一步逼近真实换算边界）。
bool yearRepresentable(int year)
{
    return year >= 1678 && year <= 2261;
}

// 变异注入点：把 `checked` 判断反过来（fieldsInRange 通过时反而返回哨兵）会
// 让所有 fromString(customFormat) 系列用例连同"非法输入 isValid()==false"
// 一起失败——tests/test_date_time.cpp 的边界长度用例与 illegalInput 系列同时
// 覆盖两个方向。
PkDateTime makeFromLocalFieldsChecked(int year, int month, int day, int hour, int minute, int second)
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
    const auto msecs =
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return PkDateTime::fromMSecsSinceEpoch(static_cast<std::int64_t>(msecs));
}

// 与 makeFromLocalFieldsChecked 对称的反方向：TimePoint → 日历字段（LocalTime）。
// `localtime_r` 对合法 time_t 基本不会失败，这里仍然检查返回值，失败时调用方
// 应当把结果当"无法渲染"处理（返回空串），不静默产出半截数据。
bool localFieldsFromTimePoint(const PkDateTime::TimePoint &tp, std::tm &out)
{
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    return localtime_r(&t, &out) != nullptr;
}

// RFC2822Date 的时区尾缀：从 `localtime_r` 填的 `tm_gmtoff`（秒、东正西负）
// 格式化成 "±hhmm"。`tm_gmtoff` 是 glibc/BSD 扩展（与本文件既有的 GNU 扩展
// 使用同一套 gnu++17 默认可见性），`localtime_r` 会按当前 DST 状态正确设置它。
// Qt 的 RFC2822 尾缀正是这个本地偏移（探针实测：LA → "-0800"、Shanghai →
// "+0800"、Kolkata 半时区 → "+0530"、UTC → "+0000"）。
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

// 变异注入点：把 `% 1000` 换成别的系数，或漏掉负数修正，会被
// tests/test_date_time.cpp 的 isoDateWithMsHasZeroMillisecondsForSecondGranularityInputs
// 用例捕获（该用例断言毫秒字段恒为 "000"，因为本任务的 fromString 系列都不
// 解析毫秒字段，往返出来的毫秒分量必须是 0，不能因为取模方向写反而产出非零
// 或负数字符串）。
int millisecondsPart(const PkDateTime::TimePoint &tp)
{
    using namespace std::chrono;
    long long ms = duration_cast<milliseconds>(tp.time_since_epoch()).count() % 1000;
    if (ms < 0) ms += 1000;
    return static_cast<int>(ms);
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

std::string PkDateTime::toString() const
{
    if (!isValid()) return std::string();
    std::tm tmVal{};
    if (!localFieldsFromTimePoint(m_time, tmVal)) return std::string();
    char buf[64];
    // 变异注入点：把 "%d"（日，不补零）错改成 "%02d" 会在单数日的用例上产出
    // 多余的前导零——本任务的探针钉死用例日期是两位数（15），单独这条不会
    // 捕获该变异，但照抄 Qt 自身的格式规则（QString::number(date.day())
    // 不补零）本身就是正确性依据，写在这里留痕。
    std::snprintf(buf, sizeof(buf), "%s %s %d %02d:%02d:%02d %d",
                  kDayNames[tmVal.tm_wday], kMonthNames[tmVal.tm_mon], tmVal.tm_mday,
                  tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec, tmVal.tm_year + 1900);
    return std::string(buf);
}

std::string PkDateTime::toString(DateFormat fmt) const
{
    if (!isValid()) return std::string();
    std::tm tmVal{};
    if (!localFieldsFromTimePoint(m_time, tmVal)) return std::string();
    char buf[64];
    switch (fmt) {
    case DateFormat::ISODate:
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                      tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday,
                      tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec);
        return std::string(buf);
    case DateFormat::ISODateWithMs: {
        const int ms = millisecondsPart(m_time);
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                      tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday,
                      tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec, ms);
        return std::string(buf);
    }
    case DateFormat::RFC2822Date: {
        // 时区尾缀按本地偏移输出（`localtime_r` 填的 tm_gmtoff），不再硬编码
        // "+0000"——对齐 Qt 按本地墙钟渲染（探针实测 LA → "-0800"、Shanghai →
        // "+0800"）。不要断言探针原始的 "-0800" 字面串，那是探针机器当时的本地
        // 偏移，跨机器会变——断言的是"随本地时区正确变化"。
        const std::string off = formatRfc2822Offset(tmVal);
        std::snprintf(buf, sizeof(buf), "%02d %s %04d %02d:%02d:%02d %s",
                      tmVal.tm_mday, kMonthNames[tmVal.tm_mon], tmVal.tm_year + 1900,
                      tmVal.tm_hour, tmVal.tm_min, tmVal.tm_sec, off.c_str());
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

    return makeFromLocalFieldsChecked(year, month, day, hour, minute, second);
}

// 只实现 5 个具体格式串，逐条按固定长度 + 分隔符位置 + 全数字校验——不是通用
// format-token 解析器。变异注入点：任一格式分支的长度阈值/分隔符下标写错，
// 会被 tests/test_date_time.cpp 对应的边界长度用例（用探针钉死的期望输出）
// 捕获；把 fieldsInRange 校验去掉会被非法输入用例捕获。
PkDateTime PkDateTime::fromString(const std::string &s, const std::string &customFormat)
{
    if (customFormat == "yyyy") {
        if (s.size() != 4 || !isAllDigits(s, 0, 4)) return PkDateTime();
        return makeFromLocalFieldsChecked(parseIntField(s, 0, 4), 1, 1, 0, 0, 0);
    }
    if (customFormat == "yyyy-MM") {
        if (s.size() != 7 || s[4] != '-' || !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2)) {
            return PkDateTime();
        }
        return makeFromLocalFieldsChecked(parseIntField(s, 0, 4), parseIntField(s, 5, 2), 1, 0, 0, 0);
    }
    if (customFormat == "yyyy-MM-dd") {
        if (s.size() != 10 || s[4] != '-' || s[7] != '-' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2)) {
            return PkDateTime();
        }
        return makeFromLocalFieldsChecked(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                                         parseIntField(s, 8, 2), 0, 0, 0);
    }
    if (customFormat == "yyyy-MM-ddThh:mm") {
        if (s.size() != 16 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2) ||
            !isAllDigits(s, 11, 2) || !isAllDigits(s, 14, 2)) {
            return PkDateTime();
        }
        return makeFromLocalFieldsChecked(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                                         parseIntField(s, 8, 2), parseIntField(s, 11, 2),
                                         parseIntField(s, 14, 2), 0);
    }
    if (customFormat == "yyyy-MM-ddThh:mm:ss") {
        if (s.size() != 19 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
            s[16] != ':' ||
            !isAllDigits(s, 0, 4) || !isAllDigits(s, 5, 2) || !isAllDigits(s, 8, 2) ||
            !isAllDigits(s, 11, 2) || !isAllDigits(s, 14, 2) || !isAllDigits(s, 17, 2)) {
            return PkDateTime();
        }
        return makeFromLocalFieldsChecked(parseIntField(s, 0, 4), parseIntField(s, 5, 2),
                                         parseIntField(s, 8, 2), parseIntField(s, 11, 2),
                                         parseIntField(s, 14, 2), parseIntField(s, 17, 2));
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
