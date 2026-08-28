kis_add_test(
    KisTgaTest.cpp
    TEST_NAME KisTgaTest
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(tga_validation_probe tga_validation_probe.cpp ../tga_validation.cpp)
target_include_directories(tga_validation_probe PRIVATE ${CMAKE_SOURCE_DIR}/pk/global)
add_test(NAME tga_validation_probe COMMAND tga_validation_probe)
