pk_add_test(
    KisOraTest
    REGISTER_FILTERS
        registerOraImportFilter
        registerOraExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaoraimport kritaoraexport
        kritalcmsengine
    )

add_executable(KisOraSavePolicyTest
    KisOraSavePolicyTest.cpp
    ../ora_save_policy.cpp
)
target_include_directories(KisOraSavePolicyTest PRIVATE ..)
target_link_libraries(KisOraSavePolicyTest PRIVATE pkxml)
add_test(NAME KisOraSavePolicyTest COMMAND KisOraSavePolicyTest)

add_test(NAME KisOraFailurePropagationSourceTest
    COMMAND ${CMAKE_COMMAND}
        -DORA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -P ${CMAKE_CURRENT_SOURCE_DIR}/KisOraFailurePropagationSourceTest.cmake)
