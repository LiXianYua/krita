pk_add_test(
    kis_webp_test
    REGISTER_FILTERS
        registerKisWebPImportFilter
        registerKisWebPExportFilter
    LINK_LIBRARIES
        kritametadata kritaimpex
        kritawebpimport kritawebpexport
)

add_executable(webp_validation_probe webp_validation_probe.cpp ../webp_validation.cpp)
add_test(NAME webp_validation_probe COMMAND webp_validation_probe)

# R-61 Task 1：p709SRGBProfile() 返空（宿主未配 EXTRA_RESOURCE_DIRS / 未注册色彩
# 引擎）时 kis_webp_export.cpp:579 裸解引用的常驻回归载体。形态照 R-59 的
# libs/impex/tests/KisPngCodecNoResourceDirsTest.cpp：plain main()、零 Qt 头。
kis_add_test(
    KisWebPExportNoResourceDirsTest.cpp
    LINK_LIBRARIES kritawebpexport kritaimpex kritaimage kritalcmsengine
)
target_include_directories(KisWebPExportNoResourceDirsTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)
