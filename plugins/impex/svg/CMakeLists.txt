add_subdirectory(tests)

set(kritasvgimport_SOURCES
    kis_svg_import.cc
    svg_import_policy.cpp
    )

kis_add_library(kritasvgimport MODULE ${kritasvgimport_SOURCES})

add_definitions(${SVG_DEFINITIONS})

target_link_libraries(kritasvgimport kritaimpex)

install(TARGETS kritasvgimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_svg.desktop DESTINATION ${KDE_INSTALL_APPDIR})
