# kis_input_transform_policy_test.cpp:189 无条件 #include 了同 TU 的 binder
# （pk/test/CMakeLists.txt 的 ODR 硬规则），而 KisInputTransformPolicyTest 在主树里
# 仍是真 QObject + Q_OBJECT，靠 Qt AUTOMOC + QTest 做槽发现。历史遗留的同 TU binder
# include 必须解析成一个 no-op。形制照 sdk/tests/CMakeLists.txt:20-23。
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_kis_input_transform_policy_test.inc"
    CONTENT "// Main-tree QObject branch: QTest discovers slots through moc.\n"
)

kis_add_test(
    kis_input_manager_test.cpp
    TEST_NAME KisInputManagerTest
    LINK_LIBRARIES kritainput kritatestsdk
)

kis_add_test(
    kis_input_transform_policy_test.cpp
    TEST_NAME KisInputTransformPolicyTest
    LINK_LIBRARIES kritainput kritatestsdk
)

add_executable(PkInputSemanticsTest pk_input_semantics_test.cpp)
target_link_libraries(PkInputSemanticsTest PRIVATE kritainput)
add_test(NAME libs-input-PkInputSemanticsTest COMMAND PkInputSemanticsTest)
