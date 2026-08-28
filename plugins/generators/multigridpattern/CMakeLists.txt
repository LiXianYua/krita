set(kritamultigridpatterngenerator_SOURCES
    multigridpatterngenerator.cpp
    multigridpatternhelpers.cpp
    )

kis_add_library(kritamultigridpatterngenerator MODULE ${kritamultigridpatterngenerator_SOURCES})
set_target_properties(kritamultigridpatterngenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritamultigridpatterngenerator PRIVATE
    kritaimage
    pkcontainer pkgeometry pkstring pkxml pkvariant pkcolor
)

install(TARGETS kritamultigridpatterngenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
