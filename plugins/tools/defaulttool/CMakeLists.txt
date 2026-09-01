project( defaulttools )

set ( defaulttools_SRCS
    Plugin.cpp
    defaulttool/DefaultTool.cpp
    defaulttool/DefaultToolPlatform.cpp
    defaulttool/DefaultToolFactory.cpp
    defaulttool/ShapeMoveStrategy.cpp
    defaulttool/ShapeResizeStrategy.cpp
    defaulttool/ShapeRotateStrategy.cpp
    defaulttool/ShapeShearStrategy.cpp
    defaulttool/ShapeGradientEditStrategy.cpp
    defaulttool/SelectionDecorator.cpp
    defaulttool/ShapeMeshGradientEditStrategy.cpp

    defaulttool/KoShapeGradientHandles.cpp
    defaulttool/KoShapeMeshGradientHandles.cpp

    referenceimagestool/ToolReferenceImages.cpp
    referenceimagestool/KisReferenceImageCollection.cpp
)

kis_add_library(krita_flaketools MODULE ${defaulttools_SRCS})

set_target_properties(krita_flaketools PROPERTIES AUTOMOC OFF)
target_include_directories(krita_flaketools PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/pigment
    ${CMAKE_SOURCE_DIR}/libs/store
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/port
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/time
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/xml)

target_link_libraries(krita_flaketools
    kritaflake
    kritacanvas
    kritapigment
    kritastore
    pkcolor
    pkconcurrent
    pkcontainer
    pkgeometry
    pkglobal
    pkimage
    pknamespace
    pkpointer
    pkport
    pkrender
    pksignal
    pkstring
    pktime
    pkvariant
    pkxml)
install(TARGETS krita_flaketools DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
