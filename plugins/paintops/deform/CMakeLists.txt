set(kritadeformpaintop_SOURCES
    deform_brush.cpp
    deform_paintop_plugin.cpp
    kis_deform_paintop.cpp
    kis_deform_paintop_settings.cpp
    KisBrushSizeOptionData.cpp
    KisDeformOptionData.cpp
    )

kis_add_library(kritadeformpaintop MODULE ${kritadeformpaintop_SOURCES})

target_link_libraries(kritadeformpaintop kritalibpaintop)

install(TARGETS kritadeformpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES  krita-deform.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images )
