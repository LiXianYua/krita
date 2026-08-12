set(kritablurfilter_SOURCES
    blur.cpp
    kis_blur_filter.cpp
    kis_gaussian_blur_filter.cpp
    kis_motion_blur_filter.cpp
    kis_lens_blur_filter.cpp
    )

kis_add_library(kritablurfilter MODULE ${kritablurfilter_SOURCES})
target_link_libraries(kritablurfilter kritaimage)
install(TARGETS kritablurfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
