pk_add_test(
    kis_rgbe_test
    REGISTER_FILTERS
        registerRGBEImportFilter
        registerRGBEExportFilter
    LINK_LIBRARIES
        kritametadata kritaimpex kritargbeimportutils
        kritargbeimport kritargbeexport
)

add_executable(rgbe_codec_probe rgbe_codec_probe.cpp ../rgbe_codec.cpp)
target_include_directories(rgbe_codec_probe PRIVATE ..)
add_test(NAME rgbe_codec_probe COMMAND rgbe_codec_probe)
