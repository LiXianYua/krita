pk_add_test(
    kis_jpegxl_test
    REGISTER_FILTERS
        registerJPEGXLImportFilter
        registerJPEGXLExportFilter
    LINK_LIBRARIES
        kritametadata kritaimpex
        kritajxlimport kritajxlexport
    )

add_executable(jxl_validation_probe jxl_validation_probe.cpp)
target_link_libraries(jxl_validation_probe PRIVATE pkport pkvariant ${JPEGXL_LIBRARIES})
target_include_directories(jxl_validation_probe SYSTEM PRIVATE ${JPEGXL_INCLUDE_DIR})
add_test(NAME jxl_validation_probe COMMAND jxl_validation_probe)
