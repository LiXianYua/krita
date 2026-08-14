set(kritatoolSmartPatch_SOURCES
    tool_smartpatch.cpp
    kis_tool_smart_patch.cpp
    kis_inpaint.cpp
    )

kis_add_library(kritatoolSmartPatch MODULE ${kritatoolSmartPatch_SOURCES})

generate_export_header(kritatoolSmartPatch BASE_NAME kritatoolSmartPatch)

target_link_libraries(kritatoolSmartPatch kritacanvas kritashapemodel)

install(TARGETS kritatoolSmartPatch  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
