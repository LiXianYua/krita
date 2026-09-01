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

set_target_properties(krita_karbontools PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(krita_karbontools PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/xml)

target_link_libraries(krita_karbontools
    kritacanvas pkcolor pkcontainer pkgeometry pkpointer pkrender pkstring pkxml)

install(TARGETS krita_karbontools DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
