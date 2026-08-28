# export

add_subdirectory(tests)

set(kritaspriterexport_SOURCES
    kis_spriter_export.cpp
    spriter_format.cpp
)

kis_add_library(kritaspriterexport MODULE ${kritaspriterexport_SOURCES})

target_link_libraries(kritaspriterexport kritaimpex pkxml)

install(TARGETS kritaspriterexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_spriter.desktop DESTINATION ${KDE_INSTALL_APPDIR})
