pk_add_test(
    kis_xcf_test
    REGISTER_FILTERS
        registerKisXCFImportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaxcfimport
        kritalcmsengine
    )

add_executable(KisXcfParserPolicyTest KisXcfParserPolicyTest.cpp)
target_link_libraries(KisXcfParserPolicyTest PRIVATE xcftools)
target_compile_definitions(KisXcfParserPolicyTest PRIVATE
    XCF_PARSER_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/data/sources/simple.xcf")
add_test(NAME KisXcfParserPolicyTest COMMAND KisXcfParserPolicyTest)
