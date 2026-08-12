#include "cases/global_case.h"
#include "../PkGlobal.h"
#include "coexist.h"

#include <cmath>
#include <limits>
#include <type_traits>

// PkTestBinder<PkGlobalCase> 由 pk_test_moc.py 生成（CMake 的 pk_test_generate
// 触发构建），像 Qt moc 输出一样直接 #include 进本 TU——显式特化必须在
// qExec<PkGlobalCase> 实例化前对本 TU 可见，编成独立目标文件的话这里只看得到
// 前置声明（不完整类型），编不过。先例：pk/test/tests/selftest_assert.cpp。
#include "pk_binder_global_case.inc"

// ---------------------------------------------------------------------------
// 所有期望值都取自**真 Qt 5.15.7**（/mnt/ssd-disk/liyang/projects/krita-ci-env/
// _install，QT_VERSION_STR "5.15.7"）的实测输出，不是「四舍五入」这类直觉。
// 对齐口径：与 Qt 的任何行为差异默认都是缺陷，所以 Qt 那些看着像 bug 的地方
// 也要一起断言下来，否则以后有人"顺手修正"就没人拦得住。
// ---------------------------------------------------------------------------

void PkGlobalCase::qrealIsDouble()
{
    // qglobal.h:280-283：只有定义了 QT_COORD_TYPE（嵌入式配置）才不是 double。
    // 桌面与 Android 都不定义，所以 qreal 就是 double。
    PK_VERIFY((std::is_same<qreal, double>::value));
    PK_VERIFY(sizeof(qreal) == sizeof(double));
    PK_VERIFY(sizeof(qreal) == 8u);
}

void PkGlobalCase::absMatchesQt()
{
    PK_COMPARE(qAbs(-3), 3);
    PK_COMPARE(qAbs(3), 3);
    PK_COMPARE(qAbs(0), 0);
    PK_COMPARE(qAbs(-3.5), 3.5);
    PK_COMPARE(qAbs(3.5), 3.5);
    PK_COMPARE(qAbs(-2.5f), 2.5f);

    // ⚠ 零号的符号：Qt 的条件是 `t >= 0`，-0.0 >= 0 为真 → qAbs(-0.0) 原样返回
    // -0.0。实测真 Qt 5.15.7：signbit(qAbs(-0.0)) == 1、1.0/qAbs(-0.0) == -inf。
    // 上面的 PK_COMPARE(qAbs(0), 0) 对这条免疫（-0.0 == 0.0 为真），所以必须用
    // signbit 直接查符号位；把条件改成 `t > 0` 这四条里的两条立刻变红。
    PK_VERIFY(std::signbit(qAbs(-0.0)));
    PK_VERIFY(!std::signbit(qAbs(0.0)));
    PK_VERIFY(std::signbit(qAbs(-0.0f)));
    PK_VERIFY(!std::signbit(qAbs(0.0f)));
    // 符号位会经 1/x 扩散成 ∓inf —— 这是"零号符号"在真实调用点上的表现形态。
    PK_VERIFY(1.0 / qAbs(-0.0) == -std::numeric_limits<double>::infinity());
    PK_VERIFY(1.0 / qAbs(0.0) == std::numeric_limits<double>::infinity());
    // qAbs 是模板而不是一组重载：非算术类型只要有 operator>= / 一元 operator-
    // 就能实例化。Qt 的 QPoint 之类不走这里，但 qint64 会。
    PK_COMPARE(qAbs(static_cast<long long>(-5)), static_cast<long long>(5));
}

void PkGlobalCase::minMaxBoundMatchQt()
{
    const int a = 2;
    const int b = 7;
    PK_COMPARE(qMin(a, b), 2);
    PK_COMPARE(qMin(b, a), 2);
    PK_COMPARE(qMax(a, b), 7);
    PK_COMPARE(qMax(b, a), 7);

    const double x = 1.5;
    const double y = -1.5;
    PK_COMPARE(qMin(x, y), -1.5);
    PK_COMPARE(qMax(x, y), 1.5);

    // qBound(min, val, max) —— 参数顺序是 (下界, 值, 上界)，不是 (值, 下界, 上界)。
    // 照抄 Qt 的签名后返回的是 const T&，实参为字面量时那个引用只在本条
    // full-expression 内有效，所以先拷进具名 int 再断言（PK_COMPARE 内部会先
    // const auto & 绑一次，跨语句用就悬垂了）。
    const int boundAbove = qBound(0, 5, 3);
    const int boundBelow = qBound(0, -1, 3);
    const int boundInside = qBound(0, 2, 3);
    PK_COMPARE(boundAbove, 3);
    PK_COMPARE(boundBelow, 0);
    PK_COMPARE(boundInside, 2);

    // qBound 的实现是 qMax(min, qMin(max, val))：min > max 这种反常输入下
    // Qt 返回 min（不是断言失败、不是 max）。照抄公式就得照抄这个结果。
    const int boundInverted = qBound(10, 5, 0);
    PK_COMPARE(boundInverted, 10);
}

