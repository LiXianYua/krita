#ifndef PK_TIME_PKDATETIME_H
#define PK_TIME_PKDATETIME_H

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

// ── PkDate —— QDate 的零 Qt 替代（julianDay 模型，照 Qt 5.15 qdatetime.h/cpp 的
// 格里历算法）。默认构造为无效（nullJd 哨兵，isNull()==!isValid()）。R-29 Task 2
// 新增；PkDateTime 仍为 epoch 版（R-29 Task 3 才重写为依赖 PkDate/PkTime）。─────
class PkDate
{
public:
    PkDate() : m_jd(nullJd()) {}
    PkDate(int y, int m, int d);
    explicit PkDate(std::int64_t julianDay) : m_jd(julianDay) {}

    bool isNull() const { return !isValid(); }
    bool isValid() const { return m_jd >= minJd() && m_jd <= maxJd(); }

    int year() const;
    int month() const;
    int day() const;
    int dayOfWeek() const;
    int dayOfYear() const;
    int daysInMonth() const;
    int daysInYear() const;

    bool setDate(int year, int month, int day);
    PkDate addDays(std::int64_t days) const;
    PkDate addMonths(int months) const;
    PkDate addYears(int years) const;
    std::int64_t daysTo(const PkDate &other) const;

    bool operator==(const PkDate &other) const { return m_jd == other.m_jd; }
    bool operator!=(const PkDate &other) const { return m_jd != other.m_jd; }
    bool operator<(const PkDate &other) const { return m_jd < other.m_jd; }
    bool operator<=(const PkDate &other) const { return m_jd <= other.m_jd; }
    bool operator>(const PkDate &other) const { return m_jd > other.m_jd; }
    bool operator>=(const PkDate &other) const { return m_jd >= other.m_jd; }

    static PkDate currentDate();
    static bool isValid(int y, int m, int d);
    static bool isLeapYear(int year);

    static PkDate fromJulianDay(std::int64_t jd) { return (jd >= minJd() && jd <= maxJd()) ? PkDate(jd) : PkDate(); }
    std::int64_t toJulianDay() const { return m_jd; }

private:
    static constexpr std::int64_t nullJd() { return (std::numeric_limits<std::int64_t>::min)(); }
    static constexpr std::int64_t minJd() { return -784350574879LL; }
    static constexpr std::int64_t maxJd() { return 784354017364LL; }
    std::int64_t m_jd;
};

// ── PkTime —— QTime 的零 Qt 替代（msecs-since-midnight 模型，照 Qt 5.15）。
// 默认构造为无效（NullTime 哨兵）。R-29 Task 2 新增。──────────────────────────
class PkTime
{
public:
    PkTime() : m_mds(NullTime) {}
    PkTime(int h, int m, int s = 0, int ms = 0);

    bool isNull() const { return m_mds == NullTime; }
    bool isValid() const;

    int hour() const { if (!isValid()) return -1; return ds() / 3600000; }
    int minute() const { if (!isValid()) return -1; return (ds() / 60000) % 60; }
    int second() const { if (!isValid()) return -1; return (ds() / 1000) % 60; }
    int msec() const { if (!isValid()) return -1; return ds() % 1000; }

    bool setHMS(int h, int m, int s, int ms = 0);
    PkTime addSecs(int secs) const;
    int secsTo(const PkTime &other) const
    { if (!isValid() || !other.isValid()) return 0; return other.m_mds / 1000 - m_mds / 1000; }
    PkTime addMSecs(int ms) const;
    int msecsTo(const PkTime &other) const
    { if (!isValid() || !other.isValid()) return 0; return other.m_mds - m_mds; }

