kis_add_test(
    kis_webp_test.cpp
    TEST_NAME kis_webp_test
    LINK_LIBRARIES kritametadata kritaimpex kritatestsdk
)

add_executable(webp_validation_probe webp_validation_probe.cpp ../webp_validation.cpp)
add_test(NAME webp_validation_probe COMMAND webp_validation_probe)
