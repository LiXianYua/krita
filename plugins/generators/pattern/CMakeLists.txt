set(kritapatterngenerator_SOURCES
    patterngenerator.cpp
    )

kis_add_library(kritapatterngenerator MODULE ${kritapatterngenerator_SOURCES})
set_target_properties(kritapatterngenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritapatterngenerator PRIVATE
    kritaimage
    pkcontainer pkgeometry pkvariant
)

install(TARGETS kritapatterngenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
