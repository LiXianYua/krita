set(kritapropagatecolors_SOURCES
    KisPropagateColorsFilter.cpp
    KisPropagateColorsFilterConfiguration.cpp
    KisPropagateColorsFilterPlugin.cpp
)

kis_add_library(kritapropagatecolors STATIC ${kritapropagatecolors_SOURCES})

target_link_libraries(kritapropagatecolors kritaimage)

install(
    TARGETS
    kritapropagatecolors
    DESTINATION 
    ${KRITA_PLUGIN_INSTALL_DIR}
)

install(
    FILES
    propagatecolors.action
    DESTINATION
    ${KDE_INSTALL_DATADIR}/krita/actions
)
