#pragma once

#include "PkDebug.h"
#include "PkLogLevel.h"
#include "PkLoggingCategory.h"

// QMessageLogger 的零 Qt 替代品：拿着 file/line/function/分类，按级别造一个
// PkDebug。是否落盘由 PkDebug 析构时的 PkLogEmit 决定——这个类本身不做使能
// 判断，使能判断与惰性求值都在下面的 qC* 宏里（构造 PkMessageLogger、以及
// 调用点 << 右侧的表达式，整个都在 for 循环体内，分类禁用时一次都不执行）。
class PkMessageLogger
{
public:
    // cat == nullptr：非分类日志（qDebug() 等自由函数用，Task 4 加），
    // 落进 PkLogContext::category 的默认值 "default"。
    PkMessageLogger(const char *file, int line, const char *fn,
                     const PkLoggingCategory *cat = nullptr);

    PkDebug debug();
    PkDebug info();
    PkDebug warning();
    PkDebug critical();
    PkDebug fatal();

private:
    PkLogContext _ctx;
};

// ---------------------------------------------------------------------------
// qCDebug/qCInfo/qCWarning/qCCritical：Krita 1296 处调用点直接写的名字，
// D-23 改名表不动它们（.superpowers/sdd/R-08/task-4-brief.md 已经定了这条：
// "不经改名"）——所以这里就是这几个真名，不是 Pk 前缀。qCFatal 不存在：
// 真 Qt 本身没有这个宏（kis_debug.h:132 附近的注释原样确认，"Qt does not
// yet define qCFatal"），我们也不多造一个 Qt 没有的东西。
//
// 惰性求值靠这个经典的单迭代 for 结构：分类禁用时 for 的条件一次性为
// false，循环体（构造 PkMessageLogger 并调 .xxx()，供调用点接 <<）一次都
// 不跑，也就是 << 右侧的表达式一次都不会被求值。for 里的变量名带 pkLog
// 前缀，避免撞调用点自己声明的同名局部变量。
// ---------------------------------------------------------------------------

#define PK_QCLOG_IMPL(category, enabledCheck, method)                        \
    for (bool pkLogCatEnabled = (category()).enabledCheck(); pkLogCatEnabled; \
         pkLogCatEnabled = false)                                            \
    PkMessageLogger(__FILE__, __LINE__, __func__, &(category())).method()

#define qCDebug(category)    PK_QCLOG_IMPL(category, isDebugEnabled, debug)
#define qCInfo(category)     PK_QCLOG_IMPL(category, isInfoEnabled, info)
#define qCWarning(category)  PK_QCLOG_IMPL(category, isWarningEnabled, warning)
#define qCCritical(category) PK_QCLOG_IMPL(category, isCriticalEnabled, critical)
