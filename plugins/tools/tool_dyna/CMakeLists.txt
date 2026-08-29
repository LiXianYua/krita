set(kritatooldyna_SOURCES
    tool_dyna.cpp
    kis_tool_dyna.cpp
    )


kis_add_library(kritatooldyna MODULE ${kritatooldyna_SOURCES})

set_target_properties(kritatooldyna PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatooldyna PRIVATE ${CMAKE_SOURCE_DIR}/pk/geometry)

target_link_libraries(kritatooldyna kritacanvas kritashapemodel pkgeometry)

install(TARGETS kritatooldyna  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
