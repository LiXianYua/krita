add_executable(krita_simplexnoise_seed_test
    SimplexNoiseSeedTest.cpp
    ../simplexnoiseseed.cpp
)
set_target_properties(krita_simplexnoise_seed_test PROPERTIES AUTOMOC OFF)
target_include_directories(krita_simplexnoise_seed_test PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/resources
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(krita_simplexnoise_seed_test PRIVATE
    kritaresources pkstring pkvariant)
add_test(NAME plugins-generators-simplexnoise-seed-regression
         COMMAND krita_simplexnoise_seed_test)
