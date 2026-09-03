set(kritaposterize_SOURCES posterize.cpp )
kis_add_library(kritaposterize STATIC ${kritaposterize_SOURCES})
target_link_libraries(kritaposterize kritaimage)
install(TARGETS kritaposterize  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
