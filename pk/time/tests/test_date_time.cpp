#include "test_date_time.h"
#include "../PkDateTime.h"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>

void TestDateTime::defaultConstructedIsInvalidAndNull()
{
    // 探针：`默认构造 QDateTime() | isNull()==true、isValid()==false，与
    // !isValid() 等价（isNull := !isValid 成立）`——严格互补，不是 AND 语义。
    PkDateTime dt;
    PK_VERIFY(!dt.isValid());
    PK_VERIFY(dt.isNull());
    PK_VERIFY(dt.isNull() == !dt.isValid());
}

void TestDateTime::fromFactoryProducesValidNonNull()
{
    // 覆盖哨兵判定的另一半：工厂构造出来的实例必须落在"有效"区间，不能被误判成
    // 哨兵值——对调 isValid() 里的 != 会让这个用例连同上一个一起失败。
    const PkDateTime fromMs = PkDateTime::fromMSecsSinceEpoch(1500000);
    const PkDateTime fromSecs = PkDateTime::fromSecsSinceEpoch(1500);
    PK_VERIFY(fromMs.isValid());
    PK_VERIFY(!fromMs.isNull());
    PK_VERIFY(fromSecs.isValid());
    PK_VERIFY(!fromSecs.isNull());
}

void TestDateTime::defaultConstructedInstancesAreEqual()
{
    // 探针：`QDateTime()==QDateTime() → true`——两个默认构造实例互相 == 为 true，
    // 不会因为"无效"就在比较上出问题。
    PK_VERIFY(PkDateTime() == PkDateTime());
}

void TestDateTime::equalityForSameAndDifferentEpoch()
{
    const PkDateTime a = PkDateTime::fromSecsSinceEpoch(1000);
    const PkDateTime aAgain = PkDateTime::fromSecsSinceEpoch(1000);
    const PkDateTime b = PkDateTime::fromSecsSinceEpoch(1500);
    PK_VERIFY(a == aAgain);
    PK_VERIFY(!(a == b));
    PK_VERIFY(a != b);
}

void TestDateTime::epochSecondsRoundTrip()
{
    // fromSecsSinceEpoch(x).toSecsSinceEpoch() 互逆，秒精度下没有截断问题。
    const std::int64_t secs = 1500;
    PK_VERIFY(PkDateTime::fromSecsSinceEpoch(secs).toSecsSinceEpoch() == secs);

    // 覆盖负数 epoch（1970 年之前），同样应严格互逆。
    const std::int64_t negativeSecs = -12345;
    PK_VERIFY(PkDateTime::fromSecsSinceEpoch(negativeSecs).toSecsSinceEpoch() == negativeSecs);
}

void TestDateTime::epochMillisecondsRoundTripAtSecondBoundary()
{
    // fromMSecsSinceEpoch 没有对应的 toMSecsSinceEpoch()（Task 2 API 面没有这一
    // 项），所以毫秒往返只能借 toSecsSinceEpoch() 验证：选一个恰好落在秒边界的
    // 毫秒值（1000 的整数倍），换算不应该有偏差——这条显式覆盖"毫秒/秒精度差异"，
    // 不是靠巧合蒙对。
    const std::int64_t msAtBoundary = 1500000; // == 1500 * 1000
    PK_VERIFY(PkDateTime::fromMSecsSinceEpoch(msAtBoundary).toSecsSinceEpoch() == 1500);

    // 与 fromSecsSinceEpoch 的同一时刻应该相等（同一个内部时间点）。
    PK_VERIFY(PkDateTime::fromMSecsSinceEpoch(msAtBoundary) == PkDateTime::fromSecsSinceEpoch(1500));
}

