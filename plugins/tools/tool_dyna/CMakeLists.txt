set(kritatooldyna_SOURCES
    tool_dyna.cpp
    kis_tool_dyna.cpp
    )


kis_add_library(kritatooldyna STATIC ${kritatooldyna_SOURCES})

set_target_properties(kritatooldyna PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatooldyna PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/flake/flake/noqt-compat
    ${CMAKE_SOURCE_DIR}/pk/global/compat
    ${CMAKE_SOURCE_DIR}/pk/signal/compat
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/string)

target_link_libraries(kritatooldyna kritacanvas kritashapemodel pkconfig pkcontainer pkgeometry pknamespace pkstring)

install(TARGETS kritatooldyna  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
