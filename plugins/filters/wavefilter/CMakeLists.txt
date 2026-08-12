set(kritawavefilter_SOURCES
    wavefilter.cpp
    )

kis_add_library(kritawavefilter MODULE ${kritawavefilter_SOURCES})

target_link_libraries(kritawavefilter kritaimage)

install(TARGETS kritawavefilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