void TestDateTime::millisecondsSubSecondTruncatesTowardZero()
{
    // 显式处理"毫秒/秒精度差异"，不要静默截断产生假绿：非整秒的毫秒值转换到
    // toSecsSinceEpoch() 时按 duration_cast<seconds> 的截断语义（向零取整）。
    // 1999ms → 1s（不是 2s，不做四舍五入）。
    PK_VERIFY(PkDateTime::fromMSecsSinceEpoch(1999).toSecsSinceEpoch() == 1);
    // 负数同理：-1999ms → -1s（向零截断，不是向下取整成 -2s）。
    PK_VERIFY(PkDateTime::fromMSecsSinceEpoch(-1999).toSecsSinceEpoch() == -1);
}

void TestDateTime::secsToSignConvention()
{
    // 探针：`secsTo() 符号方向 | a.secsTo(b) == b - a（后减前，a=1000s, b=1500s
    // → a.secsTo(b)=500, b.secsTo(a)=-500）`——逐值钉死，不是弱断言"结果非负"。
    const PkDateTime a = PkDateTime::fromSecsSinceEpoch(1000);
    const PkDateTime b = PkDateTime::fromSecsSinceEpoch(1500);
    PK_VERIFY(a.secsTo(b) == 500);
    PK_VERIFY(b.secsTo(a) == -500);
    // 自身到自身必须是 0。
    PK_VERIFY(a.secsTo(a) == 0);
}

void TestDateTime::currentDateTimeAndUtcAreValid()
{
    // 覆盖两个当前时刻工厂函数确实产出有效实例（不是哨兵值）——不断言具体值
    // （那是墙钟当前时刻，不可控），只断言 isValid()/isNull() 与
    // toSecsSinceEpoch() 落在合理范围（远大于 0，2020-01-01 之后的任意时刻）。
    const PkDateTime now = PkDateTime::currentDateTime();
    const PkDateTime nowUtc = PkDateTime::currentDateTimeUtc();
    PK_VERIFY(now.isValid());
    PK_VERIFY(!now.isNull());
    PK_VERIFY(nowUtc.isValid());
    PK_VERIFY(!nowUtc.isNull());
    PK_VERIFY(now.toSecsSinceEpoch() > 1577836800); // 2020-01-01T00:00:00Z 之后
    PK_VERIFY(nowUtc.toSecsSinceEpoch() > 1577836800);
}

// ============================================================================
// R-16 Task 3：字符串转换（toString / fromString 系列）
//
// 期望值全部来自探针原始输出（docs/superpowers/plans/R-16-probe/
// probe_time_output.txt，逐字摘录进 .superpowers/sdd/R-16/task-3-context.md），
// 不是弱断言——每条都逐值核对，不是只判断 isValid()。
// ============================================================================

void TestDateTime::fromStringYyyyBoundary()
{
    // 探针：`fromString("2024","yyyy") isValid => [true]`；
    // `fromString("2024","yyyy") toString(ISODate) => [2024-01-01T00:00:00]`。
    const PkDateTime dt = PkDateTime::fromString("2024", "yyyy");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-01-01T00:00:00");
}

void TestDateTime::fromStringYyyyMMBoundary()
{
    // 探针：`fromString("2024-03","yyyy-MM") toString(ISODate) =>
    // [2024-03-01T00:00:00]`。
    const PkDateTime dt = PkDateTime::fromString("2024-03", "yyyy-MM");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-01T00:00:00");
}

void TestDateTime::fromStringYyyyMMddBoundary()
{
    // 探针：`fromString("2024-03-15","yyyy-MM-dd") toString(ISODate) =>
    // [2024-03-15T00:00:00]`。
    const PkDateTime dt = PkDateTime::fromString("2024-03-15", "yyyy-MM-dd");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T00:00:00");
}

void TestDateTime::fromStringYyyyMMddThhMmBoundary()
{
    // 探针：`fromString(...THh:mm) toString(ISODate) => [2024-03-15T08:30:00]`
    // ——缺失的秒字段补 0。
    const PkDateTime dt = PkDateTime::fromString("2024-03-15T08:30", "yyyy-MM-ddThh:mm");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T08:30:00");
}

