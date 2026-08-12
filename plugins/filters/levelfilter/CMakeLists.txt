set(kritalevelfilter_SOURCES
    KisLevelsFilterPlugin.cpp
    KisLevelsFilter.cpp
    KisLevelsFilterConfiguration.cpp
    ../colorsfilters/kis_multichannel_utils.cpp
    ../colorsfilters/virtual_channel_info.cpp
    )
kis_add_library(kritalevelfilter MODULE ${kritalevelfilter_SOURCES})
target_link_libraries(kritalevelfilter kritaimage)
install(TARGETS kritalevelfilter  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
