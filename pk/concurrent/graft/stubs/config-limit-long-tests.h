#pragma once
// 试接垫片 —— 不是对 Krita 源树的改动，也不是最终交付物。
//
// 真品 config-limit-long-tests.h 由 CMake 从仓库根的
// config-limit-long-tests.h.cmake 生成（`#cmakedefine LIMIT_LONG_TESTS 1`），
// 本仓库的独立薄壳 pk/concurrent 工程不跑那条 configure_file 规则，源树里
// 也确实没有生成过 config-limit-long-tests.h 这个文件（只有 .cmake 模板）。
//
// LIMIT_LONG_TESTS 未定义是 KDECI CI 默认值（该缓存变量默认关闭）。
// kis_lockless_stack_test.cpp::stressTestBulkPop() 的 #ifdef/#else 两个分支
// 本身取值完全相同（numThreads=3、numObjects=10000000），所以本文件"不定义
// LIMIT_LONG_TESTS"这条选择对试接目标的行为没有任何影响——两条分支殊途同归。
