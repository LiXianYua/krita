add_subdirectory(gradient)
add_subdirectory(solid)
add_subdirectory(pattern)
add_subdirectory(simplexnoise)
add_subdirectory(screentone)
add_subdirectory(multigridpattern)
if(KSeExpr_FOUND)
  add_subdirectory(seexpr)
endif()

if(TARGET kritaseexprgenerator)
    add_executable(krita_generator_module_registration_test
        screentone/tests/GeneratorModuleRegistrationTest.cpp)
    set_target_properties(krita_generator_module_registration_test PROPERTIES AUTOMOC OFF)
    target_compile_definitions(krita_generator_module_registration_test PRIVATE
        GENERATOR_GRADIENT_MODULE="$<TARGET_FILE:kritagradientgenerator>"
        GENERATOR_SOLID_MODULE="$<TARGET_FILE:kritacolorgenerator>"
        GENERATOR_PATTERN_MODULE="$<TARGET_FILE:kritapatterngenerator>"
        GENERATOR_SIMPLEX_MODULE="$<TARGET_FILE:kritasimplexnoisegenerator>"
        GENERATOR_SCREENTONE_MODULE="$<TARGET_FILE:kritascreentonegenerator>"
        GENERATOR_MULTIGRID_MODULE="$<TARGET_FILE:kritamultigridpatterngenerator>"
        GENERATOR_SEEXPR_MODULE="$<TARGET_FILE:kritaseexprgenerator>")
    target_link_libraries(krita_generator_module_registration_test PRIVATE
        kritaimage ${CMAKE_DL_LIBS})
    add_dependencies(krita_generator_module_registration_test
        kritagradientgenerator kritacolorgenerator kritapatterngenerator
        kritasimplexnoisegenerator kritascreentonegenerator
        kritamultigridpatterngenerator kritaseexprgenerator)
    add_test(NAME plugins-generators-module-registration-regression
             COMMAND krita_generator_module_registration_test)
endif()
