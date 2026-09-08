set(kritaembossfilter_SOURCES kis_emboss_filter_plugin.cpp kis_emboss_filter.cpp )
kis_add_library(kritaembossfilter STATIC ${kritaembossfilter_SOURCES})
set_target_properties(kritaembossfilter PROPERTIES AUTOMOC OFF)
target_link_libraries(kritaembossfilter PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritaembossfilter INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritaembossfilter kritaimage)
install(TARGETS kritaembossfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
