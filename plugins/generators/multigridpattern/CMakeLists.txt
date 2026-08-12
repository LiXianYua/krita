set(kritamultigridpatterngenerator_SOURCES
    multigridpatterngenerator.cpp
    )

kis_add_library(kritamultigridpatterngenerator MODULE ${kritamultigridpatterngenerator_SOURCES})

target_link_libraries(kritamultigridpatterngenerator kritaimage)

install(TARGETS kritamultigridpatterngenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
