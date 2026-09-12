#pragma once
//
// R-65 修复轮 · `QTest::qSleep` 的过渡垫片 —— **正家是 pk/test（S0）**，这里只是
// 让 tiles3 组里两个真实测试类在 S0 交付前能编过、跑绿的最小实现。
//
// 为什么可以加这一条（brief §2.1 选项 A 的条件：「先探针、后动手，确为精确等价才允许」）：
//   探针 `evidence/qsleep-probe.cpp`（原始输出 `evidence/qsleep-probe-output.txt`）
//   对真 Qt 5.15.7 实测：
//     * QTest::qSleep(N) 的实测耗时与「纯阻塞 N 毫秒」逐格一致（N∈{1,200,500,1200}，
//       与 nanosleep({N/1000,(N%1000)*1e6}) 的最小 elapsed 相同，差异只在 ms 级调度噪声）；
//     * 探针 ms=0 的 case 触发了 Qt 自己的断言
//       `ASSERT: "ms > 0" in .../qtbase/src/testlib/qtestcase.cpp, line 2484`
//       —— 这直接指认了它就在 qtestcase.cpp 的 qSleep 实现里（Unix 分支即 nanosleep）。
//   ⇒ 真 Qt 的 qSleep 是**纯阻塞 sleep**，不推进事件循环；本垫片用**逐字照抄 Qt 的
//     Unix 分支**（nanosleep）实现，语义精确等价。
//
// 与薄壳先例 `.exec/shell/kritaimage/CMakeLists.txt` 的 `PkTest::qSleep`（用 usleep）的
// 差异：本垫片用 nanosleep 而不是 usleep(ms*1000)——usleep 的 POSIX 约定是
// `usec < 1000000`（即 ms<1000），本机 arm64/macOS 上恰好容错、但 Linux 上是
// EINVAL 或未定义；nanosleep 对任意 ms 都与 Qt 逐字一致，所以选它。
//
// 与 Qt 的**已知偏离**（登记）：Qt 的 qSleep 对 ms<=0 走 QTEST_ASSERT 直接 abort
// （探针实测本 build 该断言生效）；本垫片按薄壳先例改成 `if (ms > 0)` 静默返回，
// 不 abort。当前全部调用点传的都是正数（500 / 200 / 1），不受影响。
//
// 归属（登记缺口）：`pk/test/README.md` §2 把 `qSleep`（实测 36 处）判给 **S0**；
// 本垫片不改变该归属，S0 交付 `pk/test` 的正家实现后本文件应删。
//
// 只在 pk 测试栈可见：本头由 `sdk/tests/PkTestCompatAll.h` 拉入，而后者只被
// `kritatestsdk_pk` 以 `-include` 注入 pk_add_test 目标的 TU —— `kritatestsdk`
// （真 Qt 测试栈）拿不到它，真 Qt 的 QTest::qSleep 不会被本定义遮蔽。

#include <ctime>

namespace PkTest {

inline void qSleep(int ms)
{
    // 逐字照抄 Qt qtestcase.cpp 的 Unix 分支：
    //     struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    //     nanosleep(&ts, nullptr);
    if (ms > 0) {
        struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
        nanosleep(&ts, nullptr);
    }
}

} // namespace PkTest
