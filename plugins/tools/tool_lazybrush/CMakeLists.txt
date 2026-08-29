set(kritatoollazybrush_SOURCES
    tool_lazybrush.cpp
    kis_tool_lazy_brush.cpp
    )

kis_add_library(kritatoollazybrush MODULE ${kritatoollazybrush_SOURCES})

set_target_properties(kritatoollazybrush PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)

generate_export_header(kritatoollazybrush BASE_NAME kritatoollazybrush)

target_include_directories(kritatoollazybrush PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/namespace)
target_link_libraries(kritatoollazybrush kritacanvas kritashapemodel pkcontainer pkpointer pkvariant pknamespace)

install(TARGETS kritatoollazybrush  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
