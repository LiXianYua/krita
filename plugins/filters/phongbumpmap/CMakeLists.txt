set(kritaphongbumpmap_SOURCES
    kis_phong_bumpmap_plugin.cpp
    kis_phong_bumpmap_filter.cpp
    phong_pixel_processor.cpp
    )

kis_add_library(kritaphongbumpmap MODULE ${kritaphongbumpmap_SOURCES})
target_link_libraries(kritaphongbumpmap kritaimage)
install(TARGETS kritaphongbumpmap  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
