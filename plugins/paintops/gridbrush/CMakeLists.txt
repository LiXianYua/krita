set(kritagridpaintop_SOURCES
    grid_paintop_plugin.cpp
    kis_grid_paintop.cpp
    KisGridOpOptionData.cpp

    kis_grid_paintop_settings.cpp
    KisGridShapeOptionData.cpp
    )

kis_add_library(kritagridpaintop MODULE ${kritagridpaintop_SOURCES})

target_link_libraries(kritagridpaintop kritalibpaintop)

install(TARGETS kritagridpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    krita-grid.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)


