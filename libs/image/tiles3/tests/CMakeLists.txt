# R-65：本组测试从真 Qt 测试栈（kritatestsdk）迁到 pk 测试栈
# （kritatestsdk_pk，由 pk_add_test 自动追加）。pk_add_test 内部转调
# kis_add_test(TEST_NAME <原 target 名>)，所以 add_test 名、NAME_PREFIX、
# KRITA_TESTS_TARGET 全部与迁移前逐字一致。
#
# R-65 修复轮（选项 A 判定后）：
#   * kis_tile_data_pooler_test / kis_low_memory_tests —— 原本只卡在 pk 栈缺
#     `QTest::qSleep`；探针实测真 Qt 的 qSleep 是纯阻塞 sleep（evidence/qsleep-probe*），
#     垫片 sdk/tests/compat/PkTestSleepShim.h 使其在 pk 栈上真绿 ⇒ 留在 pk 栈。
#   * kis_memory_window_test —— 原本只卡在 `QTest::qWait`；两处调用点按
#     sdk/tests/README「事件循环测试改造模式」判为 PATTERN-1（其后是已同步完成的
#     写/读操作，等待是调试用节流，语义中立），删掉两次等待后留在 pk 栈。
#   * kis_tiled_data_manager_test / kis_lockless_stack_test —— 卡在
#     `QBENCHMARK_ONCE`/`QBENCHMARK`（决策 D-30 明确不实现，本轮不得实现）
#     ⇒ 按选项 B 退回真 Qt 测试栈，与迁移前逐字一致。
pk_add_tests(
    kis_chunk_allocator_test
    kis_store_limits_test
    kis_swapped_data_store_test
    kis_tile_data_store_test
    kis_tile_data_pooler_test
    kis_low_memory_tests
    kis_memory_window_test
    LINK_LIBRARIES kritaimage
    )

# 退回真 Qt 测试栈的两个（选项 B）：与迁移前逐字一致——同样的
# kis_add_tests(<name>.cpp … LINK_LIBRARIES kritaimage kritatestsdk)。
kis_add_tests(
    kis_tiled_data_manager_test.cpp
    kis_lockless_stack_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk
    )

set_tests_properties(libs-image-tiles3-kis_low_memory_tests PROPERTIES TIMEOUT 180)

# R-67 常驻回归载体（判据②）：KisMemoryWindow 的映射窗口必须页对齐。
# 是**复刻调用点形状的 driver**，不是真实测试类 —— 理由见该 .cpp 文件头
# （真实测试类 kis_memory_window_test.cpp 卡在 R-65 的测试栈迁移上编不过）。
# 刻意**不用** kis_add_test：那个宏套的是 kritatestsdk（真 Qt 测试栈），而本文件
# 零 Qt 头、零 Qt 链接，形态照 libs/impex/tests 里 R-59 交付的那一个。
add_executable(kis_memory_window_alignment_test kis_memory_window_alignment_test.cpp)
ecm_mark_nongui_executable(kis_memory_window_alignment_test)
target_link_libraries(kis_memory_window_alignment_test PRIVATE kritaimage)
add_test(NAME libs-image-tiles3-kis_memory_window_alignment_test
         COMMAND kis_memory_window_alignment_test)
