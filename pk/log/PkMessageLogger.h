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

    // printf 风格重载：真 Qt 的 QMessageLogger 除了上面的无参流式版本，还有
    // 一族 `void debug(const char *msg, ...) const` ——kis_debug.h 的调用点
    // 两种形态都用，实测：qDebug(fmt,...) 55 处、qWarning(fmt,...) 60 处、
    // qFatal(fmt,...) 65 处（qFatal 只有这一种形态，从没见过 qFatal() 流式
    // 写法）。落地在 PkMessageLogger.cpp，直接走 PkLogEmit——不经 PkDebug，
    // 因为这里已经是格式化完的最终文本，不需要流式拼接的分隔符/引号逻辑。
    void debug(const char *msg, ...) const;
    void info(const char *msg, ...) const;
    void warning(const char *msg, ...) const;
    void critical(const char *msg, ...) const;
    // qFatal 45+ 处调用点依赖它不返回（PkLogBackend.h 契约：level==PkLogFatal
    // 时 PkLogEmit 内部必 abort）。这里额外加 [[noreturn]] 如实标注，并在
    // PkLogEmit 之后补一道 std::abort() 兜底——不依赖"契约不会被将来悄悄改掉"
    // 这件事本身来保证不返回。
    [[noreturn]] void fatal(const char *msg, ...) const;

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

// 让位给真 Qt（R-34）：qDebug/qInfo/qWarning/qCritical/qFatal、qPrintable 宏与
// QtMessageHandler/qInstallMessageHandler 是真 Qt qlogging.h 也有的名字，real Qt 已进
// TU（QT_CORE_LIB）时让位。qCDebug/qCInfo/qCWarning/qCCritical 只在真 Qt 的
// qloggingcategory.h 里定义（libs/global 基线测试的 include 集不拉它），用逐项
// `#if !defined(...)` 让位——真 Qt 已可见就让位、否则用 pk 版（kis_debug.h 的
// dbgKrita/warnKrita 一族经它工作，real Qt 在场时也依赖 pk 版）。
// PkMessageLogger 类本身是 pk 自有名，保留。
#if !defined(qCDebug)
#define qCDebug(category)    PK_QCLOG_IMPL(category, isDebugEnabled, debug)
#endif
#if !defined(qCInfo)
#define qCInfo(category)     PK_QCLOG_IMPL(category, isInfoEnabled, info)
#endif
#if !defined(qCWarning)
#define qCWarning(category)  PK_QCLOG_IMPL(category, isWarningEnabled, warning)
#endif
#if !defined(qCCritical)
#define qCCritical(category) PK_QCLOG_IMPL(category, isCriticalEnabled, critical)
#endif

// ---------------------------------------------------------------------------
// qDebug/qInfo/qWarning/qCritical/qFatal：无分类版本。真 Qt 里这五个本身也是
// 宏（qlogging.h：`#define qDebug QMessageLogger(file,line,func).debug`），
// 不是普通自由函数——只有宏才能让 __FILE__/__LINE__/__func__ 落在调用点而
// 不是这个头文件里。这里照抄同样的做法。故意不加 Pk 前缀，理由与 qCDebug 一族
// 相同：D-23 改名表没有它们，S 线不打算动这 1000+ 处调用点。
//
// 不经 minLevel 过滤/惰性求值——真 Qt 的无分类版本本身就不受 QLoggingCategory
// 管，PkMessageLogger 的 cat==nullptr 分支把 ctx.category 落成 "default"
// （已实测事实），调用点该表达式该求值就求值，与 qC* 家族的按需求值语义不同。
//
// `.debug` 之类不带括号——留给调用点自己的 `()` 或 `("fmt", ...)` 决定落在
// 上面哪一族重载（无参流式 vs printf 变参），两种形态调用点都真实存在。
// ---------------------------------------------------------------------------

#if !defined(qDebug)
#define qDebug    PkMessageLogger(__FILE__, __LINE__, __func__).debug
#endif
#if !defined(qInfo)
#define qInfo     PkMessageLogger(__FILE__, __LINE__, __func__).info
#endif
#if !defined(qWarning)
#define qWarning  PkMessageLogger(__FILE__, __LINE__, __func__).warning
#endif
#if !defined(qCritical)
#define qCritical PkMessageLogger(__FILE__, __LINE__, __func__).critical
#endif
#if !defined(qFatal)
#define qFatal    PkMessageLogger(__FILE__, __LINE__, __func__).fatal
#endif

// qPrintable(x)：真 Qt 是 `(x).toLocal8Bit().constData()`。这里鸭子类型地
// 只要求 x 有 `.PkToUtf8()`（PkString 族，与 PkDebug.h 的 PkDebugHasToUtf8
// 探测同一个成员名），纯文本宏替换——pk/log 不需要认识 PkString 的定义，
// 展开发生在调用点自己已经 #include 过 pk/string 的那个翻译单元里
// （pk/log 不许 include/链接 pk/string 的硬约束因此不受影响）。
// 与真 Qt 同款的生命周期注意事项：PkToUtf8() 返回的临时 std::string 活到
// 整条语句结束为止，指针只能在同一语句内用（例如直接喂给 printf 变参）。
#if !defined(qPrintable)
#define qPrintable(str) ((str).PkToUtf8().c_str())
#endif

// QtMessageHandler / qInstallMessageHandler：真 Qt 签名是
// `void (*)(QtMsgType, const QMessageLogContext &, const QString &)`。这里
// 消息参数改用 `const char *`（不是 QString/PkString）——与 PkLogSinkFn
// （PkLogSink.h）同一个理由：pk/log 不许依赖 pk/string，const char* 是这一层
// 已经在用的通用形态。已知局限：真实调用点里处理函数签名写的是
// `const QString &`（QString compat 垫片会把它改写成 `const PkString &`），
// 那样的签名与这里的 `const char *` 不匹配，编不过——这属于下一个把
// kis_debug.h/具体 handler 实现接进来的 Task 要处理的问题，不在本 Task 范围
// （brief 的"产出"只要求这个符号本身可用，没有承诺兼容那几处具体调用点的
// 精确签名）。
//
// 实现（PkMessageLogger.cpp）借道已有的 PkLogAddSink/PkLogRemoveSink
// （PkLogSink.h，Task 1 就有的公开 API），不碰 PkLogBackend.cpp——那是前面
// Task 的文件，本 Task 不顺手重构它。已知偏差：真 Qt 装了 handler 之后默认
// 输出（这里对应 spdlog 的 stderr 落盘）会被完全接管，这里只是"多一路"，
// PkLogBackend.cpp 该怎么落盘还怎么落盘，handler 收到的是旁路的第二份。
#if !defined(QT_CORE_LIB)
using QtMessageHandler = void (*)(PkLogLevel type, const PkLogContext &context,
                                   const char *message);
QtMessageHandler qInstallMessageHandler(QtMessageHandler handler);
#endif // !defined(QT_CORE_LIB)
