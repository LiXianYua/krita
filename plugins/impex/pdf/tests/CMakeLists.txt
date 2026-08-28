kis_add_test(
    KisPdfTest.cpp
    TEST_NAME KisPdfTest
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(KisPdfImportPolicyTest KisPdfImportPolicyTest.cpp ../pdf_import_policy.cpp)
target_include_directories(KisPdfImportPolicyTest PRIVATE ..)
target_link_libraries(KisPdfImportPolicyTest PRIVATE Poppler::Cpp)
add_test(NAME KisPdfImportPolicyTest COMMAND KisPdfImportPolicyTest)
