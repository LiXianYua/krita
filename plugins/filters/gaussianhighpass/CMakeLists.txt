set(kritahighpassfilter_SOURCES
    gaussianhighpass.cpp
    gaussianhighpass_filter.cpp
    )

kis_add_library(kritagaussianhighpassfilter MODULE ${kritahighpassfilter_SOURCES})

target_link_libraries(kritagaussianhighpassfilter kritaimage)

install(TARGETS kritagaussianhighpassfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    gaussianhighpass.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
