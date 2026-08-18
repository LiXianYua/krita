#pragma once

// Q_ASSERT 用于运行时断言（仅在 DEBUG 时生效，RELEASE 时编译成空操作）。
// 在 Qt 环境下由 qassert.h 提供；在无 Qt 环境下用标准 C++ assert 替代。
#ifdef NDEBUG
    #define Q_ASSERT(cond) (static_cast<void>(0))
#else
    #include <cassert>
    #define Q_ASSERT(cond) assert(cond)
#endif
