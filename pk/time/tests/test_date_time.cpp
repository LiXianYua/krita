#include "test_date_time.h"
#include "../PkDateTime.h"

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

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_date_time.inc"

int run_date_time_tests(int argc, char **argv)
{
    TestDateTime tc;
    return PkTest::qExec(&tc, argc, argv);
}
