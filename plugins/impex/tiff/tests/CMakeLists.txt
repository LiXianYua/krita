kis_add_test(
    kis_tiff_test.cpp
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(tiff_validation_probe tiff_validation_probe.cpp)
add_test(NAME tiff_validation_probe COMMAND tiff_validation_probe)
