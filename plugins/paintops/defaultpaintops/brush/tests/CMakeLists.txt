# on macOS cannot link to a MH_LIBRARY, see bug 417391
kis_add_test(KisDabRenderingQueueTest.cpp
    TEST_NAME KisDabRenderingQueueTest
    LINK_LIBRARIES kritadefaultpaintops_static kritalibpaintop kritaimage kritatestsdk
    )
target_include_directories(KisDabRenderingQueueTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../duplicate")


krita_add_broken_unit_test(kis_brushop_test.cpp ../../../../../sdk/tests/stroke_testing_utils.cpp
    TEST_NAME KisBrushOpTest
    LINK_LIBRARIES kritaimage kritalibpaintop kritatestsdk
    )