void TestDateTime::fromStringYyyyMMddThhMmSsBoundary()
{
    // 探针：`fromString(...THh:mm:ss) toString(ISODate) =>
    // [2024-03-15T08:30:45]`。
    const PkDateTime dt = PkDateTime::fromString("2024-03-15T08:30:45", "yyyy-MM-ddThh:mm:ss");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T08:30:45");
}

void TestDateTime::fromStringCustomFormatRejectsIllegalInput()
{
    // 探针：`fromString("abc","yyyy") isValid => [false]`。
    PK_VERIFY(!PkDateTime::fromString("abc", "yyyy").isValid());
    // 长度对但分隔符不对/非数字，同样必须拒绝（不是只测探针那一条）。
    PK_VERIFY(!PkDateTime::fromString("2024.03", "yyyy-MM").isValid());
    PK_VERIFY(!PkDateTime::fromString("abcd-03-15", "yyyy-MM-dd").isValid());
    // 不支持的格式串：直接返回无效实例，不崩溃、不误判。
    PK_VERIFY(!PkDateTime::fromString("2024-03-15", "yyyy/MM/dd").isValid());
}

void TestDateTime::fromStringCustomFormatEmptyStringIsNull()
{
    // 探针：`fromString("","yyyy") isValid => [false]`、
    // `fromString("","yyyy") isNull => [true]`。
    const PkDateTime dt = PkDateTime::fromString("", "yyyy");
    PK_VERIFY(!dt.isValid());
    PK_VERIFY(dt.isNull());
}

void TestDateTime::fromStringDefaultParsesTextDateShape()
{
    // 探针：`fromString(TextDate default) isValid => [true]`；
    // `fromString(TextDate default) toString(ISODate) =>
    // [2015-05-20T03:40:13]`。
    const PkDateTime dt = PkDateTime::fromString("Wed May 20 03:40:13 2015");
    PK_VERIFY(dt.isValid());
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2015-05-20T03:40:13");
}

void TestDateTime::fromStringDefaultRejectsGarbage()
{
    // 探针：`fromString(garbage) isValid => [false]`。
    PK_VERIFY(!PkDateTime::fromString("not-a-date-at-all").isValid());
    // token 数量对但字段不合法（月份缩写查不到）同样要拒绝。
    PK_VERIFY(!PkDateTime::fromString("Wed Xyz 20 03:40:13 2015").isValid());
    // token 数量多一个（第 6 个），形态不对，拒绝。
    PK_VERIFY(!PkDateTime::fromString("Wed May 20 03:40:13 2015 extra").isValid());
    PK_VERIFY(PkDateTime::fromString("").isNull());
}

void TestDateTime::fromStringIsoDateMarkerMatchesCustomFormat()
{
    // fromString(s, DateFormat::ISODate) 等价于
    // fromString(s, "yyyy-MM-ddThh:mm:ss")——真实调用点
    // kis_exif_io.cpp/kis_exiv2_common.h 用的是 Qt::ISODate。
    const PkDateTime viaMarker =
        PkDateTime::fromString("2024-01-15T12:30:45", PkDateTime::DateFormat::ISODate);
    const PkDateTime viaCustomFormat =
        PkDateTime::fromString("2024-01-15T12:30:45", "yyyy-MM-ddThh:mm:ss");
    PK_VERIFY(viaMarker.isValid());
    PK_VERIFY(viaMarker == viaCustomFormat);
}

void TestDateTime::toStringDefaultMatchesTextDateShape()
{
    // 探针：`toString() default => [Mon Jan 15 12:30:45 2024]`——2024-01-15
    // 经 UTC 日历字段计算确实是星期一（2024-01-01 是周一，15 号仍是周一）。
    const PkDateTime dt =
        PkDateTime::fromString("2024-01-15T12:30:45", PkDateTime::DateFormat::ISODate);
    PK_VERIFY(dt.toString() == "Mon Jan 15 12:30:45 2024");
}

