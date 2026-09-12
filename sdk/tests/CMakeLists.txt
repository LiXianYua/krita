add_library(kritatestsdk INTERFACE)

target_link_libraries(kritatestsdk INTERFACE
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Widgets
    kritaglobal
    pktest
)

target_compile_definitions(kritatestsdk INTERFACE
    FILES_DEFAULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    SYSTEM_RESOURCES_DATA_DIR="${CMAKE_SOURCE_DIR}/data/"
)

# R-65：pk 测试栈接线（kritatestsdk_pk INTERFACE target + pk_add_test/pk_add_tests）。
# 本目录在根 CMakeLists.txt 里排在 libs/plugins/benchmarks 之前，所以这里定义的
# 函数与 INTERFACE target 对下游全部目录可见。上面的 kritatestsdk 一个字不动。
include(pk_test_support.cmake)

if(BUILD_TESTING)
    # The standalone S-03-b shell maps QObject to PkTestObject and generates
    # this binder in its own binary directory.  The transitional main tree
    # keeps TestKUndo2Stack as a real QObject and dispatches it through QTest,
    # so its historical same-TU binder include must resolve to a no-op here.
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_testkundo2stack.inc"
        CONTENT "// Main-tree QObject branch: QTest discovers slots through moc.\n"
    )

    kis_add_test(
        PkSimpleTestBridgeTest.cpp
        LINK_LIBRARIES kritatestsdk pkconcurrent
    )
    target_compile_definitions(PkSimpleTestBridgeTest PRIVATE KRITA_TESTSDK_PK_NATIVE)

    # R-65 机器自测（本 Task 的「证明 target」）：sdk/tests 内的一个真实测试类
    # 走 pk_add_test 全链路——pk_test_moc 生成 binder、wrap TU 含入、聚合头
    # -include 预激活、注册转调 kis_add_test。它零 Krita 库依赖，所以红了就是
    # 机器坏了，不是测试面缺垫片。
    pk_add_test(PkTestSupportSelfTest)
endif()

add_library(kritatransformmaskstubs STATIC
    KisDumbTransformMaskParams.cpp
    KisDumbAnimatedTransformMaskParamsHolder.cpp
    KritaTransformMaskStubs.cpp
)

generate_export_header(kritatransformmaskstubs BASE_NAME kritatransformmaskstubs)

target_link_libraries(kritatransformmaskstubs INTERFACE kritatestsdk)
target_link_libraries(kritatransformmaskstubs PUBLIC kritaimage)
