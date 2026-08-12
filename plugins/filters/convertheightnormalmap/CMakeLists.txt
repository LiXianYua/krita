set(kritaconvertheighttonormalmap_SOURCES
    kis_convert_height_to_normal_map_filter.cpp
)

kis_add_library(kritaconvertheighttonormalmap MODULE ${kritaconvertheighttonormalmap_SOURCES})
target_link_libraries(kritaconvertheighttonormalmap kritaimage)
install(TARGETS kritaconvertheighttonormalmap  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    convertheightnormalmap.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
