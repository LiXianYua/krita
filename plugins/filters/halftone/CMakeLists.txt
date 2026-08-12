set(kritahalftone_SOURCES
    KisHalftoneFilter.cpp
    KisHalftoneFilterConfiguration.cpp
)

kis_add_library(kritahalftone MODULE ${kritahalftone_SOURCES})
target_link_libraries(kritahalftone kritaimage)
install(TARGETS kritahalftone DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    halftone.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
