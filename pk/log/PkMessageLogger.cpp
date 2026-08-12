#include "PkMessageLogger.h"

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
