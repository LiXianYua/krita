project( defaulttools )

set ( defaulttools_SRCS
    Plugin.cpp
    defaulttool/DefaultTool.cpp
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

qt_add_resources(defaulttools_SRCS defaulttools.qrc)

kis_add_library(krita_flaketools MODULE ${defaulttools_SRCS})
target_link_libraries(krita_flaketools kritaflake kritawidgets kritacanvas kritaimpex)
install(TARGETS krita_flaketools DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
