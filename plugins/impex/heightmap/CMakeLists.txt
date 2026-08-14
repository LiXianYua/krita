add_subdirectory(tests)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

set(kritaheightmapimport_SOURCES
    kis_heightmap_import.cpp
    kis_heightmap_utils.cpp
    )

kis_add_library(kritaheightmapimport MODULE ${kritaheightmapimport_SOURCES})

target_link_libraries(kritaheightmapimport kritaimpex )

install(TARGETS kritaheightmapimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaheightmapexport_SOURCES
    kis_heightmap_export.cpp
    kis_heightmap_utils.cpp
    )

kis_add_library(kritaheightmapexport MODULE ${kritaheightmapexport_SOURCES})

target_link_libraries(kritaheightmapexport kritaimpex)

install(TARGETS kritaheightmapexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_heightmap.desktop DESTINATION ${KDE_INSTALL_APPDIR})