void PkGlobalCase::roundMatchesQt()
{
    PK_COMPARE(qRound(0.0), 0);
    PK_COMPARE(qRound(3.0), 3);
    PK_COMPARE(qRound(-3.0), -3);
    PK_COMPARE(qRound(0.5), 1);
    PK_COMPARE(qRound(1.5), 2);
    PK_COMPARE(qRound(2.4999999), 2);
    PK_COMPARE(qRound(-0.4), 0);
    PK_COMPARE(qRound(-0.6), -1);

    // ⚠ Qt5 的 qRound 对**负半值**是向 +∞ 取整，不是「远离零」：
    //     qRound(-0.5) == 0、qRound(-1.5) == -1、qRound(-2.5) == -2
    // 实测真 Qt 5.15.7 确认（公式 int(d - double(int(d-1)) + 0.5) + int(d-1)）。
    // 写成 -1 / -2（std::round 的语义）就是与 Qt 的行为差异。
    PK_COMPARE(qRound(-0.5), 0);
    PK_COMPARE(qRound(-1.5), -1);
    PK_COMPARE(qRound(-2.5), -2);

    // ⚠ int(d + 0.5) 的已知怪癖：0.49999999999999994 + 0.5 在 double 下恰好
    // 等于 1.0（结果落在 1.0 与前一个可表示数的中点上，ties-to-even 取 1.0），
    // 所以 Qt 给 1 而不是 0。照抄公式就必须照抄这个结果。
    PK_COMPARE(qRound(0.49999999999999994), 1);
}

void PkGlobalCase::roundFloatOverloadIsReallyFloat()
{
    PK_COMPARE(qRound(0.5f), 1);
    PK_COMPARE(qRound(-0.5f), 0);
    PK_COMPARE(qRound(-1.5f), -1);

    // 这一条是 float 重载真的存在、且真的按 float 算的判别式：
    // 0.49999997f + 0.5f 在 float 下进位到 1.0f（qRound → 1），
    // 同一个数提升到 double 后 + 0.5 只有 0.99999997（qRound → 0）。
    // 删掉 qRound(float) 重载让实参隐式提升，这一条立刻变红。
    PK_COMPARE(qRound(0.49999997f), 1);
}

void PkGlobalCase::fuzzyCompareMatchesQt()
{
    PK_VERIFY(qFuzzyCompare(1.0, 1.0));
    PK_VERIFY(qFuzzyCompare(0.0, 0.0));
    PK_VERIFY(!qFuzzyCompare(1.0, 1.0000001));
    PK_VERIFY(!qFuzzyCompare(1.0, 1.000001));
    PK_VERIFY(!qFuzzyCompare(-1.0, 1.0));
    PK_VERIFY(qFuzzyCompare(-1.0, -1.0));

    // 相对误差 1e-12：差 1e-13 的两个 1.0 附近的数算相等，差 1e-11 的不算。
    PK_VERIFY(qFuzzyCompare(1.0, 1.0 + 1e-13));
    PK_VERIFY(!qFuzzyCompare(1.0, 1.0 + 1e-11));
    // 把系数**夹到 1e12**：下面这对分别落在 1e12 阈值的两侧（1.0±5e-13 相等、
    // 1.0±5e-12 不相等）。少了它们，系数改成 1e11 或 1e13 都还能全绿——
    // 上面那两条只夹到「1e11..1e13 之间某个值」，不够。实测真 Qt 确认。
    PK_VERIFY(qFuzzyCompare(1.0, 1.0 + 5e-13));
    PK_VERIFY(!qFuzzyCompare(1.0, 1.0 + 5e-12));

    // 浮点相加的经典例子：0.1 + 0.2 != 0.3，但 qFuzzyCompare 认为相等。
    PK_VERIFY(qFuzzyCompare(0.1 + 0.2, 0.3));

    // ⚠ 右端取 qMin(|p1|, |p2|)，所以任何一侧是 0 时永远不成立——两个方向都是
    // false（实测真 Qt 确认）。这条与 pk/test 的 pkFloatingCompare 不同：那边
    // 对 0 走的是 pkFuzzyIsNull 分支。别把两者混为一谈。
    PK_VERIFY(!qFuzzyCompare(0.0, 1e-300));
    PK_VERIFY(!qFuzzyCompare(1e-300, 0.0));

    // ⚠ 右端到底是 qMin 还是 qMax 的**判别输入**。上面全部用例都落在
    // |p1| ≈ |p2| 的区域，那里 qMin ≈ qMax，把 qMin 换成 qMax 一条都不会红。
    // 这一对的两侧差了整整 1.0（|Δ|·1e12 = 1e12）：
    //     qMin = 999999999999.5 < 1e12  → false（真 Qt 5.15.7 实测）
    //     qMax = 1000000000000.5 > 1e12 → true （qMax 变体）
    // 两个方向都取，防止有人只在一侧特判。
    const double fuzzyBig1 = 999999999999.5;
    const double fuzzyBig2 = 1000000000000.5;
    PK_VERIFY(!qFuzzyCompare(fuzzyBig1, fuzzyBig2));
    PK_VERIFY(!qFuzzyCompare(fuzzyBig2, fuzzyBig1));
}

