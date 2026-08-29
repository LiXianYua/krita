set(kritatoolpolyline_SOURCES
    tool_polyline.cc
    kis_tool_polyline.cc
    )

qt_add_resources(kritatoolpolyline_SOURCES polyline.qrc)

kis_add_library(kritatoolpolyline MODULE ${kritatoolpolyline_SOURCES})

set_target_properties(kritatoolpolyline PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolpolyline PRIVATE ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry)

target_link_libraries(kritatoolpolyline kritacanvas kritashapemodel pkcontainer pkgeometry)

install(TARGETS kritatoolpolyline  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
    KisToolPolyline.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
