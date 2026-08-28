set(imageshape_cow_test_SRCS
    ImageShapeCowTest.cpp
    ../ImageShape.cpp
    ../ImageShapeState.cpp
    ../ImageShapePngData.cpp
    ../ImageShapeFactory.cpp
    ../ImageShapePlugin.cpp
)

kis_add_test(${imageshape_cow_test_SRCS}
    TEST_NAME ImageShapeCowTest
    LINK_LIBRARIES pkstring pkcontainer pkgeometry pkimage pkvariant pkxml PNG::PNG JPEG::JPEG TIFF::TIFF
)
set_target_properties(ImageShapeCowTest PROPERTIES AUTOMOC OFF)
target_include_directories(ImageShapeCowTest BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/namespace
)

kis_add_test(ImageShapeSerializationTest.cpp ../ImageShapePngData.cpp
    TEST_NAME ImageShapeSerializationTest
    LINK_LIBRARIES pkstring pkcontainer pkgeometry pkimage pkvariant PNG::PNG JPEG::JPEG TIFF::TIFF
)
set_target_properties(ImageShapeSerializationTest PROPERTIES AUTOMOC OFF)