void PkGlobalCase::fuzzyCompareFloatOverloadIsReallyFloat()
{
    // float 的相对误差是 1e-5，比 double 的 1e-12 松 7 个数量级。
    // 同一对数：float 下相等、double 下不相等——float 重载缺失（实参提升到
    // double）时第一条立刻变红。
    PK_VERIFY(qFuzzyCompare(1.0f, 1.000001f));
    PK_VERIFY(!qFuzzyCompare(1.0, 1.000001));
    PK_VERIFY(!qFuzzyCompare(1.0f, 1.0001f));
    PK_VERIFY(qFuzzyCompare(1.0f, 1.0f));
    // 把 float 的系数夹到 1e5（同 double 那边的理由）。实测真 Qt 确认。
    PK_VERIFY(qFuzzyCompare(1.0f, 1.0f + 5e-6f));
    PK_VERIFY(!qFuzzyCompare(1.0f, 1.0f + 5e-5f));

    // float 重载右端 qMin/qMax 的判别输入（同 double 那边的理由，阈值 1e5）：
    //     |Δ|·1e5 = 1e5；qMin = 99999.5f < 1e5 → false（真 Qt 实测）
    //                    qMax = 100000.5f > 1e5 → true（qMax 变体）
    const float fuzzyBig1 = 99999.5f;
    const float fuzzyBig2 = 100000.5f;
    PK_VERIFY(!qFuzzyCompare(fuzzyBig1, fuzzyBig2));
    PK_VERIFY(!qFuzzyCompare(fuzzyBig2, fuzzyBig1));
}

void PkGlobalCase::fuzzyIsNullMatchesQt()
{
    PK_VERIFY(qFuzzyIsNull(0.0));
    PK_VERIFY(qFuzzyIsNull(-0.0));
    PK_VERIFY(qFuzzyIsNull(1e-13));
    PK_VERIFY(qFuzzyIsNull(-1e-13));
    // 阈值是 <=，边界值 1e-12 本身算 null（实测真 Qt 确认）。
    PK_VERIFY(qFuzzyIsNull(1e-12));
    PK_VERIFY(!qFuzzyIsNull(1e-11));
    PK_VERIFY(!qFuzzyIsNull(1.0));
    PK_VERIFY(!qFuzzyIsNull(-1.0));
    // 把阈值夹到 1e-12（落在两侧的一对），阈值上下动一个数量级都会变红。
    PK_VERIFY(qFuzzyIsNull(5e-13));
    PK_VERIFY(!qFuzzyIsNull(5e-12));

    // float 阈值 1e-5：1e-6f 算 null、1e-4f 不算。float 重载缺失时第一条变红
    //（1e-6f 提升到 double 后大于 1e-12，会被判成非 null）。
    PK_VERIFY(qFuzzyIsNull(1e-6f));
    PK_VERIFY(!qFuzzyIsNull(1e-4f));
    PK_VERIFY(qFuzzyIsNull(5e-6f));
    PK_VERIFY(!qFuzzyIsNull(5e-5f));
}

