set(kritaextensioncolorsfilters_SOURCES
    colors.cpp
    kis_color_to_alpha.cpp
    kis_minmax_filters.cpp
    KisFilterFastColorOverlay.cpp
    )

kis_add_library(kritaextensioncolorsfilters STATIC ${kritaextensioncolorsfilters_SOURCES})
set_target_properties(kritaextensioncolorsfilters PROPERTIES AUTOMOC OFF)
target_link_libraries(kritaextensioncolorsfilters PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritaextensioncolorsfilters INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritaextensioncolorsfilters kritaimage)
install(TARGETS kritaextensioncolorsfilters  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
