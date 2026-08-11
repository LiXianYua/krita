#pragma once
#include <cstdio>
extern int g_selftestFailures;
#define SELF_EXPECT(cond, msg)                                        \
    do {                                                              \
        if (!(cond)) {                                                \
            ++g_selftestFailures;                                     \
            std::printf("SELFTEST FAIL: %s  (%s:%d)\n", (msg),        \
                        __FILE__, __LINE__);                          \
        }                                                             \
    } while (0)
