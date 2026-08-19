#pragma once

#include <cstdio>

// `libs/global/kis_assert.h` 的最小替身——照抄
// pk/container/tests/graft/stubs/kis_assert.h（已交付的最小 stub，brief 第
// 12-15 行明确要求复用而不是重新设计）。候选 1
// （KisDatabaseTransactionLock.cpp）用到的是 `KIS_SAFE_ASSERT_RECOVER_RETURN`，
// 宏体逐字抄自 `libs/global/kis_assert.h:126`：
//     if (!(cond) && (kis_safe_assert_recoverable(#cond,__FILE__,__LINE__), true))
// 形状必须一样——它是个**带悬空 `if` 的语句宏**，后面跟调用点自己写的恢复块。

inline void kis_safe_assert_recoverable(const char *assertion, const char *file, int line)
{
    std::fprintf(stderr, "SAFE ASSERT (krita): \"%s\" in file %s, line %d\n", assertion, file, line);
}

inline void qt_noop() {}

#define KIS_SAFE_ASSERT_RECOVER(cond) if (!(cond) && (kis_safe_assert_recoverable(#cond,__FILE__,__LINE__), true))
#define KIS_SAFE_ASSERT_RECOVER_BREAK(cond) KIS_SAFE_ASSERT_RECOVER(cond) { break; }
#define KIS_SAFE_ASSERT_RECOVER_RETURN(cond) do { KIS_SAFE_ASSERT_RECOVER(cond) { return; } } while (0)
#define KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(cond, val) do { KIS_SAFE_ASSERT_RECOVER(cond) { return (val); } } while (0)
#define KIS_SAFE_ASSERT_RECOVER_NOOP(cond) do { KIS_SAFE_ASSERT_RECOVER(cond) { qt_noop(); } } while (0)
