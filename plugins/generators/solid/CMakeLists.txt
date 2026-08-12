set(kritacolorgenerator_SOURCES
    colorgenerator.cpp
    )

kis_add_library(kritacolorgenerator MODULE ${kritacolorgenerator_SOURCES})

target_link_libraries(kritacolorgenerator kritaimage)

install(TARGETS kritacolorgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
