set(kritadodgeburn_SOURCES DodgeBurnPlugin.cpp DodgeBurn.cpp )
kis_add_library(kritadodgeburn STATIC ${kritadodgeburn_SOURCES})
target_link_libraries(kritadodgeburn kritaimage)
install(TARGETS kritadodgeburn  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
