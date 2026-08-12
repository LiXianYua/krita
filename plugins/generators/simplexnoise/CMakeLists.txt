set(kritasimplexnoisegenerator_SOURCES
    simplexnoisegenerator.cpp
    3rdparty/c-open-simplex/open-simplex-noise.c
    )

kis_add_library(kritasimplexnoisegenerator MODULE ${kritasimplexnoisegenerator_SOURCES})
target_link_libraries(kritasimplexnoisegenerator kritaimage)
install(TARGETS kritasimplexnoisegenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
