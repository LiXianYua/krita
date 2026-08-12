set(kritapatterngenerator_SOURCES
    patterngenerator.cpp
    )

kis_add_library(kritapatterngenerator MODULE ${kritapatterngenerator_SOURCES})

target_link_libraries(kritapatterngenerator kritaimage)

install(TARGETS kritapatterngenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
