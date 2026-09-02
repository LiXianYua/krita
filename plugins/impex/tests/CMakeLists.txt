add_executable(kis_impex_registration_once_test kis_impex_registration_once_test.cpp)
add_test(NAME kis_impex_registration_once_test COMMAND kis_impex_registration_once_test)

add_executable(kis_qimageio_capabilities_test kis_qimageio_capabilities_test.cpp ../qimageio/kis_qimageio_export.cpp)
target_link_libraries(kis_qimageio_capabilities_test PRIVATE kritaimpex)
add_test(NAME kis_qimageio_capabilities_test COMMAND kis_qimageio_capabilities_test)

set(kis_impex_registration_targets
    kritabrushexport kritabrushimport kritacsvexport kritacsvimport kritaexrexport kritaexrimport
    kritagifexport kritagifimport kritaheifexport kritaheifimport kritaheightmapexport kritaheightmapimport
    kritajp2import kritajpegexport kritajpegimport kritajxlexport kritajxlimport kritakraexport kritakraimport
    kritakrzexport kritaoraexport kritaoraimport kritapdfimport kritapngexport kritapngimport kritapsdexport
    kritapsdimport kritaqimageioexport kritaqimageioimport kritaqmlexport krita_raw_import kritargbeexport
    kritargbeimport kritaspriterexport kritasvgimport kritatgaexport kritatgaimport kritatiffexport
    kritatiffimport kritawebpexport kritawebpimport kritaxcfimport)
set(kis_impex_registration_existing_targets)
foreach(target IN LISTS kis_impex_registration_targets)
    if(TARGET ${target})
        list(APPEND kis_impex_registration_existing_targets ${target})
    endif()
endforeach()

# The oracle covers the complete codec set. Optional codec configurations omit
# this test while retaining successful CMake generation and the focused tests.
list(LENGTH kis_impex_registration_targets kis_impex_registration_target_count)
list(LENGTH kis_impex_registration_existing_targets kis_impex_registration_existing_count)
if(kis_impex_registration_target_count EQUAL kis_impex_registration_existing_count)
    add_executable(kis_impex_static_registration_test kis_impex_static_registration_test.cpp)
    target_link_libraries(kis_impex_static_registration_test PRIVATE kritaimpex)
    foreach(target IN LISTS kis_impex_registration_existing_targets)
        target_link_libraries(kis_impex_static_registration_test PRIVATE "$<TARGET_FILE:${target}>")
    endforeach()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(kis_impex_static_registration_test PRIVATE -Wl,--no-as-needed -Wl,--as-needed)
    endif()
    add_dependencies(kis_impex_static_registration_test ${kis_impex_registration_existing_targets})
    add_test(NAME kis_impex_static_registration_test COMMAND kis_impex_static_registration_test)
endif()
