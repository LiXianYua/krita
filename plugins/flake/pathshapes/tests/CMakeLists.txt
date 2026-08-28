set(path_shapes_core_test_SRCS
    PathShapesCoreTest.cpp
    ../PathShapesPlugin.cpp
    ../ellipse/EllipseShape.cpp
    ../ellipse/EllipseShapeConfigCommand.cpp
    ../ellipse/EllipseShapeFactory.cpp
    ../rectangle/RectangleShape.cpp
    ../rectangle/RectangleShapeConfigCommand.cpp
    ../rectangle/RectangleShapeFactory.cpp
    ../spiral/SpiralShape.cpp
    ../spiral/SpiralShapeConfigCommand.cpp
    ../spiral/SpiralShapeFactory.cpp
    ../star/StarShape.cpp
    ../star/StarShapeConfigCommand.cpp
    ../star/StarShapeFactory.cpp
)

kis_add_test(${path_shapes_core_test_SRCS}
    TEST_NAME PathShapesCoreTest
    LINK_LIBRARIES
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
target_include_directories(PathShapesCoreTest BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/flake/imageshape/tests/stubs
    ${CMAKE_SOURCE_DIR}/plugins/flake/pathshapes
    ${CMAKE_SOURCE_DIR}/pk/image
)
