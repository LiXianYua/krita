kis_add_test(
    KisBrushTest.cpp
    TEST_NAME KisBrushTest
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(KisAnimatedBrushAnnotationTest
    KisAnimatedBrushAnnotationTest.cpp
    ../animated_brush_annotation_data.cpp
)
target_include_directories(KisAnimatedBrushAnnotationTest PRIVATE ..)
target_link_libraries(KisAnimatedBrushAnnotationTest PRIVATE kritastore pkvariant)
add_test(NAME KisAnimatedBrushAnnotationTest COMMAND KisAnimatedBrushAnnotationTest)
