kis_add_test(
    kis_tiff_test.cpp
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(tiff_validation_probe tiff_validation_probe.cpp)
target_link_libraries(tiff_validation_probe PRIVATE pkport pkvariant ${TIFF_LIBRARIES})
target_include_directories(tiff_validation_probe SYSTEM PRIVATE ${TIFF_INCLUDE_DIR})
add_test(NAME tiff_validation_probe COMMAND tiff_validation_probe)
