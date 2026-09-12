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

    # R-65 全分支评审 B-1 修复：本 target 按设计**零 Krita 库依赖**
    # （`pk_add_test` 不传 LINK_LIBRARIES），于是 `libs/pigment` 不在它的 -I 上。
    # 而 Task 3 加的 `sdk/tests/compat/QBitArray` 要 `#include <PkBitArray.h>`
    # （该头住 `libs/pigment/`）——只有这一个 target 会炸，因为别的 pk target 都链
    # `kritaimage`（传递带 `libs/pigment`）。
    # **只补包含目录，不引入库依赖**（保住「零 Krita 库依赖 ⇒ 红了就是机器坏了」这条性质）。
    target_include_directories(PkTestSupportSelfTest PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/pigment)

    # 同一根因的第二处：聚合头（Task 3 扩了 `QDom*` 预激活）经
    # `pk/xml/compat/QDomDocument` → `PkXmlNode.h` 拉 `pugixml.hpp`，而 pugixml 的
    # include 目录是**经库目标传递**的，零依赖的自测 target 拿不到。
    # 链 `pkxml`（**pk 库，不是 Krita 库**）——它 PUBLIC 带 pkcontainer/pkstring/pugixml，
    # 于是「零 **Krita** 库依赖 ⇒ 红了就是机器坏了」这条性质仍然成立，
    # 且今后聚合头再加需要 pk 模块的垫片也不会再让本 target 静默红。
    # 注意：必须用**无关键字**签名——`kis_add_test`（KRITA_ADD_UNIT_TEST）用的是
    # 纯签名，CMake 要求同一个 target 的 target_link_libraries 全纯或全带关键字。
    target_link_libraries(PkTestSupportSelfTest pkxml)
endif()

add_library(kritatransformmaskstubs STATIC
    KisDumbTransformMaskParams.cpp
    KisDumbAnimatedTransformMaskParamsHolder.cpp
    KritaTransformMaskStubs.cpp
)

generate_export_header(kritatransformmaskstubs BASE_NAME kritatransformmaskstubs)

target_link_libraries(kritatransformmaskstubs INTERFACE kritatestsdk)
target_link_libraries(kritatransformmaskstubs PUBLIC kritaimage)
