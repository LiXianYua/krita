kis_add_tests(
    KisAsyncColorSamplerHelperTest.cpp
    kis_aspect_ratio_locker_test.cpp
    kis_coordinates_converter_test.cpp
    kis_grid_config_test.cpp
    kis_multi_surface_policy_test.cpp
    kis_opengl_policy_test.cpp
    kis_prescaled_projection_test.cpp
    kis_selection_options_test.cpp
    kis_stabilized_events_sampler_test.cpp
    fill_processing_visitor_test.cpp
    zoomhandler_test.cpp
    LINK_LIBRARIES kritacanvas kritatestsdk
)

target_link_libraries(KisAsyncColorSamplerHelperTest kritaselectiontools)
target_include_directories(KisAsyncColorSamplerHelperTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/tools/selectiontools)
