pk_add_test(
    kis_heightmap_test
    REGISTER_FILTERS
        registerKisHeightMapImportFilter
        registerKisHeightMapExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaheightmapimport kritaheightmapexport
        kritalcmsengine
    )

add_executable(heightmap_validation_probe
    heightmap_validation_probe.cpp
    ../kis_heightmap_utils.cpp)
target_link_libraries(heightmap_validation_probe PRIVATE kritapigment)
add_test(NAME heightmap_validation_probe COMMAND heightmap_validation_probe)
