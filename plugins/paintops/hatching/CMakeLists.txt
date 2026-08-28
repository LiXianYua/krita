set(kritahatchingpaintop_SOURCES
    hatching_paintop_plugin.cpp
    kis_hatching_paintop.cpp
    kis_hatching_paintop_settings.cpp
    hatching_brush.cpp
    KisHatchingOptionsData.cpp
    KisHatchingPreferencesData.cpp
    )

kis_add_library(kritahatchingpaintop MODULE ${kritahatchingpaintop_SOURCES})
target_include_directories(kritahatchingpaintop PRIVATE ${CMAKE_SOURCE_DIR}/pk/string ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry ${CMAKE_SOURCE_DIR}/pk/pointer ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritahatchingpaintop kritalibpaintop)

install(TARGETS kritahatchingpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES  krita-hatching.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
