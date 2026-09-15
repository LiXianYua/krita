pk_add_test(
    KisBrushTest
    REGISTER_FILTERS
        registerKisBrushImportFilter
        registerKisBrushExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritabrushimport kritabrushexport
        kritalcmsengine
    )

add_executable(KisAnimatedBrushAnnotationTest
    KisAnimatedBrushAnnotationTest.cpp
    ../animated_brush_annotation_data.cpp
)
target_include_directories(KisAnimatedBrushAnnotationTest PRIVATE ..)
target_link_libraries(KisAnimatedBrushAnnotationTest PRIVATE kritastore pkvariant)
add_test(NAME KisAnimatedBrushAnnotationTest COMMAND KisAnimatedBrushAnnotationTest)
