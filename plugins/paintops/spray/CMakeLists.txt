set(kritaspraypaintop_SOURCES
    spray_paintop_plugin.cpp
    kis_spray_paintop.cpp
    kis_spray_paintop_settings.cpp
    spray_brush.cpp
    KisSprayRandomDistributions.cpp
    KisSprayOpOptionData.cpp
    KisSprayOpOption.cpp

    KisSprayShapeOptionData.cpp

    KisSprayShapeDynamicsOptionData.cpp

    )

kis_add_library(kritaspraypaintop MODULE ${kritaspraypaintop_SOURCES})

target_link_libraries(kritaspraypaintop kritalibpaintop)

install(TARGETS kritaspraypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES
    krita-spray.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)

