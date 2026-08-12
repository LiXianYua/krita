#pragma once

#include "PkLogLevel.h"

// Q_LOGGING_CATEGORY / QLoggingCategory 的零 Qt 替代品。
//
// 语义（.superpowers/sdd/R-08/task-3-brief.md，与真 Qt 探针实测对齐）：
// - 三参 Q_LOGGING_CATEGORY(name, "string", QtXxxMsg) 里第三参是"默认最小
//   级别"，不是"这一条消息的级别"——minLevel 存进分类对象，isXxxEnabled()
//   拿被测级别跟它比。
// - PkLogLevel 的取值刻意保持单调（Debug<Info<Warning<Critical<Fatal），
//   所以 "level >= minLevel" 就是 QLoggingCategory::isXxxEnabled() 的对应
//   判据。真 Qt 的 QtInfoMsg 数值是 4（不在严重性顺序上），那次跳跃是
//   compat 垫片（Task 4）按名字映射时才要关心的事，这里不用管。
// - 两参形态（没给 minLevel）默认全开，对应默认参数 PkLogDebug（最低级别）。
class PkLoggingCategory
{
public:
    explicit PkLoggingCategory(const char *name, PkLogLevel minLevel = PkLogDebug);

    const char *categoryName() const;

    bool isDebugEnabled() const;
    bool isInfoEnabled() const;
    bool isWarningEnabled() const;
    bool isCriticalEnabled() const;

    // QT_LOGGING_RULES 环境变量过滤判定为范围外（零调用点）——这是程序内的
    // 等价手段：运行时改一个分类的最低级别。只对调用方直接持有的非 const
    // 实例生效；经 PK_LOGGING_CATEGORY 生成的单例是 const 引用，够不到它。
    void PkSetLevel(PkLogLevel level);

private:
    const char *_name;
    PkLogLevel _minLevel;
};

// ---------------------------------------------------------------------------
// PK_LOGGING_CATEGORY / PK_DECLARE_LOGGING_CATEGORY：Q_LOGGING_CATEGORY 家族
// 的对应物。生成的是**函数**而不是全局对象——kis_debug.h:21 声明的正是
// `extern const QLoggingCategory &_41000();`，qC* 宏（PkMessageLogger.h）
// 对实参补 `()` 来调用它，拿到的是函数体里那个 static 单例的引用。
// ---------------------------------------------------------------------------

#define PK_DECLARE_LOGGING_CATEGORY(name) const PkLoggingCategory &name();

#define PK_LOGGING_CATEGORY_2(name, string)                                  \
    const PkLoggingCategory &name()                                          \
    {                                                                        \
        static const PkLoggingCategory cat(string);                         \
        return cat;                                                          \
    }
#define PK_LOGGING_CATEGORY_3(name, string, level)                           \
    const PkLoggingCategory &name()                                          \
    {                                                                        \
        static const PkLoggingCategory cat(string, level);                  \
        return cat;                                                          \
    }
#define PK_LOGGING_CATEGORY_PICK(_1, _2, _3, NAME, ...) NAME
#define PK_LOGGING_CATEGORY(...)                                             \
    PK_LOGGING_CATEGORY_PICK(__VA_ARGS__, PK_LOGGING_CATEGORY_3,             \
                              PK_LOGGING_CATEGORY_2)(__VA_ARGS__)
