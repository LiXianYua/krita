set(kritarandompickfilter_SOURCES
    randompickfilter.cpp
    )

kis_add_library(kritarandompickfilter STATIC ${kritarandompickfilter_SOURCES})
set_target_properties(kritarandompickfilter PROPERTIES AUTOMOC OFF)

target_link_libraries(kritarandompickfilter PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritarandompickfilter INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritarandompickfilter kritaimage)

install(TARGETS kritarandompickfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
