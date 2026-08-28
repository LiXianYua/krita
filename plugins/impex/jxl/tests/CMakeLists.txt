kis_add_test(
    kis_jpegxl_test.cpp
    TEST_NAME kis_jpegxl_test
    LINK_LIBRARIES kritametadata kritaimpex kritatestsdk
)

add_executable(jxl_validation_probe jxl_validation_probe.cpp)
add_test(NAME jxl_validation_probe COMMAND jxl_validation_probe)
