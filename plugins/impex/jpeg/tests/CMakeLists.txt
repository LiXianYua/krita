kis_add_test(
    kis_jpeg_test.cpp
    TEST_NAME kis_jpeg_test
    LINK_LIBRARIES kritametadata kritaimpex kritatestsdk
    )

add_executable(jpeg_validation_probe jpeg_validation_probe.cpp)
add_test(NAME jpeg_validation_probe COMMAND jpeg_validation_probe)
