set(kritagradientmap_SOURCES
    KisGradientMapFilter.cpp
    KisGradientMapFilterConfiguration.cpp
    KisGradientMapFilterPlugin.cpp
    KisGradientMapFilterNearestCachedGradient.cpp
    KisGradientMapFilterDitherCachedGradient.cpp
)

kis_add_library(kritagradientmap MODULE ${kritagradientmap_SOURCES})
target_link_libraries(kritagradientmap kritaui)
install(TARGETS kritagradientmap  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    gradientmap.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
