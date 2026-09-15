pk_add_test(
    kis_exr_test
    REGISTER_FILTERS
        registerexrImportFilter
        registerEXRExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaexrimport kritaexrexport
        kritalcmsengine
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