    bool operator==(const PkTime &other) const { return m_mds == other.m_mds; }
    bool operator!=(const PkTime &other) const { return m_mds != other.m_mds; }
    bool operator<(const PkTime &other) const { return m_mds < other.m_mds; }
    bool operator<=(const PkTime &other) const { return m_mds <= other.m_mds; }
    bool operator>(const PkTime &other) const { return m_mds > other.m_mds; }
    bool operator>=(const PkTime &other) const { return m_mds >= other.m_mds; }

    static PkTime fromMSecsSinceStartOfDay(int msecs) { return PkTime(msecs); }
    int msecsSinceStartOfDay() const { return m_mds == NullTime ? 0 : m_mds; }

    static PkTime currentTime();
    static bool isValid(int h, int m, int s, int ms = 0);

private:
    enum { NullTime = -1 };
    // 私有单参构造：msecs-since-midnight 语义（照 Qt 私有 `QTime(int ms)`），供
    // fromMSecsSinceStartOfDay / addSecs / addMSecs 内部把"绕回一天内的毫秒数"
    // 直接构造成 PkTime——public 面仍是 4 参构造，不暴露该入口。
    explicit PkTime(int msecs) : m_mds(msecs) {}
    int ds() const { return m_mds == NullTime ? 0 : m_mds; }
    int m_mds;
};

// QDateTime 的核心值语义对应物。R-16 Task 2（`pk/time`）+ Task 3（字符串转换）。
// R-29 Task 3 把内部表示从「单 TimePoint」重写为「m_date + m_time」calendar 模型
// （对齐 Qt QDateTime），同时**保留全部 epoch API 面**（currentDateTime/
// currentDateTimeUtc/fromMSecsSinceEpoch/fromSecsSinceEpoch/toSecsSinceEpoch/
// secsTo/toString/fromString 系列，语义与探针钉死的一致），并新增 calendar API
// （date()/time()/setDate/setTime/addDays/addMonths/addYears/daysTo/msecsTo/
// operator<）。
//
// 内部表示：m_date（PkDate）+ m_time（PkTime）。**毫秒是 PkTime 的组成部分
// （m_time.msec()），不额外存储独立字段**。epoch 换算走内部 helper：
//   - (date,time) → TimePoint：`toTimePoint()`，用 mktime 读系统 TZ（LocalTime）
//   - TimePoint → (date,time)：`fromTimePoint()`，用 localtime_r 读系统 TZ
// `using Clock/TimePoint` 别名保留（内部 helper 返回/接受 TimePoint）。
//
// 语义按 .superpowers/sdd/R-16/probe-facts.md 实测钉死（探针原始输出见
// docs/superpowers/plans/R-16-probe/probe_time_output.txt）：
//   - 默认构造 `QDateTime()`：`isNull()==true`、`isValid()==false`，
//     `isNull() == !isValid()`（**不是** AND 语义——`isNull := !isValid` 是头文件
//     本身的事实：`qdatetime.h:77-78`），两个默认构造的 `QDateTime()` 互相 `==`
//     为 `true`。calendar 版里默认构造 m_date()/m_time() 都是 invalid →
//     isValid()==false → isNull()==true，天然成立。
//   - `secsTo()` 符号方向：`a.secsTo(b) == b - a`（后减前），返回类型 `qint64`
//   - `fromMSecsSinceEpoch`/`fromSecsSinceEpoch` 单参/三参默认 `timeSpec()` 是
//     `Qt::LocalTime` 不是 UTC
//
// **LocalTime 落地方式（2026-08-18 裁决，见 `R线-spec.md`「PkDateTime 时区」）**：
// `pk/time` 不接 Qt、不接时区数据库、也不对调用方暴露 `timeSpec()` 概念——真实调用点
// 里没有一处读取它。"LocalTime 还是 UTC" 只在把时间点拆解成日历字段时才落地（已按
// 裁决用 C 库 `localtime_r`/`mktime` 落地为 LocalTime，详见 PkDateTime.cpp 顶部
// 注释）。`currentDateTime()` 与 `currentDateTimeUtc()` 在存储层面仍是同一个绝对
// 时刻（`std::chrono::system_clock::now()`）——两者在 API 面上保留区分，但
// `pk/time` 不跟踪 timeSpec，因此 `currentDateTimeUtc()` 实例经 `toString()` 渲染
// 出来的是**本地墙钟**而非 UTC——这是与 Qt 的已知近似，登记在
// pk/time/oracle/R-16.deviation。`fromMSecsSinceEpoch`/`fromSecsSinceEpoch` 同理。
//
// 选用 std::chrono::system_clock 而不是 steady_clock：PkDateTime 表达的是墙钟
// 时间（wall-clock，"现在是几点几分"），这正是 system_clock 的契约——它跟随系统
// 时间调整；steady_clock（PkElapsedTimer 用的）保证的是单调递增的相对计时，两者
// 目的不同，不能互换。
class PkDateTime
{
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    PkDateTime() : m_date(), m_time() {}
    // 照 Qt `QDateTime(const QDate &, const QTime &)`（calendar 版的主要构造）。
    PkDateTime(const PkDate &date, const PkTime &time) : m_date(date), m_time(time) {}

