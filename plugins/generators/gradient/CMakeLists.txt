set(kritagradientgenerator_SOURCES
    KisGradientGeneratorPlugin.cpp
    KisGradientGenerator.cpp
    KisGradientGeneratorConfiguration.cpp
    )

kis_add_library(kritagradientgenerator MODULE ${kritagradientgenerator_SOURCES})
target_link_libraries(kritagradientgenerator kritaimage)
install(TARGETS kritagradientgenerator DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
