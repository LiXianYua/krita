set(kritagradientmap_SOURCES
    KisGradientMapFilter.cpp
    KisGradientMapFilterConfiguration.cpp
    KisGradientMapFilterPlugin.cpp
    KisGradientMapFilterNearestCachedGradient.cpp
    KisGradientMapFilterDitherCachedGradient.cpp
)

kis_add_library(kritafilterdithering STATIC
    KisDitherUtil.cpp
    KisDitherConfigurationHelper.cpp
)
set_target_properties(kritafilterdithering PROPERTIES AUTOMOC OFF)
target_include_directories(kritafilterdithering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kritafilterdithering PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritafilterdithering INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritafilterdithering kritaimage)

kis_add_library(kritagradientmap STATIC ${kritagradientmap_SOURCES})
target_link_libraries(kritagradientmap kritafilterdithering kritaimage)
install(TARGETS kritagradientmap  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    gradientmap.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
