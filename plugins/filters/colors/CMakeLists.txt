set(kritaextensioncolorsfilters_SOURCES
    colors.cpp
    kis_color_to_alpha.cpp
    kis_minmax_filters.cpp
    KisFilterFastColorOverlay.cpp
    )

kis_add_library(kritaextensioncolorsfilters MODULE ${kritaextensioncolorsfilters_SOURCES})
target_link_libraries(kritaextensioncolorsfilters kritaimage)
install(TARGETS kritaextensioncolorsfilters  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
