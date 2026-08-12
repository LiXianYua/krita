#include "PkMessageLogger.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "PkLogBackend.h"
#include "PkLogSink.h"

namespace {

// printf 变参族的公共格式化尾巴：先用 vsnprintf(nullptr,0,...) 探出所需长度
// （同一份 va_list 不能用两次，所以 va_copy 一份专门用来探长度），再格式化
// 进正好大小的缓冲区。needed<0 是 vsnprintf 自身的编码错误，退化成空串而不是
// 让调用点崩掉——日志路径本身出错不该拖垮宿主。
std::string pkFormatVaList(const char *msg, va_list args)
{
    va_list probe;
    va_copy(probe, args);
    const int needed = std::vsnprintf(nullptr, 0, msg, probe);
    va_end(probe);
    if (needed < 0) {
        return std::string();
    }
    std::vector<char> buf(static_cast<std::size_t>(needed) + 1);
    std::vsnprintf(buf.data(), buf.size(), msg, args);
    return std::string(buf.data(), static_cast<std::size_t>(needed));
}

} // namespace

PkMessageLogger::PkMessageLogger(const char *file, int line, const char *fn,
                                  const PkLoggingCategory *cat)
{
    _ctx.file = file;
    _ctx.line = line;
    _ctx.function = fn;
    _ctx.category = cat ? cat->categoryName() : "default";
}

PkDebug PkMessageLogger::debug()
{
    return PkDebug(PkLogDebug, _ctx);
}

PkDebug PkMessageLogger::info()
{
    return PkDebug(PkLogInfo, _ctx);
}

PkDebug PkMessageLogger::warning()
{
    return PkDebug(PkLogWarning, _ctx);
}

PkDebug PkMessageLogger::critical()
{
    return PkDebug(PkLogCritical, _ctx);
}

PkDebug PkMessageLogger::fatal()
{
    return PkDebug(PkLogFatal, _ctx);
}

void PkMessageLogger::debug(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    const std::string text = pkFormatVaList(msg, args);
    va_end(args);
    PkLogEmit(PkLogDebug, _ctx, text);
}

void PkMessageLogger::info(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    const std::string text = pkFormatVaList(msg, args);
    va_end(args);
    PkLogEmit(PkLogInfo, _ctx, text);
}

void PkMessageLogger::warning(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    const std::string text = pkFormatVaList(msg, args);
    va_end(args);
    PkLogEmit(PkLogWarning, _ctx, text);
}

void PkMessageLogger::critical(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    const std::string text = pkFormatVaList(msg, args);
    va_end(args);
    PkLogEmit(PkLogCritical, _ctx, text);
}

void PkMessageLogger::fatal(const char *msg, ...) const
{
    va_list args;
    va_start(args, msg);
    const std::string text = pkFormatVaList(msg, args);
    va_end(args);
    PkLogEmit(PkLogFatal, _ctx, text);
    // PkLogEmit 对 PkLogFatal 契约上必 abort（PkLogBackend.h）；这行是双保险，
    // 让 [[noreturn]] 不依赖"契约不会被将来悄悄改掉"这件事本身来保证成立。
    std::abort();
}

// ---------------------------------------------------------------------------
// qInstallMessageHandler：借道已有的 sink 注册表实现，不碰 PkLogBackend.cpp
// （细节见 PkMessageLogger.h 里 QtMessageHandler 声明处的注释）。
// ---------------------------------------------------------------------------

namespace {

QtMessageHandler g_installedHandler = nullptr;
int g_installedHandlerSinkHandle = 0;

void pkInvokeInstalledHandler(PkLogLevel level, const PkLogContext &ctx,
                               const char *message, void *)
{
    if (g_installedHandler) {
        g_installedHandler(level, ctx, message);
    }
}

} // namespace

QtMessageHandler qInstallMessageHandler(QtMessageHandler handler)
{
    QtMessageHandler previous = g_installedHandler;
    if (g_installedHandlerSinkHandle != 0) {
        PkLogRemoveSink(g_installedHandlerSinkHandle);
        g_installedHandlerSinkHandle = 0;
    }
    g_installedHandler = handler;
    if (handler) {
        g_installedHandlerSinkHandle = PkLogAddSink(&pkInvokeInstalledHandler, nullptr);
    }
    return previous;
}
