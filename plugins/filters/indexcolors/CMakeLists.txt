set(INDEXCOLORS_SOURCE indexcolors.cpp indexcolorpalette.cpp palettegeneratorconfig.cpp)
kis_add_library(kritaindexcolors MODULE ${INDEXCOLORS_SOURCE})
target_link_libraries(kritaindexcolors kritaimage)
install(TARGETS kritaindexcolors          DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