void TestDateTime::toStringIsoDate()
{
    // 探针：`toString(Qt::ISODate) => [2024-01-15T12:30:45]`。
    const PkDateTime dt =
        PkDateTime::fromString("2024-01-15T12:30:45", PkDateTime::DateFormat::ISODate);
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-01-15T12:30:45");
}

void TestDateTime::toStringIsoDateWithMs()
{
    // 探针：`toString(Qt::ISODateWithMs) => [2024-01-15T12:30:45.000]`——本任务
    // 的 fromString 系列都不解析毫秒字段，毫秒分量恒为 000。
    const PkDateTime dt =
        PkDateTime::fromString("2024-01-15T12:30:45", PkDateTime::DateFormat::ISODate);
    PK_VERIFY(dt.toString(PkDateTime::DateFormat::ISODateWithMs) == "2024-01-15T12:30:45.000");
}

void TestDateTime::toStringRfc2822DateReflectsLocalOffset()
{
    // RFC2822Date 的时区尾缀必须随本地时区输出（不是固定 "+0000"）——2026-08-18
    // 裁决把日历字段渲染/解析从固定 UTC 改成 LocalTime 后，这个断言从"固定 +0000"
    // 改成"与系统 C 库报告的本地偏移一致"。日期时间主体是本地墙钟字段的往返
    // （parse 本地 → render 本地），与时区无关，恒为 "15 Jan 2024 12:30:45"。
    const PkDateTime dt =
        PkDateTime::fromString("2024-01-15T12:30:45", PkDateTime::DateFormat::ISODate);
    const std::string s = dt.toString(PkDateTime::DateFormat::RFC2822Date);

    // 形状："15 Jan 2024 12:30:45 ±hhmm"，主体 20 字符 + 空格 + 5 字符尾缀。
    PK_VERIFY(s.size() == 26);
    PK_VERIFY(s.substr(0, 20) == "15 Jan 2024 12:30:45");
    PK_VERIFY(s[20] == ' ');
    PK_VERIFY(s[21] == '+' || s[21] == '-');
    for (int i = 22; i < 26; ++i) {
        PK_VERIFY(std::isdigit(static_cast<unsigned char>(s[i])) != 0);
    }

    // 尾缀数值必须等于系统 C 库报告的本地偏移（秒、东正西负）。这是独立于被测
    // 实现的真值来源——localtime_r/tm_gmtoff 直接读系统 TZ，不是 PkDateTime 自己
    // 的实现；拿它比对能抓住"实现偷偷硬编码 +0000"这类回归（非 UTC 机器上，
    // "+0000" 与真实偏移数值必然不等）。
    const std::time_t t = static_cast<std::time_t>(dt.toSecsSinceEpoch());
    std::tm tmVal{};
    localtime_r(&t, &tmVal);
    const long expectedSecs = tmVal.tm_gmtoff;

    const int offHh = std::stoi(s.substr(22, 2));
    const int offMm = std::stoi(s.substr(24, 2));
    long parsedSecs = static_cast<long>(offHh) * 3600 + static_cast<long>(offMm) * 60;
    if (s[21] == '-') parsedSecs = -parsedSecs;
    PK_VERIFY(parsedSecs == expectedSecs);
}

void TestDateTime::toStringOnInvalidReturnsEmpty()
{
    // 无效实例的 toString() 系列返回空串，不是给一段垃圾字符——真实调用点
    // 拿到无效 PkDateTime 时不应该看到半截格式化结果。
    const PkDateTime invalid;
    PK_VERIFY(invalid.toString().empty());
    PK_VERIFY(invalid.toString(PkDateTime::DateFormat::ISODate).empty());
    PK_VERIFY(invalid.toString(PkDateTime::DateFormat::RFC2822Date).empty());
    PK_VERIFY(invalid.toString(PkDateTime::DateFormat::ISODateWithMs).empty());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_date_time.inc"

int run_date_time_tests(int argc, char **argv)
{
    TestDateTime tc;
    return PkTest::qExec(&tc, argc, argv);
}
