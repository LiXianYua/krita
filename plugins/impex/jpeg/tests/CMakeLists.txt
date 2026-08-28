kis_add_test(
    kis_jpeg_test.cpp
    TEST_NAME kis_jpeg_test
    LINK_LIBRARIES kritametadata kritaimpex kritatestsdk
    )

add_executable(jpeg_validation_probe
    jpeg_validation_probe.cpp
    ../kis_jpeg_source.cpp
    ../kis_jpeg_destination.cpp
)
target_link_libraries(jpeg_validation_probe PRIVATE pkport pkvariant ${JPEG_LIBRARIES})
target_include_directories(jpeg_validation_probe SYSTEM PRIVATE ${JPEG_INCLUDE_DIR})
add_test(NAME jpeg_validation_probe COMMAND jpeg_validation_probe)
