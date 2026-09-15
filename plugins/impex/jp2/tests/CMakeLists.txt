pk_add_test(KisJP2Test
    REGISTER_FILTERS
        registerjp2ImportFilter
    LINK_LIBRARIES
        kritaimpex
        kritajp2import
)

add_executable(jp2_validation_probe jp2_validation_probe.cpp ../jp2_validation.cpp)
target_link_libraries(jp2_validation_probe PRIVATE OpenJPEG::openjp2)
add_test(NAME jp2_validation_probe COMMAND jp2_validation_probe)
