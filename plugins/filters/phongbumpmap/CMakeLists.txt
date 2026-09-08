set(kritaphongbumpmap_SOURCES
    kis_phong_bumpmap_plugin.cpp
    kis_phong_bumpmap_filter.cpp
    phong_pixel_processor.cpp
    )

kis_add_library(kritaphongbumpmap STATIC ${kritaphongbumpmap_SOURCES})
set_target_properties(kritaphongbumpmap PROPERTIES AUTOMOC OFF)
target_link_libraries(kritaphongbumpmap PUBLIC
    kritaimage_noqt_compile_interface)
target_link_libraries(kritaphongbumpmap INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritaphongbumpmap kritaimage)
install(TARGETS kritaphongbumpmap  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
