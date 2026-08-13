kis_add_tests(
    TestPerspectiveBasedAssistantHelper.cpp
    TestAssistants.cpp
    LINK_LIBRARIES kritaassistanttool_static kritatestsdk
    )

kis_add_test(
    TestPaintingAssistantModel.cpp
    TEST_NAME TestPaintingAssistantModel
    LINK_LIBRARIES kritaassistanttool_static Qt${QT_MAJOR_VERSION}::Test
)