void PkGlobalCase::isNaNMatchesQt()
{
    PK_VERIFY(qIsNaN(std::numeric_limits<double>::quiet_NaN()));
    PK_VERIFY(!qIsNaN(0.0));
    PK_VERIFY(!qIsNaN(1.0));
    PK_VERIFY(!qIsNaN(-1.0));
    PK_VERIFY(!qIsNaN(std::numeric_limits<double>::infinity()));
    PK_VERIFY(!qIsNaN(-std::numeric_limits<double>::infinity()));
    PK_VERIFY(!qIsNaN(std::numeric_limits<double>::denorm_min()));
}

void PkGlobalCase::infMatchesQt()
{
    PK_VERIFY(qInf() == std::numeric_limits<double>::infinity());
    PK_VERIFY(qInf() > 0.0);
    PK_VERIFY(std::isinf(qInf()));
    PK_VERIFY(!qIsNaN(qInf()));
    // -qInf() 是负无穷：调用点（libs/image 一处）就是这么用的。
    PK_VERIFY(std::isinf(-qInf()));
    PK_VERIFY(-qInf() < 0.0);
}

// ---------------------------------------------------------------------------
// 两份 compat/QtGlobal 共存：三种 include 顺序各一个 TU（tests/coexist_*.cpp）。
// 三个 TU 能编过本身就是一半断言；这里核对它们给出的取值一致且与真 Qt 一致。
// 用宏共享检查体，让失败信息的 file:line 落回各自的调用点。
//
// 这两条路径上 qFuzzyCompare / qFuzzyIsNull / qAbs 让位给了 pk/test 的实现
//（PkGlobal.h 机制①），而 pk/test 不在 R-03 的 locks 里。fuzzyZeroA/B 这两条
// 就是「让位是安全的」这个断言的守卫：给 pkFuzzyCompare 注入零侧特判分支
//（一条真实的对 Qt 偏离），只有它们会红。
// ---------------------------------------------------------------------------
#define PK_CHECK_COEXIST_PROBE(probeExpr)                        \
    do {                                                         \
        const PkCoexistProbe pkProbe_ = (probeExpr);             \
        PK_COMPARE(pkProbe_.absNeg, 2.5);                        \
        PK_COMPARE(pkProbe_.roundHalfPos, 1);                    \
        PK_COMPARE(pkProbe_.roundHalfNeg, 0);                    \
        PK_COMPARE(pkProbe_.boundAbove, 3);                      \
        PK_VERIFY(pkProbe_.fuzzyEqual);                          \
        PK_VERIFY(!pkProbe_.fuzzyDiffer);                        \
        PK_VERIFY(!pkProbe_.fuzzyZeroA);                         \
        PK_VERIFY(!pkProbe_.fuzzyZeroB);                         \
        PK_VERIFY(pkProbe_.fuzzyNull);                           \
        PK_VERIFY(!pkProbe_.fuzzyNotNull);                       \
        PK_COMPARE(pkProbe_.qrealSize, sizeof(double));          \
        PK_VERIFY(pkProbe_.qrealIsDouble);                       \
    } while (false)

void PkGlobalCase::coexistWithPkTestShimFirst()
{
    PK_CHECK_COEXIST_PROBE(pkCoexistTestShimFirst());
}

void PkGlobalCase::coexistWithGeometryShimFirst()
{
    PK_CHECK_COEXIST_PROBE(pkCoexistGeometryShimFirst());
}

// 第三条 include 路径（真实调用点的顺序：先 <QRect> 后 <QtGlobal>）。
//
// **真正的断言在编译期** —— tests/coexist_compat_rect_first.cpp 能编过，就说明
// compat/ 的两条传递 include 纪律都还在（详见那个文件顶部）。这里这四个取值
// 只是把那个 TU 钉进可执行文件、并让它必须被一个测试函数调用，否则它可以被
// 悄悄从 CMakeLists 里漏掉而没有任何东西变红。
//
// 取值刻意不碰 qAbs / qFuzzy* / qRound（那个 TU 里这些名字来自 pk/test 那份
// 垫片，odr-use 会造成 ODR 违反，理由见 coexist.h）。
void PkGlobalCase::coexistWithCompatRectFirst()
{
    const PkCompatIncludeProbe p = pkCompatRectFirstProbe();
    PK_COMPARE(p.rectRight, 3);
    PK_COMPARE(p.rectBottom, 5);
    PK_COMPARE(p.rectFRight, 5.0);
    PK_COMPARE(p.rectFBottom, 7.0);
}

int run_global_tests()
{
    PkGlobalCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
