add_subdirectory(tests)

set(kritagifexport_SOURCES
    kis_gif_export.cpp
    qgiflibhandler.cpp
    )

kis_add_library(kritagifexport MODULE ${kritagifexport_SOURCES})

target_link_libraries(kritagifexport kritaimpex ${GIF_LIBRARY})

install(TARGETS kritagifexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritagifimport_SOURCES
    kis_gif_import.cpp
    qgiflibhandler.cpp
    )

kis_add_library(kritagifimport MODULE ${kritagifimport_SOURCES})

target_link_libraries(kritagifimport kritaimpex ${GIF_LIBRARY})

install(TARGETS kritagifimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_gif.desktop DESTINATION ${KDE_INSTALL_APPDIR})
