#include "PkLoggingCategory.h"

#include "PkLogBackend.h"

PkLoggingCategory::PkLoggingCategory(const char *name, PkLogLevel minLevel)
    : _name(name), _minLevel(minLevel)
{
    // 构造即注册：没经这一步的分类名，PkLogEmit 只打诊断行、不落 spdlog
    // （PkLogBackend.h 的契约）。幂等——同名 logger 已存在时只 set_level。
    PkLogEnsureLogger(_name, _minLevel);
}

const char *PkLoggingCategory::categoryName() const
{
    return _name;
}

bool PkLoggingCategory::isDebugEnabled() const
{
    return PkLogDebug >= _minLevel;
}

bool PkLoggingCategory::isInfoEnabled() const
{
    return PkLogInfo >= _minLevel;
}

bool PkLoggingCategory::isWarningEnabled() const
{
    return PkLogWarning >= _minLevel;
}

bool PkLoggingCategory::isCriticalEnabled() const
{
    return PkLogCritical >= _minLevel;
}

void PkLoggingCategory::PkSetLevel(PkLogLevel level)
{
    _minLevel = level;
}
