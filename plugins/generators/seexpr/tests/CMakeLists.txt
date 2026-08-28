kis_add_tests(
    kis_seexpr_generator_test.cpp

    LINK_LIBRARIES kritaimage kritatestsdk pktest pkuuid
)

target_compile_definitions(kis_seexpr_generator_test PRIVATE
    SEEXPR_MODULE_PATH="$<TARGET_FILE:kritaseexprgenerator>")
target_link_libraries(kis_seexpr_generator_test ${CMAKE_DL_LIBS})
add_dependencies(kis_seexpr_generator_test kritaseexprgenerator)
