set(imageshape_cow_test_SRCS
    ImageShapeCowTest.cpp
    ../ImageShape.cpp
    ../ImageShapePngData.cpp
    ../ImageShapeFactory.cpp
    ../ImageShapePlugin.cpp
)

kis_add_test(${imageshape_cow_test_SRCS}
    TEST_NAME ImageShapeCowTest
    LINK_LIBRARIES kritaflake kritaresources pkstring pkcontainer pkgeometry pkimage pkvariant pkxml PNG::PNG
)
set_target_properties(ImageShapeCowTest PROPERTIES AUTOMOC OFF)

kis_add_test(ImageShapeSerializationTest.cpp ../ImageShapePngData.cpp
    TEST_NAME ImageShapeSerializationTest
    LINK_LIBRARIES pkstring pkcontainer pkgeometry pkimage pkvariant PNG::PNG
)
set_target_properties(ImageShapeSerializationTest PROPERTIES AUTOMOC OFF)
