add_subdirectory(tests)

set(kritatgaexport_SOURCES
    kis_tga_export.cpp
    tga_validation.cpp
    )

kis_add_library(kritatgaexport STATIC ${kritatgaexport_SOURCES})

target_link_libraries(kritatgaexport kritaimpex)

install(TARGETS kritatgaexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritatgaimport_SOURCES
    kis_tga_import.cpp
    tga_validation.cpp
    )

kis_add_library(kritatgaimport STATIC ${kritatgaimport_SOURCES})

target_link_libraries(kritatgaimport kritaimpex)

install(TARGETS kritatgaimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_tga.desktop DESTINATION ${KDE_INSTALL_APPDIR})
