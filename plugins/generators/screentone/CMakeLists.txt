set(kritascreentonegenerator_SOURCES
    KisScreentoneGenerator.cpp
    KisScreentoneScreentoneFunctions.cpp
    KisScreentoneBrightnessContrastFunctions.cpp
    KisScreentoneGeneratorPlugin.cpp
    KisScreentoneGeneratorConfiguration.cpp
    KisScreentoneGeneratorTemplate.cpp
    )

kis_add_library(kritascreentonegenerator MODULE ${kritascreentonegenerator_SOURCES})
set_target_properties(kritascreentonegenerator PROPERTIES AUTOMOC OFF)
target_link_libraries(kritascreentonegenerator PRIVATE
    kritaimage
    pkconcurrent pkstring pkpointer pkcolor pkgeometry pkcontainer pkvariant
)
install(TARGETS kritascreentonegenerator DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
