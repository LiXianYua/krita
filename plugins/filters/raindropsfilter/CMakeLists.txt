set(kritaraindropsfilter_SOURCES kis_raindrops_filter_plugin.cpp kis_raindrops_filter.cpp )
kis_add_library(kritaraindropsfilter STATIC ${kritaraindropsfilter_SOURCES})
set_target_properties(kritaraindropsfilter PROPERTIES AUTOMOC OFF)
target_link_libraries(kritaraindropsfilter PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritaraindropsfilter INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritaraindropsfilter kritaimage)
install(TARGETS kritaraindropsfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
