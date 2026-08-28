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

kis_add_library(krita_shape_paths MODULE ${pathshapes_SOURCES})
set_target_properties(krita_shape_paths PROPERTIES AUTOMOC OFF)

target_include_directories(krita_shape_paths PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/xml
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/global
)

target_link_libraries(krita_shape_paths
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

install(TARGETS krita_shape_paths  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
