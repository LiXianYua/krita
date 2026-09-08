set(kritanoisefilter_SOURCES
    noisefilter.cpp
    )

kis_add_library(kritanoisefilter STATIC ${kritanoisefilter_SOURCES})
set_target_properties(kritanoisefilter PROPERTIES AUTOMOC OFF)
target_link_libraries(kritanoisefilter PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritanoisefilter INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritanoisefilter kritaimage)
install(TARGETS kritanoisefilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
