kis_add_test(
    kis_xcf_test.cpp
    TEST_NAME kis_xcf_test
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(KisXcfParserPolicyTest KisXcfParserPolicyTest.cpp)
target_link_libraries(KisXcfParserPolicyTest PRIVATE xcftools)
target_compile_definitions(KisXcfParserPolicyTest PRIVATE
    XCF_PARSER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/data/sources/simple.xcf")
add_test(NAME KisXcfParserPolicyTest COMMAND KisXcfParserPolicyTest)
