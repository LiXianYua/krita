set(kritasimplexnoisegenerator_SOURCES
    simplexnoisegenerator.cpp
    simplexnoiseseed.cpp
    3rdparty/c-open-simplex/open-simplex-noise.c
    )

kis_add_library(kritasimplexnoisegenerator MODULE ${kritasimplexnoisegenerator_SOURCES})
set_target_properties(kritasimplexnoisegenerator PROPERTIES AUTOMOC OFF)
target_include_directories(kritasimplexnoisegenerator PRIVATE ${CMAKE_SOURCE_DIR}/libs/resources)
target_link_libraries(kritasimplexnoisegenerator PRIVATE
    kritaimage kritaresources
    pkstring pkgeometry pkvariant
)
install(TARGETS kritasimplexnoisegenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
