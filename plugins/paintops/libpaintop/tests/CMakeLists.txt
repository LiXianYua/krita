########### next target ###############


kis_add_tests(KisCurveOptionDataTest.cpp
    LINK_LIBRARIES kritaimage kritalibpaintop kritatestsdk)

kis_add_tests(kis_linked_pattern_manager_test.cpp
    LINK_LIBRARIES kritaimage kritalibpaintop kritatestsdk)

krita_add_broken_unit_test(
    freehand_stroke_test.cpp
    $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME FreehandStrokeTest
    NAME_PREFIX plugins-paintops-libpaintop-
    LINK_LIBRARIES kritalibpaintop kritacanvas kritatestsdk
)

krita_add_broken_unit_test(
    FreehandStrokeBenchmark.cpp
    $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME FreehandStrokeBenchmark
    NAME_PREFIX plugins-paintops-libpaintop-
    LINK_LIBRARIES kritalibpaintop kritacanvas kritatestsdk
)

krita_add_broken_unit_test(
    KisPaintOnTransparencyMaskTest.cpp
    $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME KisPaintOnTransparencyMaskTest
    NAME_PREFIX plugins-paintops-libpaintop-
    LINK_LIBRARIES kritalibpaintop kritacanvas kritatestsdk
)
