kis_add_test(
    kis_input_manager_test.cpp
    TEST_NAME KisInputManagerTest
    LINK_LIBRARIES kritainput kritatestsdk
)

kis_add_test(
    kis_input_transform_policy_test.cpp
    TEST_NAME KisInputTransformPolicyTest
    LINK_LIBRARIES kritainput kritatestsdk
)
