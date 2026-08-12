// 唯一 #include <spdlog/spdlog.h> 的文件——pklog 的使用者对 spdlog 一无所知
// （CMakeLists.txt 里 target_link_libraries(pklog PRIVATE spdlog::spdlog)）。
// 换后端只用改这一个翻译单元。
#include "PkLogBackend.h"

#include <cstdio>
#include <cstdlib>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "PkLogSink.h"

namespace {

// PkLogLevel 的取值刻意与 spdlog::level::level_enum 无关（见 PkLogLevel.h），
// 这里做一次性映射。5 档对 5 档，单调递增，PkLogFatal 落 critical——
// abort 由调用方 PkLogEmit 在 log() 之后另行触发。
spdlog::level::level_enum mapLevel(PkLogLevel level)
{
    switch (level) {
    case PkLogDebug:
        return spdlog::level::debug;
    case PkLogInfo:
        return spdlog::level::info;
    case PkLogWarning:
        return spdlog::level::warn;
    case PkLogCritical:
        return spdlog::level::err;
    case PkLogFatal:
        return spdlog::level::critical;
    }
    // 兜底：level 是普通 enum，理论上能被越界的 int 强转进来。
    return spdlog::level::info;
}

} // namespace

void PkLogEnsureLogger(const char *categoryName, PkLogLevel minLevel)
{
    auto logger = spdlog::get(categoryName);
    if (!logger) {
        logger = spdlog::stderr_color_mt(categoryName);
    }
    logger->set_level(mapLevel(minLevel));
}

void PkLogEmit(PkLogLevel level, const PkLogContext &ctx, const std::string &message)
{
    // 先 sink 后 spdlog：Flutter 侧订阅不受 spdlog 的 minLevel 过滤影响，
    // 语义由 tests/test_sink.cpp 钉住。
    PkLogDispatchToSinks(level, ctx, message.c_str());

    auto logger = spdlog::get(ctx.category);
    if (!logger) {
        // 评审 Important 项：不再静默兜底创建 logger。旧行为是
        // spdlog::stderr_color_mt(ctx.category)，用 spdlog 默认级别（info），
        // 会绕开调用方本该经 PkLogEnsureLogger 设的 minLevel，让过滤策略悄悄
        // 失真而不报错——brief 也没有要求这条路径，多出来就是负债。
        //
        // 选择「去掉兜底」（brief 倾向 a），但不用 assert/abort 让整个宿主进程
        // （Krita 是交互式绘画应用）因为一次日志调用点配置遗漏就崩掉：
        // - sink 通道（上面 PkLogDispatchToSinks 已经分发）不受影响，
        //   Flutter 侧订阅仍然收到这条消息；
        // - spdlog 侧显式打一行诊断到 stderr 再跳过，不产出一条误导性的、
        //   级别过滤失真的日志行，也不假装什么都没发生；
        // - PkLogFatal 的"终止进程"契约独立于 logger 是否建好，照样 abort，
        //   不因为这条防御路径悄悄失效。
        std::fprintf(stderr,
                     "PkLogEmit: no logger for category \"%s\" -- call "
                     "PkLogEnsureLogger first (message dropped from spdlog "
                     "sink, still delivered to PkLogAddSink subscribers)\n",
                     ctx.category);
        if (level == PkLogFatal) {
            std::abort();
        }
        return;
    }
    logger->log(spdlog::source_loc{ctx.file, ctx.line, ctx.function},
                mapLevel(level), message);

    if (level == PkLogFatal) {
        std::abort();
    }
}
