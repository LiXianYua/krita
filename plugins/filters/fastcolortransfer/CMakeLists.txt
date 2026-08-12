set(kritafastcolortransferfilter_SOURCES
    fastcolortransfer.cpp
)
kis_add_library(kritafastcolortransferfilter MODULE ${kritafastcolortransferfilter_SOURCES})
target_link_libraries(kritafastcolortransferfilter kritaimage)
install(TARGETS kritafastcolortransferfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
