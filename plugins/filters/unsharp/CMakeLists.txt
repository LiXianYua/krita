add_subdirectory( tests )

set(kritaunsharpfilter_SOURCES
    unsharp.cpp
    kis_unsharp_filter.cpp
    )

kis_add_library(kritaunsharpfilter MODULE ${kritaunsharpfilter_SOURCES})

target_link_libraries(kritaunsharpfilter kritaimage)

install(TARGETS kritaunsharpfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
