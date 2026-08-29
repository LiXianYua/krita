set(kritatoolpolygon_SOURCES
    tool_polygon.cc
    kis_tool_polygon.cc
    )

qt_add_resources(kritatoolpolygon_SOURCES tool_polygon.qrc)
kis_add_library(kritatoolpolygon MODULE ${kritatoolpolygon_SOURCES})

set_target_properties(kritatoolpolygon PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolpolygon PRIVATE ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry)

target_link_libraries(kritatoolpolygon kritacanvas kritashapemodel pkcontainer pkgeometry)

install(TARGETS kritatoolpolygon  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
    KisToolPolygon.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
