add_subdirectory(tests)

set(kritawebpexport_SOURCES
    kis_webp_export.cpp
)

add_library(kritawebpexport MODULE ${kritawebpexport_SOURCES})
target_link_libraries(kritawebpexport kritaimpex WebP::webp WebP::libwebpmux)

install(TARGETS kritawebpexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


set(kritawebpimport_SOURCES
    kis_webp_import.cpp
    webp_validation.cpp
)

add_library(kritawebpimport MODULE ${kritawebpimport_SOURCES})
target_link_libraries(kritawebpimport kritaimpex WebP::webp WebP::webpdemux)

install(TARGETS kritawebpimport DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_webp.desktop DESTINATION ${KDE_INSTALL_APPDIR})
