set(kritascreentonegenerator_SOURCES
    KisScreentoneGenerator.cpp
    KisScreentoneScreentoneFunctions.cpp
    KisScreentoneBrightnessContrastFunctions.cpp
    KisScreentoneGeneratorPlugin.cpp
    KisScreentoneGeneratorConfiguration.cpp
    KisScreentoneGeneratorTemplate.cpp
    )

kis_add_library(kritascreentonegenerator MODULE ${kritascreentonegenerator_SOURCES})
target_link_libraries(kritascreentonegenerator kritaimage)
install(TARGETS kritascreentonegenerator DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
