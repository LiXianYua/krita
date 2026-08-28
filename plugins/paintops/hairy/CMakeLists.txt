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
target_include_directories(kritahairypaintop PRIVATE ${CMAKE_SOURCE_DIR}/pk/string ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry ${CMAKE_SOURCE_DIR}/pk/image ${CMAKE_SOURCE_DIR}/pk/pointer ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritahairypaintop kritalibpaintop)

install(TARGETS kritahairypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    krita-sumi.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)

