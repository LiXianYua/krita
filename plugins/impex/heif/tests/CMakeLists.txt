if(APPLE)
    krita_add_broken_unit_test(
        KisHeifTest.cpp
        TEST_NAME KisHeifTest
        LINK_LIBRARIES kritaimpex kritatestsdk
    )

else(APPLE)

    kis_add_test(
        KisHeifTest.cpp
        TEST_NAME KisHeifTest
        LINK_LIBRARIES kritaimpex kritatestsdk
    )


endif()
