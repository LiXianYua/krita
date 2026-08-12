set(kritathreshold_SOURCES threshold.cpp )

kis_add_library(kritathreshold MODULE ${kritathreshold_SOURCES})
target_link_libraries(kritathreshold kritaimage)
install(TARGETS kritathreshold  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    threshold.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)

