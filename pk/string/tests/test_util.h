#pragma once
#include <cstdio>

extern int g_failures;

#define _expect(cond, msg)                                        \
    do {                                                          \
        if (!(cond)) {                                            \
            ++g_failures;                                         \
            std::printf("FAIL: %s  (%s:%d)\n", (msg),             \
                        __FILE__, __LINE__);                      \
        }                                                         \
    } while (0)
