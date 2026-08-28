set(kritacolorgenerator_SOURCES
    colorgenerator.cpp
    )

kis_add_library(kritacolorgenerator MODULE ${kritacolorgenerator_SOURCES})
set_target_properties(kritacolorgenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritacolorgenerator PRIVATE kritaimage pkgeometry pkvariant)

install(TARGETS kritacolorgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
