kis_add_tests(
    kis_tiled_data_manager_test.cpp
    kis_low_memory_tests.cpp
    kis_lockless_stack_test.cpp
    kis_chunk_allocator_test.cpp
    kis_memory_window_test.cpp
    kis_store_limits_test.cpp
    kis_swapped_data_store_test.cpp
    kis_tile_data_store_test.cpp
    kis_tile_data_pooler_test.cpp
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
