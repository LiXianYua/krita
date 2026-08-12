set(kritahairypaintop_SOURCES
    hairy_paintop_plugin.cpp
    kis_hairy_paintop.cpp
    kis_hairy_paintop_settings.cpp
    bristle.cpp
    hairy_brush.cpp
    trajectory.cpp
    KisHairyBristleOptionData.cpp
    KisHairyInkOptionData.cpp
    )

kis_add_library(kritahairypaintop MODULE ${kritahairypaintop_SOURCES})

target_link_libraries(kritahairypaintop kritalibpaintop)

install(TARGETS kritahairypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    krita-sumi.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)


