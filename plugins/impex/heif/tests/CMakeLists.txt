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

add_executable(heif_validation_probe heif_validation_probe.cpp)
target_link_libraries(heif_validation_probe PRIVATE HEIF::heif)
add_test(NAME heif_validation_probe COMMAND heif_validation_probe)
