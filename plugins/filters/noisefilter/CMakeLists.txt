set(kritanoisefilter_SOURCES
    noisefilter.cpp
    )

kis_add_library(kritanoisefilter MODULE ${kritanoisefilter_SOURCES})
target_link_libraries(kritanoisefilter kritaimage)
install(TARGETS kritanoisefilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
