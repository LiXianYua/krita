#ifndef PK_TIME_PKDATETIME_H
#define PK_TIME_PKDATETIME_H

#include <chrono>
#include <cstdint>
#include <string>

// QDateTime 的核心值语义对应物。R-16 Task 2（`pk/time`）+ Task 3（字符串转换）。
//
// Task 2 的 API 面严格按 .superpowers/sdd/R-16/task-2-brief.md——保留范围内只有
// 下面这些成员（真实调用点分析出来的，不多不少）：
//   currentDateTime() / currentDateTimeUtc() / fromMSecsSinceEpoch() /
//   fromSecsSinceEpoch() / toSecsSinceEpoch() / isValid() / isNull() /
//   operator== / secsTo() / 默认构造
// Task 3（本次新增）按 .superpowers/sdd/R-16/task-3-brief.md 加了
// toString()/toString(DateFormat)/fromString() 系列，见下方对应方法注释。
//
// 语义按 .superpowers/sdd/R-16/probe-facts.md 实测钉死（探针原始输出见
// docs/superpowers/plans/R-16-probe/probe_time_output.txt）：
//   - 默认构造 `QDateTime()`：`isNull()==true`、`isValid()==false`，
//     `isNull() == !isValid()`（**不是** AND 语义——`isNull := !isValid` 是头文件
//     本身的事实：`qdatetime.h:77-78`，`isNull() = !isValid()`），两个默认构造的
//     `QDateTime()` 互相 `==` 为 `true`
//     （探针：`默认构造 QDateTime()` 一行）
//   - `secsTo()` 符号方向：`a.secsTo(b) == b - a`（后减前），
//     返回类型 `qint64`（`sizeof==8`）
//     （探针：`secsTo() 符号方向 | a.secsTo(b) == b - a（后减前，a=1000s, b=1500s
//       → a.secsTo(b)=500, b.secsTo(a)=-500）`）
//   - `fromMSecsSinceEpoch`/`fromSecsSinceEpoch` 单参/三参默认 `timeSpec()` 是
//     `Qt::LocalTime` 不是 UTC（`qdatetime.h:402` 三参默认值 + 探针对单参版的
//     确认：`fromMSecsSinceEpoch(0)（单参重载）默认 timeSpec() | Qt::LocalTime`）
//
// **LocalTime/UTC 落地方式（本任务自行判断的实现细节，已在报告里写明依据）**：
// `pk/time` 不接 Qt、不支持时区、也不对调用方暴露 `timeSpec()` 概念——真实调用点
// 里没有一处读取它。`std::chrono::system_clock::time_point` 本身就是一个不带
// 时区标记的绝对 epoch 时间点，"LocalTime 还是 UTC" 这个区别只在**把时间点拆解成
// 年/月/日/时/分/秒的日历字段**时才有意义（那是 Task 3 `toString`/日历访问器的
// 事）。本类型（Task 2）完全不做日历拆解，`currentDateTime()` 与
// `currentDateTimeUtc()` 在存储层面因此是同一件事：都是
// `std::chrono::system_clock::now()` 的这一个绝对时刻。两个工厂函数在 API 面上
// 保留区分（调用点语义上一个说"系统本地当前时刻"一个说"UTC 当前时刻"），但在
// `pk/time` 这一层，两者产出的内部时间点相同——差异会在 Task 3
// 需要按日历字段渲染时才需要落地为真正的时区转换。`fromMSecsSinceEpoch`/
// `fromSecsSinceEpoch` 同理：它们只是"给一个 epoch 数字，存成内部时间点"，不模拟
// "本地时区"这个概念。
//
// 哨兵设计：与 PkElapsedTimer 同一种思路——不额外放一个 bool 有效位，直接借用
// std::chrono 自己的 TimePoint::min() 作"无效/未设置"的哨兵值，`isNull()` 直接
// 等价于 `!isValid()`（严格按上面「编译期已确认的事实」实现，不是 AND 语义）。
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

    constexpr PkDateTime() noexcept : m_time(TimePoint::min()) {}

    // 探针：`默认构造 QDateTime()`；currentDateTime()/currentDateTimeUtc() 在
    // pk/time 这一层存储相同的绝对时刻（见上方类注释的 LocalTime/UTC 落地说明）。
    static PkDateTime currentDateTime() noexcept
    {
        PkDateTime dt;
        dt.m_time = Clock::now();
        return dt;
    }

    static PkDateTime currentDateTimeUtc() noexcept
    {
        PkDateTime dt;
        dt.m_time = Clock::now();
        return dt;
    }

    // 探针：`fromMSecsSinceEpoch(0)（单参重载）默认 timeSpec() | Qt::LocalTime`——
    // pk/time 不暴露 timeSpec()，这里只是把 epoch 毫秒数存成内部时间点（见上方类
    // 注释）。std::chrono::milliseconds → system_clock 的原生 duration（通常是
    // 纳秒）是加宽转换，不丢精度。
    static PkDateTime fromMSecsSinceEpoch(std::int64_t msecs) noexcept
    {
        PkDateTime dt;
        dt.m_time = TimePoint(std::chrono::milliseconds(msecs));
        return dt;
    }

    static PkDateTime fromSecsSinceEpoch(std::int64_t secs) noexcept
    {
        PkDateTime dt;
        dt.m_time = TimePoint(std::chrono::seconds(secs));
        return dt;
    }

    // 头文件事实：`QDateTime::secsTo`/`msecsTo` 返回 `qint64`（`qdatetime.h:365-
    // 366`），不是 `int`——这里用 std::int64_t（与 PkElapsedTimer 的既有做法一致，
    // 逐位等价 qint64）。截断到更窄类型是调用点的事，不是本能力的事。
    //
    // 变异注入点：把除数从 1000（duration_cast<seconds> 的语义等价物）换成毫秒
    // 精度会被 tests/test_date_time.cpp 的 roundTripSecondsToMsBoundary 一类用例
    // 捕获——这里没有手写系数，直接借助 duration_cast<seconds>，把"取整方式"这个
    // 决策交给标准库、避免自己手写一个可能出错的换算常数。
    std::int64_t toSecsSinceEpoch() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::seconds>(m_time.time_since_epoch()).count();
    }

    // 变异注入点：哨兵判定 `!=` 改成 `==`（或反过来）会让默认构造/工厂构造的
    // isValid() 结果对调，被 tests/test_date_time.cpp 的
    // defaultConstructedIsInvalidAndNull / fromFactoryProducesValidNonNull
    // 两个用例捕获。
    bool isValid() const noexcept { return m_time != TimePoint::min(); }

    // 严格 `isNull() == !isValid()`——不是"日期 isNull 且时间 isNull"的 AND 语义
    // （probe-facts.md 已用探针坐实，PkAuxTypes.cpp 那份 AND 语义是不遵循的占位
    // 先例，见类注释）。变异注入点：把这里改回手写的 AND 组合逻辑会被
    // defaultConstructedIsInvalidAndNull 用例捕获（该用例同时断言两者互补）。
    bool isNull() const noexcept { return !isValid(); }

    // 变异注入点：比较错字段/漏比较会被 equalityForSameAndDifferentEpoch 与
    // defaultConstructedInstancesAreEqual 两个用例捕获（后者专门覆盖两个默认
    // 构造实例互相 == 为 true 这条探针实测行为）。
    bool operator==(const PkDateTime &other) const noexcept { return m_time == other.m_time; }
    bool operator!=(const PkDateTime &other) const noexcept { return !(*this == other); }

    // 探针：`secsTo() 符号方向 | a.secsTo(b) == b - a（后减前，a=1000s, b=1500s
    // → a.secsTo(b)=500, b.secsTo(a)=-500）`——变异注入点：把 `other.m_time -
    // m_time` 写反成 `m_time - other.m_time` 会让符号整体翻转，被
    // tests/test_date_time.cpp 的 secsToSignConvention 用例捕获（该用例同时断言
    // 正负两个方向，翻转符号也过不了）。
    std::int64_t secsTo(const PkDateTime &other) const noexcept
    {
        return std::chrono::duration_cast<std::chrono::seconds>(other.m_time - m_time).count();
    }

    // ---- R-16 Task 3：字符串转换 ----
    //
    // 日历字段（年/月/日/时/分/秒）的拆解与合成统一按 UTC 处理，不读取/依赖
    // 机器本地时区：`pk/time` 没有时区数据库，`std::chrono`（C++17）本身也拿
    // 不到系统时区偏移这类信息。这个决策同时解释了 RFC2822Date 固定输出
    // "+0000" 时区尾缀的原因（详见 PkDateTime.cpp 顶部注释与
    // .superpowers/sdd/R-16/task-3-report.md）。

    // 仿 Qt `Qt::TextDate`（`toString()` 默认格式）里日期时间部分能表达的三种
    // 常用定制格式，够真实调用点用——不做 Qt 全部 `Qt::DateFormat` 枚举。
    enum class DateFormat { ISODate, RFC2822Date, ISODateWithMs };

    // 仿 Qt::TextDate 默认格式："Www Mmm d hh:mm:ss yyyy"（星期缩写 月缩写 日
    // 时:分:秒 年；日不补零，时分秒补零——照抄 Qt 自己的格式规则）。探针：
    // `toString() default` → `Mon Jan 15 12:30:45 2024`。无效实例返回空串。
    std::string toString() const;

    // ISODate → "yyyy-MM-ddThh:mm:ss"（探针：`2024-01-15T12:30:45`）；
    // RFC2822Date → "dd Mmm yyyy hh:mm:ss +0000"（时区尾缀固定 +0000，见上方
    // 类注释；探针给的 "-0800" 是探针机器当时的本地偏移，不是恒定值，不能照抄
    // 断言）；ISODateWithMs → "yyyy-MM-ddThh:mm:ss.zzz"（探针：
    // `2024-01-15T12:30:45.000`）。无效实例返回空串。
    std::string toString(DateFormat fmt) const;

    // 无格式兜底，仿 Qt::TextDate 默认解析：只认 "Www Mmm d hh:mm:ss yyyy" 这
    // 一种固定形态（5 个空白分隔 token：星期缩写/月缩写/日/hh:mm:ss/年），不
    // 匹配（含空串、纯垃圾串）返回无效实例（isValid()==false）。真实调用点
    // （`kis_meta_data_parser.cc` 的 `DateParser::parse` 兜底分支）输入永远是
    // 不匹配前 5 种定制格式的字符串，不需要通用自然语言日期解析器——探针：
    // `"Wed May 20 03:40:13 2015"` 能解析、纯垃圾串 `isValid()==false`。
    static PkDateTime fromString(const std::string &s);

    // 只实现这 5 个具体格式串："yyyy"/"yyyy-MM"/"yyyy-MM-dd"/
    // "yyyy-MM-ddThh:mm"/"yyyy-MM-ddThh:mm:ss"（真实调用点
    // `DateParser::parse` 六分支里前 5 支实际用到的全部格式），不做通用
    // format-token 解析器——理由：真实调用点只有这 5 种，一般化解析器是
    // "10 倍工作量买 0 收益"。缺失字段补默认值（月/日补 1，时分秒补 0）。
    // `customFormat` 不是这 5 个之一、或 `s` 不匹配该格式（长度不对/非数字/
    // 分隔符不对），返回无效实例；`s` 为空串额外 `isNull()==true`（沿用哨兵
    // 语义，自动成立，不需要特判）。
    static PkDateTime fromString(const std::string &s, const std::string &customFormat);

    // 等价于 `fromString(s, "yyyy-MM-ddThh:mm:ss")`——真实调用点
    // （`kis_exif_io.cpp`、`kis_exiv2_common.h`）传的都是 `Qt::ISODate`。只支持
    // `DateFormat::ISODate`；其余枚举值当前没有真实调用点，直接返回无效实例
    // （等真的出现调用点再扩展，不预先做通用化——同一条 YAGNI 理由）。
    static PkDateTime fromString(const std::string &s, DateFormat fmt);

private:
    TimePoint m_time;
};

#endif // PK_TIME_PKDATETIME_H
