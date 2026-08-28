kis_add_test(KisJP2Test.cpp
    TEST_NAME KisJP2Test
    LINK_LIBRARIES kritaimpex kritatestsdk
)

add_executable(jp2_validation_probe jp2_validation_probe.cpp ../jp2_validation.cpp)
target_link_libraries(jp2_validation_probe PRIVATE OpenJPEG::openjp2)
add_test(NAME jp2_validation_probe COMMAND jp2_validation_probe)
