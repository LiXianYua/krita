set(kritatangentnormalpaintop_SOURCES
    kis_tangent_normal_paintop_plugin.cpp
    kis_tangent_normal_paintop.cpp
    KisTangentTiltOption.cpp
    KisTangentTiltOptionData.cpp

)

kis_add_library(kritatangentnormalpaintop MODULE ${kritatangentnormalpaintop_SOURCES})
target_include_directories(kritatangentnormalpaintop PRIVATE ${CMAKE_SOURCE_DIR}/pk/string ${CMAKE_SOURCE_DIR}/pk/container ${CMAKE_SOURCE_DIR}/pk/geometry ${CMAKE_SOURCE_DIR}/pk/pointer ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritatangentnormalpaintop kritalibpaintop)

install(TARGETS kritatangentnormalpaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
########### install files ###############

install( FILES  krita-tangentnormal.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
install( FILES  krita-tangentnormal-preview.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
