pk_add_test(
    KisGifTest
    REGISTER_FILTERS
        registerKisGIFImportFilter
        registerKisGIFExportFilter
    LINK_LIBRARIES
        kritaimpex
        kritagifimport kritagifexport
    )

add_executable(gif_codec_probe gif_codec_probe.cpp ../qgiflibhandler.cpp)
target_link_libraries(gif_codec_probe PRIVATE kritastore pkimage GIF::GIF)
target_include_directories(gif_codec_probe PRIVATE ${CMAKE_SOURCE_DIR}/libs/pigment)
add_test(NAME gif_codec_probe COMMAND gif_codec_probe)

add_executable(gif_import_probe
    gif_import_probe.cpp
    ../kis_gif_import.cpp
    ../qgiflibhandler.cpp)
target_link_libraries(gif_import_probe PRIVATE kritaimpex GIF::GIF)
add_test(NAME gif_import_probe COMMAND gif_import_probe)
