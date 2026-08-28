add_executable(krita_multigrid_generator_test
    KisMultigridPatternGeneratorTest.cpp
    ../multigridpatternhelpers.cpp)
set_target_properties(krita_multigrid_generator_test PROPERTIES AUTOMOC OFF)
target_link_libraries(krita_multigrid_generator_test PRIVATE
    pkcontainer pkgeometry pkstring pkxml)
add_test(NAME plugins-generators-multigrid-geometry-config-regression
         COMMAND krita_multigrid_generator_test)
