set(kritaconvolutionfilters_SOURCES
   convolutionfilters.cpp
   kis_convolution_filter.cpp )
kis_add_library(kritaconvolutionfilters STATIC ${kritaconvolutionfilters_SOURCES})
target_link_libraries(kritaconvolutionfilters kritaimage)
install(TARGETS kritaconvolutionfilters  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
