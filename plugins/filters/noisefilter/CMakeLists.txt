set(kritanoisefilter_SOURCES
    noisefilter.cpp
    )

kis_add_library(kritanoisefilter STATIC ${kritanoisefilter_SOURCES})
target_compile_definitions(kritanoisefilter PRIVATE QT_NO_VERSION_TAGGING)
target_link_libraries(kritanoisefilter kritaimage)
install(TARGETS kritanoisefilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
