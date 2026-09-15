pk_add_test(
    kis_csv_test
    REGISTER_FILTERS
        registerKisCSVImportFilter
        registerKisCSVExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritacsvimport kritacsvexport
        kritalcmsengine
    BROKEN
    )

add_executable(kis_csv_write_all_test csv_write_all_test.cpp)
target_link_libraries(kis_csv_write_all_test PRIVATE kritaimpex)
add_test(NAME kis_csv_write_all_test COMMAND kis_csv_write_all_test)
