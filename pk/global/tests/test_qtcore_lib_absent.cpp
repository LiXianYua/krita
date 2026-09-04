// R-35 回归测试：PkGlobal.h 在「-DQT_CORE_LIB 定义但真 Qt 头不在 TU 里」时的自包含性。
//
// 背景：R-34 把 qAbs/qRound/qMin/qMax/qBound/qIsNull/qFuzzy*/qIsNaN/qInf/qQNaN 与
// namespace Pk 枚举族包进 `#if !defined(QT_CORE_LIB)`，语义是「real Qt 在场就全让位」。
// 但主树编译行**全局**带 -DQT_CORE_LIB（实测：kritasurfacecolormanagementapi 的
// KisColorimetryUtils.cpp 编译命令含 -DQT_CORE_LIB），某 TU 若没 include 真 Qt 头
// （S-04 的 surfaceapi 正是这样经 PkVectorND.h 拉本头），这些符号解析不到——
// pkQtFuzzy* 无条件调 qAbs/qMin、PkPoint.h/PkVectorND.h 内联代码调 qAbs/qRound/
// qIsNull，全部编译失败。R-35 把守卫放宽到
// `!QT_CORE_LIB || !<真 Qt 对应头 include guard>`：真 Qt 头不在场时 pk 提供。
//
// 本 TU 用 COMPILE_DEFINITIONS QT_CORE_LIB 单独编译、不 include 任何真 Qt 头、也不
// include pk/test 的 compat/QtGlobal（那会拉进 pk/test 的 qAbs 定义与 qFuzzy* #define，
// 改变被测变量）。**能编过本身就是断言的一半**（这些符号在 -DQT_CORE_LIB 下必须可用）；
// 取值断言是另一半（放宽后语义与 test_global.cpp 的无 -DQT_CORE_LIB 路径逐字一致）。
#include "../PkGlobal.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>

// 编译期断言：namespace Pk 枚举在 -DQT_CORE_LIB 无真 Qt 下可用（QNAMESPACE_H 守卫放宽）。
static_assert(Pk::IgnoreAspectRatio == 0, "Pk::AspectRatioMode must be available");
static_assert(Pk::WindingFill == 1, "Pk::FillRule must be available");
static_assert(Pk::SmoothTransformation == 1, "Pk::TransformationMode must be available");
static_assert(Pk::black == 2, "Pk::GlobalColor must be available");

// pkQtFuzzy* 在 -DQT_CORE_LIB 下仍 constexpr 可用。
static_assert(pkQtFuzzyCompare(1.0, 1.0), "pkQtFuzzyCompare(1.0, 1.0) must be true");
static_assert(pkQtFuzzyIsNull(0.0), "pkQtFuzzyIsNull(0.0) must be true");

int run_qtcore_lib_absent_tests()
{
    int failures = 0;
    // 标量语义与 test_global.cpp（无 -DQT_CORE_LIB 路径）逐条一致：放宽后同一公式。
    if (pkAbs(-3) != 3) { std::printf("pkAbs(-3) != 3\n"); failures++; }
    if (pkAbs(-3.5) != 3.5) { std::printf("pkAbs(-3.5)\n"); failures++; }
    if (!std::signbit(pkAbs(-0.0))) { std::printf("pkAbs(-0.0) signbit\n"); failures++; }  // -0.0 保留符号位
    if (pkRound(-0.5) != 0) { std::printf("pkRound(-0.5) != 0\n"); failures++; }          // 负半值向 +∞
    if (pkRound(-1.5) != -1) { std::printf("pkRound(-1.5) != -1\n"); failures++; }
    if (pkMin(2, 1) != 1) { std::printf("pkMin(2,1)\n"); failures++; }
    if (pkMax(2, 1) != 2) { std::printf("pkMax(2,1)\n"); failures++; }
    if (pkBound(1, 5, 3) != 3) { std::printf("pkBound(1,5,3)\n"); failures++; }
    if (!pkIsNull(0.0f) || pkIsNull(1e-6f)) { std::printf("qIsNull exact-zero\n"); failures++; }
    if (!pkQtFuzzyCompare(1.0, 1.0)) { std::printf("pkQtFuzzyCompare(1.0,1.0)\n"); failures++; }
    if (!pkQtFuzzyIsNull(0.0)) { std::printf("pkQtFuzzyIsNull(0.0)\n"); failures++; }
    if (!pkQtFuzzyCompare(1.0, 1.0)) { std::printf("pkQtFuzzyCompare\n"); failures++; }
    if (!pkQtFuzzyIsNull(0.0)) { std::printf("pkQtFuzzyIsNull\n"); failures++; }
    if (!pkIsNaN(std::numeric_limits<double>::quiet_NaN())) { std::printf("qIsNaN\n"); failures++; }
    if (pkInf() != std::numeric_limits<double>::infinity()) { std::printf("qInf\n"); failures++; }
    return failures;
}
