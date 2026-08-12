set(kritasketchpaintop_SOURCES
    sketch_paintop_plugin.cpp
    kis_sketch_paintop.cpp
    kis_sketch_paintop_settings.cpp
    KisSketchOpOptionData.cpp
    )

kis_add_library(kritasketchpaintop MODULE ${kritasketchpaintop_SOURCES})

target_link_libraries(kritasketchpaintop kritalibpaintop)

install(TARGETS kritasketchpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES  krita-sketch.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)


