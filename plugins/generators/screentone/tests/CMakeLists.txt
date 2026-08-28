kis_add_tests(
    KisScreentoneGeneratorTest.cpp

    LINK_LIBRARIES kritaimage kritatestsdk pktest pkuuid
)

target_compile_definitions(KisScreentoneGeneratorTest PRIVATE
    SCREENTONE_MODULE_PATH="$<TARGET_FILE:kritascreentonegenerator>")
target_link_libraries(KisScreentoneGeneratorTest ${CMAKE_DL_LIBS})
add_dependencies(KisScreentoneGeneratorTest kritascreentonegenerator)
