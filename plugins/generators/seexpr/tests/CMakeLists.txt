kis_add_tests(
    kis_seexpr_generator_test.cpp

    LINK_LIBRARIES kritaimage kritatestsdk pktest pkuuid
)

target_link_libraries(kis_seexpr_generator_test
    -Wl,--whole-archive
    kritaseexprgenerator
    kritalcmsengine
    -Wl,--no-whole-archive)
