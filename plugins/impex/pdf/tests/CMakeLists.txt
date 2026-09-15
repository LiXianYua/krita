pk_add_test(
    KisPdfTest
    REGISTER_FILTERS
        registerKisPDFImportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritapdfimport
        kritalcmsengine
    )

add_executable(KisPdfImportPolicyTest KisPdfImportPolicyTest.cpp ../pdf_import_policy.cpp)
target_include_directories(KisPdfImportPolicyTest PRIVATE ..)
target_link_libraries(KisPdfImportPolicyTest PRIVATE Poppler::Cpp)
add_test(NAME KisPdfImportPolicyTest COMMAND KisPdfImportPolicyTest)
