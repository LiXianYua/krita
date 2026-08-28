kis_add_test(
    kis_heightmap_test.cpp
    TEST_NAME kis_heightmap_test
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(heightmap_validation_probe
    heightmap_validation_probe.cpp
    ../kis_heightmap_utils.cpp)
target_link_libraries(heightmap_validation_probe PRIVATE kritapigment)
add_test(NAME heightmap_validation_probe COMMAND heightmap_validation_probe)
