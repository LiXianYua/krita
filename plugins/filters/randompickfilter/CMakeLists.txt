set(kritarandompickfilter_SOURCES
    randompickfilter.cpp
    )

kis_add_library(kritarandompickfilter MODULE ${kritarandompickfilter_SOURCES})

target_link_libraries(kritarandompickfilter kritaimage)

install(TARGETS kritarandompickfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
