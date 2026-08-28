add_subdirectory(tests)

set(kritaqimageioexport_SOURCES
    kis_qimageio_export.cpp
    )

kis_add_library(kritaqimageioexport MODULE ${kritaqimageioexport_SOURCES})

target_link_libraries(kritaqimageioexport kritaimpex)

install(TARGETS kritaqimageioexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaqimageioimport_SOURCES
    kis_qimageio_import.cpp
    )


kis_add_library(kritaqimageioimport MODULE ${kritaqimageioimport_SOURCES})

target_link_libraries(kritaqimageioimport kritaimpex)

install(TARGETS kritaqimageioimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_qimageio.desktop DESTINATION ${KDE_INSTALL_APPDIR})
