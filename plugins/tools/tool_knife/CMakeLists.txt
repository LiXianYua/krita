add_subdirectory(tests)

set(kritatoolKnife_static_SOURCES
    KisToolKnife.cpp
    CutThroughShapeStrategy.cpp
    RemoveGutterStrategy.cpp
    GutterWidthsConfig.cpp
    )


set(kritatoolKnife_SOURCES
    ToolKnife.cpp
    )


kis_add_library(kritatoolKnife_static STATIC ${kritatoolKnife_static_SOURCES})
generate_export_header(kritatoolKnife_static BASE_NAME kritatoolKnife)


kis_add_library(kritatoolKnife MODULE ${kritatoolKnife_SOURCES})

set_target_properties(kritatoolKnife_static kritatoolKnife PROPERTIES
    AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolKnife_static PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant)
target_include_directories(kritatoolKnife PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritatoolKnife_static PUBLIC
    kritacanvas kritashapemodel pkcontainer pkgeometry pkpointer pkrender pkstring pkvariant)
target_link_libraries(kritatoolKnife PRIVATE kritatoolKnife_static)

install(TARGETS kritatoolKnife  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
