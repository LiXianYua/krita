#pragma once

#include <cstdio>

// `libs/global/kis_assert.h` 的最小替身。
//
// 被测实现 `kis_fill_interval_map.cpp` 通过 `kis_fill_sanity_checks.h` 走到
// **`KIS_SAFE_ASSERT_RECOVER`**：那个头无条件 `#define ENABLE_FILL_SANITY_CHECKS`，
// 所以 `cropInterval()` 里那段 assert 是**真的编进来了**，不是被 `#ifdef` 切掉的。
//
// 宏体逐字抄自 `libs/global/kis_assert.h:126`：
//     if (!(cond) && (kis_safe_assert_recoverable(#cond,__FILE__,__LINE__), true))
// 形状必须一样 —— 它是个**带悬空 `if` 的语句宏**，后面跟调用点自己写的恢复块。
// 换成 `assert()` 或 `do{}while(0)` 会让恢复块变成孤立复合语句，行为就变了。

inline void kis_safe_assert_recoverable(const char *assertion, const char *file, int line)
{
    std::fprintf(stderr, "SAFE ASSERT (krita): \"%s\" in file %s, line %d\n", assertion, file, line);
}

// `qt_noop()` 在真品里来自 `<QtGlobal>`。pk/test 的 `compat/QtGlobal` 只交付了
// R-11 范围内有真实调用点的那几项（判据①），没有它 —— 而 pk/test 是本任务的
// **只读依赖**，不能往里加。所以在这里补一个同义定义，属于脚手架层面的权宜，
// 不是对 pk/test 范围的意见。
inline void qt_noop() {}

#define KIS_SAFE_ASSERT_RECOVER(cond) if (!(cond) && (kis_safe_assert_recoverable(#cond,__FILE__,__LINE__), true))
#define KIS_SAFE_ASSERT_RECOVER_BREAK(cond) KIS_SAFE_ASSERT_RECOVER(cond) { break; }
#define KIS_SAFE_ASSERT_RECOVER_RETURN(cond) do { KIS_SAFE_ASSERT_RECOVER(cond) { return; } } while (0)
#define KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(cond, val) do { KIS_SAFE_ASSERT_RECOVER(cond) { return (val); } } while (0)
#define KIS_SAFE_ASSERT_RECOVER_NOOP(cond) do { KIS_SAFE_ASSERT_RECOVER(cond) { qt_noop(); } } while (0)
