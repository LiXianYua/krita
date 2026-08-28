set(path_shapes_core_test_SRCS
    PathShapesCoreTest.cpp
    ../PathShapesPlugin.cpp
    ../ellipse/EllipseShape.cpp
    ../ellipse/EllipseShapeFactory.cpp
    ../rectangle/RectangleShape.cpp
    ../rectangle/RectangleShapeFactory.cpp
    ../spiral/SpiralShape.cpp
    ../spiral/SpiralShapeFactory.cpp
    ../star/StarShape.cpp
    ../star/StarShapeFactory.cpp
)

kis_add_test(${path_shapes_core_test_SRCS}
    TEST_NAME PathShapesCoreTest
    LINK_LIBRARIES
        kritaflake
        pkstring
        pkcontainer
        pkvariant
        pkgeometry
        pkcolor
        pkpointer
        pkxml
        pknamespace
        pkglobal
)
set_target_properties(PathShapesCoreTest PROPERTIES AUTOMOC OFF)
