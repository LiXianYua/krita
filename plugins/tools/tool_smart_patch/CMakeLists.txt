set(kritatoolSmartPatch_SOURCES
    tool_smartpatch.cpp
    kis_tool_smart_patch.cpp
    kis_inpaint.cpp
    )

kis_add_library(kritatoolSmartPatch MODULE ${kritatoolSmartPatch_SOURCES})

set_target_properties(kritatoolSmartPatch PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolSmartPatch PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render)

generate_export_header(kritatoolSmartPatch BASE_NAME kritatoolSmartPatch)

target_link_libraries(kritatoolSmartPatch
    kritacanvas kritashapemodel pkcontainer pkgeometry pkpointer pkrender)

install(TARGETS kritatoolSmartPatch  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
