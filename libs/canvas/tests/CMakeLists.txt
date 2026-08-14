kis_add_tests(
    KisAsyncColorSamplerHelperTest.cpp
    kis_coordinates_converter_test.cpp
    kis_grid_config_test.cpp
    kis_multi_surface_policy_test.cpp
    kis_opengl_policy_test.cpp
    kis_prescaled_projection_test.cpp
    kis_selection_options_test.cpp
    kis_stabilized_events_sampler_test.cpp
    LINK_LIBRARIES kritacanvas kritatestsdk
)

krita_add_broken_unit_test(
    fill_processing_visitor_test.cpp
    $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME FillProcessingVisitorTest
    NAME_PREFIX libs-canvas-
    LINK_LIBRARIES kritacanvas kritatestsdk
)
