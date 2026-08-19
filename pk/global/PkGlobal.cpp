// PkGlobal.h 的 Q_ASSERT 触发路径。真 Qt 对应 qt_assert（qglobal.h:849-850，
// Q_CORE_EXPORT void qt_assert(...) noexcept，实现在 qglobal.cpp）。照抄形态：
// 打一行到 stderr 然后 abort()。abort() 会经 SIGABRT 结束进程，不跑析构、不 flush
// 常规 stdio —— 与 Qt 一致（Qt 这里也是 fprintf + abort）。
#include "PkGlobal.h"

#include <cstdio>
#include <cstdlib>

void pk_qt_assert(const char *what, const char *file, int line)
{
    std::fprintf(stderr, "ASSERT: %s in file %s, line %d\n", what, file, line);
    std::abort();
}
