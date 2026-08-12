// Task 5：用真实的 kis_debug.h 宏跑一遍，验证三参 Q_LOGGING_CATEGORY 的分类
// 过滤语义。不是单元测试（不接 pk/test harness）——它是"这份垫片能不能扛住
// 真实调用点"的试接证据，跑法见 kis_debug_build.sh。
#include "kis_debug.h"
#include <cstdio>
#include "PkLogSink.h"

static int g_count = 0;
static void capture(PkLogLevel, const PkLogContext &ctx, const char *m, void *)
{ ++g_count; std::printf("[%s] %s\n", ctx.category, m); }

int main()
{
    PkLogAddSink(capture, nullptr);
    warnKrita  << "warn from macro" << 1 << 2;      // krita.general，QtInfoMsg → 应输出
    errFile    << ppVar(g_count);                    // krita.file，QtInfoMsg → 应输出
    dbgFile    << "should be filtered";              // krita.file 的 debug → 不应输出
    dbgTablet  << "tablet debug is on by default";   // krita.tabletlog 是 QtDebugMsg → 应输出
    if (g_count != 3) { std::printf("FAIL: expected 3 emissions, got %d\n", g_count); return 1; }
    std::printf("kis_debug.h 78 宏试编通过，分类过滤符合三参 Q_LOGGING_CATEGORY 语义\n");
    return 0;
}
