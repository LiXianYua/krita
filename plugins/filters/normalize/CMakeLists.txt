set(kritanormalize_SOURCES
    kis_normalize.cpp
)
kis_add_library(kritanormalize STATIC ${kritanormalize_SOURCES})
target_link_libraries(kritanormalize kritaimage)
install(TARGETS kritanormalize  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    normalize.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
