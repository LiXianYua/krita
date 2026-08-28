##### Tests that currently fail and should be fixed #####

# Works under Linux but does not work under Windows
krita_add_broken_unit_test( kis_exif_test.cpp
    TEST_NAME KisExifTest
    LINK_LIBRARIES kritametadata kritapigment kritaimage kritalibbrush kritaimpex kritatestsdk
    )

add_executable(kis_metadata_regression_test kis_metadata_regression_test.cpp)
target_link_libraries(kis_metadata_regression_test PRIVATE kritaexifcommon)
target_link_libraries(kis_metadata_regression_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_definitions(kis_metadata_regression_test PRIVATE
    METADATA_EXIF_MODULE="$<TARGET_FILE:kritaexif>"
    METADATA_IPTC_MODULE="$<TARGET_FILE:kritaiptc>"
    METADATA_XMP_MODULE="$<TARGET_FILE:kritaxmp>")
add_dependencies(kis_metadata_regression_test kritaexif kritaiptc kritaxmp)
add_test(NAME plugins-metadata-kis_metadata-path-regression
         COMMAND kis_metadata_regression_test path)
add_test(NAME plugins-metadata-kis_metadata-date-regression
         COMMAND kis_metadata_regression_test date)
add_test(NAME plugins-metadata-kis_metadata-registry-regression
         COMMAND kis_metadata_regression_test registry)
add_test(NAME plugins-metadata-kis_metadata-exif-oecf-regression
         COMMAND kis_metadata_regression_test exif-oecf)
add_test(NAME plugins-metadata-kis_metadata-xmp-unicode-regression
         COMMAND kis_metadata_regression_test xmp-unicode)
add_test(NAME plugins-metadata-kis_metadata-xmp-array-index-regression
         COMMAND kis_metadata_regression_test xmp-array-index)
