set(kritagradientgenerator_SOURCES
    KisGradientGeneratorPlugin.cpp
    KisGradientGenerator.cpp
    KisGradientGeneratorConfiguration.cpp
    )

kis_add_library(kritagradientgenerator MODULE ${kritagradientgenerator_SOURCES})
set_target_properties(kritagradientgenerator PROPERTIES AUTOMOC OFF)
target_link_libraries(kritagradientgenerator PRIVATE
    kritaimage
    pkstring pkxml pkcolor pkgeometry pkvariant
)
install(TARGETS kritagradientgenerator DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
