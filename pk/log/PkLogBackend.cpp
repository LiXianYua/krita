// 唯一 #include <spdlog/spdlog.h> 的文件——pklog 的使用者对 spdlog 一无所知
// （CMakeLists.txt 里 target_link_libraries(pklog PRIVATE spdlog::spdlog)）。
// 换后端只用改这一个翻译单元。
#include "PkLogBackend.h"

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
        // 调用方本应先 PkLogEnsureLogger 建好分类 logger；这里兜底防止漏建
        // 时直接崩，退化行为是"用默认级别的 stderr logger"。
        logger = spdlog::stderr_color_mt(ctx.category);
    }
    logger->log(spdlog::source_loc{ctx.file, ctx.line, ctx.function},
                mapLevel(level), message);

    if (level == PkLogFatal) {
        std::abort();
    }
}
