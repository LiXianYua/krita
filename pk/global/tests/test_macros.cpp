// Q_UNUSED / Q_ASSERT 两个宏的行为测试。
//
// 本文件由 CMake 单独加 -Wall -Werror 编译（见 CMakeLists.txt），所以
// unusedDoesNotWarn 里那个「声明了却没正经用」的变量是**真的在 -Werror 下编过**
// —— Q_UNUSED 若是空宏或缺 (void) 转型，-Wunused-variable 会当场把编译打断。
// 这是 Q_UNUSED 的唯一有效测试形态：断言它「把未使用变量压成不报警」。

#include "cases/macros_case.h"
#include "../PkGlobal.h"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include "pk_binder_macros_case.inc"

void PkMacrosCase::unusedDoesNotWarn()
{
    // -Wall -Werror：这行变量若没有被 Q_UNUSED 消费，编译直接失败。
    int pkUnusedVar = 42;
    Q_UNUSED(pkUnusedVar);
    PK_VERIFY(true);
}

void PkMacrosCase::assertTrueIsNoop()
{
    // Q_ASSERT(true) 不该中断执行：能走到下一行就算过。
    Q_ASSERT(true);
    PK_VERIFY(true);
}

void PkMacrosCase::assertFalseAborts()
{
#if defined(NDEBUG)
    // Release（NDEBUG）下 Q_ASSERT 展开成 ((void)0)，无 abort 可验。
    PK_SKIP("NDEBUG 下 Q_ASSERT 是空操作，无 SIGABRT 可验");
#else
    // Debug 构建下 Q_ASSERT(false) → pk_qt_assert → fprintf(stderr) + abort()
    // → 子进程以 SIGABRT 终止。fork 子进程验证，不让断言失败拖垮整个测试进程。
    const pid_t pid = fork();
    PK_VERIFY(pid >= 0);
    if (pid == 0) {
        // 子进程：Q_ASSERT(false) 应触发 pk_qt_assert；若宏被写坏成空操作，
        // 落到 _exit(0)，父进程的 WIFSIGNALED 断言会红。
        Q_ASSERT(false);
        _exit(0);
    }
    int status = 0;
    const pid_t waited = waitpid(pid, &status, 0);
    PK_VERIFY(waited == pid);
    PK_VERIFY(WIFSIGNALED(status));
    PK_VERIFY(WTERMSIG(status) == SIGABRT);
#endif
}

int run_macros_tests()
{
    PkMacrosCase tc;
    const char *argv[] = {"test_pkglobal"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
