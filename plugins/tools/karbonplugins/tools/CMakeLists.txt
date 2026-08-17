set(karbon_tools_SOURCES
   KarbonToolsPlugin.cpp
   CalligraphyTool/KarbonCalligraphyTool.cpp
   CalligraphyTool/KarbonCalligraphyToolFactory.cpp
   CalligraphyTool/KarbonCalligraphicShape.cpp
   CalligraphyTool/KarbonCalligraphicShapeFactory.cpp
   CalligraphyTool/KarbonSimplifyPath.cpp
)

qt_add_resources(karbon_tools_SOURCES karbontools.qrc)

kis_add_library(krita_karbontools MODULE ${karbon_tools_SOURCES})

target_link_libraries(krita_karbontools kritacanvas)

install(TARGETS krita_karbontools DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
