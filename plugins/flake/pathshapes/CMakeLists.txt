set(pathshapes_SOURCES
    ellipse/EllipseShape.cpp
    ellipse/EllipseShapeFactory.cpp
    ellipse/EllipseShapeConfigCommand.cpp
    spiral/SpiralShape.cpp
    spiral/SpiralShapeFactory.cpp
    spiral/SpiralShapeConfigCommand.cpp
    star/StarShape.cpp
    star/StarShapeFactory.cpp
    star/StarShapeConfigCommand.cpp
    rectangle/RectangleShape.cpp
    rectangle/RectangleShapeFactory.cpp
    rectangle/RectangleShapeConfigCommand.cpp
    PathShapesPlugin.cpp
)

qt_add_resources(pathshapes_SOURCES pathshapes.qrc)

kis_add_library(krita_shape_paths MODULE ${pathshapes_SOURCES})

target_link_libraries(krita_shape_paths kritaflake)

install(TARGETS krita_shape_paths  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