    // ── epoch 工厂（照 Qt：fromMSecsSinceEpoch/fromSecsSinceEpoch 默认 LocalTime）──
    static PkDateTime currentDateTime();
    static PkDateTime currentDateTimeUtc();
    static PkDateTime fromMSecsSinceEpoch(std::int64_t msecs);
    static PkDateTime fromSecsSinceEpoch(std::int64_t secs);

    std::int64_t toMSecsSinceEpoch() const;
    std::int64_t toSecsSinceEpoch() const;

    // ── calendar 访问器 ──
    PkDate date() const { return m_date; }
    PkTime time() const { return m_time; }
    void setDate(const PkDate &date) { m_date = date; }
    void setTime(const PkTime &time) { m_time = time; }
    void setMSecsSinceEpoch(std::int64_t msecs) { *this = fromMSecsSinceEpoch(msecs); }
    void setSecsSinceEpoch(std::int64_t secs) { *this = fromSecsSinceEpoch(secs); }

    bool isNull() const;
    bool isValid() const;
    bool operator==(const PkDateTime &other) const;
    bool operator!=(const PkDateTime &other) const { return !(*this == other); }
    bool operator<(const PkDateTime &other) const;
    bool operator<=(const PkDateTime &other) const { return !(other < *this); }
    bool operator>(const PkDateTime &other) const { return other < *this; }
    bool operator>=(const PkDateTime &other) const { return !(*this < other); }

    std::int64_t secsTo(const PkDateTime &other) const;
    std::int64_t msecsTo(const PkDateTime &other) const;
    std::int64_t daysTo(const PkDateTime &other) const;

    PkDateTime addDays(std::int64_t days) const;
    PkDateTime addMonths(int months) const;
    PkDateTime addYears(int years) const;
    PkDateTime addSecs(std::int64_t secs) const;
    PkDateTime addMSecs(std::int64_t msecs) const;

    // ── 字符串转换（保 R-16 Task 3 API）──
    enum class DateFormat { ISODate, RFC2822Date, ISODateWithMs };
    std::string toString() const;
    std::string toString(DateFormat fmt) const;
    static PkDateTime fromString(const std::string &s);
    static PkDateTime fromString(const std::string &s, const std::string &customFormat);
    static PkDateTime fromString(const std::string &s, DateFormat fmt);

private:
    PkDate m_date;
    PkTime m_time;
    // 毫秒是 PkTime 的组成部分（m_time.msec()），不额外存储独立字段。
    static PkDateTime fromLocalFields(int year, int month, int day, int hour, int minute, int second, int msec);
    static PkDateTime fromTimePoint(const std::chrono::system_clock::time_point &tp);
    std::chrono::system_clock::time_point toTimePoint() const;
};

#endif // PK_TIME_PKDATETIME_H
