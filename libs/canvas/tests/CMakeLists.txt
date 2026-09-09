kis_add_tests(
    KisAsyncColorSamplerHelperTest.cpp
    kis_aspect_ratio_locker_test.cpp
    kis_coordinates_converter_test.cpp
    kis_grid_config_test.cpp
    kis_multi_surface_policy_test.cpp
    kis_opengl_policy_test.cpp
    kis_prescaled_projection_test.cpp
    kis_selection_options_test.cpp
    kis_selection_tool_factory_test.cpp
    kis_stabilized_events_sampler_test.cpp
    zoomhandler_test.cpp
    LINK_LIBRARIES kritacanvas kritatestsdk
)

kis_add_test(
    fill_processing_visitor_test.cpp
    TEST_NAME FillProcessingVisitorTest
    NAME_PREFIX libs-canvas-
    LINK_LIBRARIES kritacanvas kritatestsdk pkimageio kritalcmsengine
)

find_program(GETTEXT_XGETTEXT_EXECUTABLE NAMES xgettext REQUIRED)
add_test(NAME libs-canvas-host-action-extraction
    COMMAND ${CMAKE_COMMAND}
        -DHOST_FACTORY_SOURCE=${CMAKE_SOURCE_DIR}/libs/flake/KoToolFactoryBase.cpp
        -DCANVAS_FACTORY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../tool/KisToolPaintFactoryBase.cpp
        -DXGETTEXT_EXECUTABLE=${GETTEXT_XGETTEXT_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_host_action_extraction.cmake)

add_test(NAME libs-canvas-selection-factory-host-boundary
    COMMAND ${CMAKE_COMMAND}
        -DSELECTION_FACTORY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../tool/KisSelectionToolFactoryBase.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_selection_factory_host_boundary.cmake)
