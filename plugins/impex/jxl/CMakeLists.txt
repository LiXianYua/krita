add_subdirectory(tests)

set(kritajxlimport_SOURCES
    JPEGXLImport.cpp
)

kis_add_library(kritajxlimport MODULE ${kritajxlimport_SOURCES})

target_link_libraries(kritajxlimport kritaimpex kritalibkra kritametadata ${JPEGXL_LIBRARIES})

install(TARGETS kritajxlimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritajxlexport_SOURCES
    JPEGXLExport.cpp
    kis_jpegxl_export_tools.h
)

kis_add_library(kritajxlexport MODULE ${kritajxlexport_SOURCES})

target_link_libraries(kritajxlexport kritalibkra kritaimpex ${JPEGXL_LIBRARIES})

install(TARGETS kritajxlexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_jxl.desktop DESTINATION ${KDE_INSTALL_APPDIR})
