kis_add_test(
    kis_exr_test.cpp
    TEST_NAME kis_exr_test
    LINK_LIBRARIES kritaimpex kritatestsdk
)

add_executable(KisExrImportPolicyTest
    KisExrImportPolicyTest.cpp
    ../exr_import_policy.cpp
)
target_include_directories(KisExrImportPolicyTest PRIVATE ..)
target_link_libraries(KisExrImportPolicyTest PRIVATE pkconfig pkxml)
add_test(NAME KisExrImportPolicyTest COMMAND KisExrImportPolicyTest)

add_test(NAME KisExrImportPolicySourceTest
    COMMAND ${CMAKE_COMMAND}
        -DEXR_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -P ${CMAKE_CURRENT_SOURCE_DIR}/KisExrImportPolicySourceTest.cmake)
