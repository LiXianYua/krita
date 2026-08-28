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
target_include_directories(kritaspraypaintop PRIVATE ${CMAKE_SOURCE_DIR}/pk/string ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry ${CMAKE_SOURCE_DIR}/pk/image ${CMAKE_SOURCE_DIR}/pk/pointer ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritaspraypaintop kritalibpaintop)

install(TARGETS kritaspraypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES
    krita-spray.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
