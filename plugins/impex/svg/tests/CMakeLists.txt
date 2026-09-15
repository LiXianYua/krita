# R-82：本目录的测试源 basename 是 `kis_svg_test`（不是 `KisSvgTest`），
# 原树用 TEST_NAME KisSvgTest 把 target/ctest 名保持成 KisSvgTest —— 逐字转发。
pk_add_test(
    kis_svg_test
    TEST_NAME KisSvgTest
    REGISTER_FILTERS
        registerKisSVGImportFilter
    LINK_LIBRARIES
        kritaimpex
        kritasvgimport
    BROKEN
    )

add_executable(KisSvgImportPolicyTest KisSvgImportPolicyTest.cpp ../svg_import_policy.cpp)
target_include_directories(KisSvgImportPolicyTest PRIVATE ..)
add_test(NAME KisSvgImportPolicyTest COMMAND KisSvgImportPolicyTest)

add_test(NAME KisSvgQtFreeSourceTest
    COMMAND ${CMAKE_COMMAND}
        -DSVG_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../kis_svg_import.cc
        -P ${CMAKE_CURRENT_SOURCE_DIR}/KisSvgQtFreeSourceTest.cmake)
