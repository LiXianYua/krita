kis_add_test(
    kis_node_dummies_graph_test.cpp
    ../../../../sdk/tests/testutil.cpp
    TEST_NAME KisNodeDummiesGraphTest
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritashapemodel kritatestsdk
)

kis_add_test(
    kis_node_shapes_graph_test.cpp
    ../../../../sdk/tests/testutil.cpp
    TEST_NAME KisNodeShapesGraphTest
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritashapemodel kritatestsdk
)

kis_add_test(
    kis_shape_controller_test.cpp
    kis_dummies_facade_base_test.cpp
    TEST_NAME kis_shape_controller_test
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritashapemodel kritatestsdk
)

kis_add_test(
    kis_dummies_facade_test.cpp
    kis_dummies_facade_base_test.cpp
    ../../../../sdk/tests/testutil.cpp
    TEST_NAME kis_dummies_facade_test
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritashapemodel kritatestsdk
)

kis_add_tests(
    KisShapeModelTest.cpp
    kis_image_view_converter_test.cpp
    kis_shape_commands_test.cpp
    kis_shape_layer_test.cpp
    kis_shape_selection_test.cpp
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritashapemodel kritaimpex kritatestsdk
)

kis_add_test(
    PkImageRasterBackendTest.cpp
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritaflake kritatestsdk
)

kis_add_test(PkSvgPainterBackendTest.cpp
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES pkrender kritaflake_imagecodec Qt${QT_MAJOR_VERSION}::Svg Qt${QT_MAJOR_VERSION}::Test
)

kis_add_test(PkReferenceImageLoaderTest.cpp
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritaflake_imagecodec Qt${QT_MAJOR_VERSION}::Gui Qt${QT_MAJOR_VERSION}::Test
)
