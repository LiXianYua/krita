add_executable(kis_impex_static_registration_test
    kis_impex_static_registration_test.cpp
)

add_executable(kis_impex_registration_once_test
    kis_impex_registration_once_test.cpp
)

add_test(NAME kis_impex_registration_once_test COMMAND kis_impex_registration_once_test)

add_executable(kis_qimageio_capabilities_test
    kis_qimageio_capabilities_test.cpp
    ../qimageio/kis_qimageio_export.cpp
)

target_link_libraries(kis_qimageio_capabilities_test PRIVATE kritaimpex)

add_test(NAME kis_qimageio_capabilities_test COMMAND kis_qimageio_capabilities_test)

target_link_libraries(kis_impex_static_registration_test PRIVATE
    kritaimpex
    -Wl,--no-as-needed
    "$<TARGET_FILE:kritabrushexport>"
    "$<TARGET_FILE:kritabrushimport>"
    "$<TARGET_FILE:kritacsvexport>"
    "$<TARGET_FILE:kritacsvimport>"
    "$<TARGET_FILE:kritaexrexport>"
    "$<TARGET_FILE:kritaexrimport>"
    "$<TARGET_FILE:kritagifexport>"
    "$<TARGET_FILE:kritagifimport>"
    "$<TARGET_FILE:kritaheifexport>"
    "$<TARGET_FILE:kritaheifimport>"
    "$<TARGET_FILE:kritaheightmapexport>"
    "$<TARGET_FILE:kritaheightmapimport>"
    "$<TARGET_FILE:kritajp2import>"
    "$<TARGET_FILE:kritajpegexport>"
    "$<TARGET_FILE:kritajpegimport>"
    "$<TARGET_FILE:kritajxlexport>"
    "$<TARGET_FILE:kritajxlimport>"
    "$<TARGET_FILE:kritakraexport>"
    "$<TARGET_FILE:kritakraimport>"
    "$<TARGET_FILE:kritakrzexport>"
    "$<TARGET_FILE:kritaoraexport>"
    "$<TARGET_FILE:kritaoraimport>"
    "$<TARGET_FILE:kritapdfimport>"
    "$<TARGET_FILE:kritapngexport>"
    "$<TARGET_FILE:kritapngimport>"
    "$<TARGET_FILE:kritapsdexport>"
    "$<TARGET_FILE:kritapsdimport>"
    "$<TARGET_FILE:kritaqimageioexport>"
    "$<TARGET_FILE:kritaqimageioimport>"
    "$<TARGET_FILE:kritaqmlexport>"
    "$<TARGET_FILE:krita_raw_import>"
    "$<TARGET_FILE:kritargbeexport>"
    "$<TARGET_FILE:kritargbeimport>"
    "$<TARGET_FILE:kritaspriterexport>"
    "$<TARGET_FILE:kritasvgimport>"
    "$<TARGET_FILE:kritatgaexport>"
    "$<TARGET_FILE:kritatgaimport>"
    "$<TARGET_FILE:kritatiffexport>"
    "$<TARGET_FILE:kritatiffimport>"
    "$<TARGET_FILE:kritawebpexport>"
    "$<TARGET_FILE:kritawebpimport>"
    "$<TARGET_FILE:kritaxcfimport>"
    -Wl,--as-needed
)

add_dependencies(kis_impex_static_registration_test
    kritabrushexport kritabrushimport kritacsvexport kritacsvimport
    kritaexrexport kritaexrimport kritagifexport kritagifimport
    kritaheifexport kritaheifimport kritaheightmapexport kritaheightmapimport
    kritajp2import kritajpegexport kritajpegimport kritajxlexport kritajxlimport
    kritakraexport kritakraimport kritakrzexport kritaoraexport kritaoraimport
    kritapdfimport kritapngexport kritapngimport kritapsdexport kritapsdimport
    kritaqimageioexport kritaqimageioimport kritaqmlexport krita_raw_import
    kritargbeexport kritargbeimport kritaspriterexport kritasvgimport
    kritatgaexport kritatgaimport kritatiffexport kritatiffimport
    kritawebpexport kritawebpimport kritaxcfimport
)

add_test(NAME kis_impex_static_registration_test COMMAND kis_impex_static_registration_test)
