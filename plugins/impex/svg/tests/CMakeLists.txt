krita_add_broken_unit_test(
    kis_svg_test.cpp
    TEST_NAME KisSvgTest
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(KisSvgImportPolicyTest KisSvgImportPolicyTest.cpp ../svg_import_policy.cpp)
target_include_directories(KisSvgImportPolicyTest PRIVATE ..)
add_test(NAME KisSvgImportPolicyTest COMMAND KisSvgImportPolicyTest)
